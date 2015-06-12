// Copyright 2012 Rui Ueyama. Released under the MIT license.

/*
 * This implements Dave Prosser's C Preprocessing algorithm, described
 * in this document: https://github.com/rui314/8cc/wiki/cpp.algo.pdf
 */

#include <ctype.h>
#include <libgen.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "8cc.h"

#define MAP_NAME    once_map
#define VALUE_T     void*
#include "generic_map.h"
#include "generic_map.c"
#undef MAP_NAME
#undef VALUE_T

static once_map *once = &empty_map(once_map);
static void *once_map_token = &once;

#define MAP_NAME    keyword_map
#define VALUE_T     int*
#include "generic_map.h"
#include "generic_map.c"
#undef MAP_NAME
#undef VALUE_T

static keyword_map *keywords = &empty_map(keyword_map);

#define MAP_NAME    guard_map
#define VALUE_T     char*
#include "generic_map.h"
#include "generic_map.c"
#undef MAP_NAME
#undef VALUE_T

static guard_map *include_guard = &empty_map(guard_map);

#define VEC_NAME    path_vec
#define VALUE_T     char*
#include "generic_vec.h"
#include "generic_vec.c"
#undef VEC_NAME
#undef VALUE_T

static path_vec *std_include_path = &empty_vector(path_vec);

static struct tm now;
static Token *cpp_token_zero = &(Token){ .kind = TNUMBER, .sval = "0" };
static Token *cpp_token_one = &(Token){ .kind = TNUMBER, .sval = "1" };

typedef void SpecialMacroHandler(Token *tok);
typedef enum { IN_THEN, IN_ELIF, IN_ELSE } CondInclCtx;
typedef enum { MACRO_OBJ, MACRO_FUNC, MACRO_SPECIAL } MacroType;

typedef struct {
    CondInclCtx ctx;
    char *include_guard;
    File *file;
    bool wastrue;
} CondIncl;

#define VEC_NAME    cond_incl_vec
#define VALUE_T     CondIncl*
#include "generic_vec.h"
#include "generic_vec.c"
#undef VEC_NAME
#undef VALUE_T

static cond_incl_vec *cond_incl_stack = &empty_vector(cond_incl_vec);

#define MAP_NAME    token_map
#define VALUE_T     Token*
#include "generic_map.h"
#include "generic_map.c"
#undef MAP_NAME
#undef VALUE_T

#define VEC_NAME    token_vec
#define VALUE_T     Token*
#include "generic_vec.h"
#include "generic_vec.c"
#undef VEC_NAME
#undef VALUE_T

#define VEC_NAME    arg_vec
#define VALUE_T     token_vec*
#include "generic_vec.h"
#include "generic_vec.c"
#undef VEC_NAME
#undef VALUE_T

typedef struct {
    MacroType kind;
    int nargs;
    token_vec *body;
    bool is_varg;
    SpecialMacroHandler *fn;
} Macro;

#define MAP_NAME    macro_map
#define VALUE_T     Macro*
#include "generic_map.h"
#include "generic_map.c"
#undef MAP_NAME
#undef VALUE_T

static macro_map *macros = &empty_map(macro_map);

static Macro *make_obj_macro(token_vec *body);
static Macro *make_func_macro(token_vec *body, int nargs, bool is_varg);
static Macro *make_special_macro(SpecialMacroHandler *fn);
static void define_obj_macro(char *name, Token *value);
static void read_directive(Token *hash);
static Token *read_expand(void);

/*
 * Constructors
 */

static CondIncl *make_cond_incl(bool wastrue) {
    CondIncl *r = malloc(sizeof(CondIncl));
    r->ctx = IN_THEN;
    r->wastrue = wastrue;
    return r;
}

static Macro *make_macro(Macro *tmpl) {
    Macro *r = malloc(sizeof(Macro));
    *r = *tmpl;
    return r;
}

static Macro *make_obj_macro(token_vec *body) {
    return make_macro(&(Macro){ MACRO_OBJ, .body = body });
}

static Macro *make_func_macro(token_vec *body, int nargs, bool is_varg) {
    return make_macro(&(Macro){
            MACRO_FUNC, .nargs = nargs, .body = body, .is_varg = is_varg });
}

static Macro *make_special_macro(SpecialMacroHandler *fn) {
    return make_macro(&(Macro){ MACRO_SPECIAL, .fn = fn });
}

static Token *make_macro_token(int position, bool is_vararg) {
    Token *r = malloc(sizeof(Token));
    r->kind = TMACRO_PARAM;
    r->is_vararg = is_vararg;
    r->hideset = NULL;
    r->position = position;
    r->space = false;
    r->bol = false;
    return r;
}

static Token *copy_token(Token *tok) {
    Token *r = malloc(sizeof(Token));
    *r = *tok;
    return r;
}

static void expect(char id) {
    Token *tok = lex();
    if (!is_keyword(tok, id))
        errort(tok, "%c expected, but got %s", id, tok2s(tok));
}

/*
 * Utility functions
 */

bool is_ident(Token *tok, char *s) {
    return tok->kind == TIDENT && !strcmp(tok->sval, s);
}

static bool next(int id) {
    Token *tok = lex();
    if (is_keyword(tok, id))
        return true;
    unget_token(tok);
    return false;
}

static void propagate_space(token_vec *tokens, Token *tmpl) {
    if (token_vec_len(tokens) == 0)
        return;
    Token *tok = copy_token(token_vec_head(tokens));
    tok->space = tmpl->space;
    token_vec_set(tokens, 0, tok);
}

/*
 * Macro expander
 */

static Token *read_ident() {
    Token *tok = lex();
    if (tok->kind != TIDENT)
        errort(tok, "identifier expected, but got %s", tok2s(tok));
    return tok;
}

void expect_newline() {
    Token *tok = lex();
    if (tok->kind != TNEWLINE)
        errort(tok, "newline expected, but got %s", tok2s(tok));
}

static token_vec *read_one_arg(Token *ident, bool *end, bool readall) {
    token_vec *r = make_token_vec();
    int level = 0;
    for (;;) {
        Token *tok = lex();
        if (tok->kind == TEOF)
            errort(ident, "unterminated macro argument list");
        if (tok->kind == TNEWLINE)
            continue;
        if (tok->bol && is_keyword(tok, '#')) {
            read_directive(tok);
            continue;
        }
        if (level == 0 && is_keyword(tok, ')')) {
            unget_token(tok);
            *end = true;
            return r;
        }
        if (level == 0 && is_keyword(tok, ',') && !readall)
            return r;
        if (is_keyword(tok, '('))
            level++;
        if (is_keyword(tok, ')'))
            level--;
        // C11 6.10.3p10: Within the macro argument list,
        // newline is considered a normal whitespace character.
        // I don't know why the standard specifies such a minor detail,
        // but the difference of newline and space is observable
        // if you stringize tokens using #.
        if (tok->bol) {
            tok = copy_token(tok);
            tok->bol = false;
            tok->space = true;
        }
        token_vec_push(r, tok);
    }
}

static arg_vec *do_read_args(Token *ident, Macro *macro) {
    arg_vec *r = make_arg_vec();
    bool end = false;
    while (!end) {
        bool in_ellipsis = (macro->is_varg &&
                            arg_vec_len(r) + 1 == macro->nargs);
        arg_vec_push(r, read_one_arg(ident, &end, in_ellipsis));
    }
    if (macro->is_varg && arg_vec_len(r) == macro->nargs - 1)
        arg_vec_push(r, make_token_vec());
    return r;
}

static arg_vec *read_args(Token *tok, Macro *macro) {
    if (macro->nargs == 0 && is_keyword(peek_token(), ')')) {
        // If a macro M has no parameter, argument list of M()
        // is an empty list. If it has one parameter,
        // argument list of M() is a list containing an empty list.
        return make_arg_vec();
    }
    arg_vec *args = do_read_args(tok, macro);
    if (arg_vec_len(args) != macro->nargs)
        errort(tok, "macro argument number does not match");
    return args;
}

static token_vec *add_hide_set(token_vec *tokens, Set *hideset) {
    token_vec *r = make_token_vec();
    for (int i = 0; i < token_vec_len(tokens); i++) {
        Token *t = copy_token(token_vec_get(tokens, i));
        t->hideset = set_union(t->hideset, hideset);
        token_vec_push(r, t);
    }
    return r;
}

static Token *glue_tokens(Token *t, Token *u) {
    Buffer *b = make_buffer();
    buf_printf(b, "%s", tok2s(t));
    buf_printf(b, "%s", tok2s(u));
    Token *r = lex_string(buf_body(b));
    return r;
}

static void glue_push(token_vec *tokens, Token *tok) {
    Token *last = token_vec_pop(tokens);
    token_vec_push(tokens, glue_tokens(last, tok));
}

static Token *stringize(Token *tmpl, token_vec *args) {
    Buffer *b = make_buffer();
    for (int i = 0; i < token_vec_len(args); i++) {
        Token *tok = token_vec_get(args, i);
        if (buf_len(b) && tok->space)
            buf_printf(b, " ");
        buf_printf(b, "%s", tok2s(tok));
    }
    buf_write(b, '\0');
    Token *r = copy_token(tmpl);
    r->kind = TSTRING;
    r->sval = buf_body(b);
    r->slen = buf_len(b);
    r->enc = ENC_NONE;
    return r;
}

static token_vec *expand_all(token_vec *tokens, Token *tmpl) {
    token_buffer_stash(token_vec_reverse(tokens));
    token_vec *r = make_token_vec();
    for (;;) {
        Token *tok = read_expand();
        if (tok->kind == TEOF)
            break;
        token_vec_push(r, tok);
    }
    propagate_space(r, tmpl);
    token_buffer_unstash();
    return r;
}

static token_vec *subst(Macro *macro, arg_vec *args, Set *hideset) {
    token_vec *r = make_token_vec();
    int len = token_vec_len(macro->body);
    for (int i = 0; i < len; i++) {
        Token *t0 = token_vec_get(macro->body, i);
        Token *t1 = (i == len - 1) ? NULL : token_vec_get(macro->body, i + 1);
        bool t0_param = (t0->kind == TMACRO_PARAM);
        bool t1_param = (t1 && t1->kind == TMACRO_PARAM);

        if (is_keyword(t0, '#') && t1_param) {
            token_vec_push(r, stringize(t0, arg_vec_get(args, t1->position)));
            i++;
            continue;
        }
        if (is_keyword(t0, KHASHHASH) && t1_param) {
            token_vec *arg = arg_vec_get(args, t1->position);
            // [GNU] [,##__VA_ARG__] is expanded to the empty token sequence
            // if __VA_ARG__ is empty. Otherwise it's expanded to
            // [,<tokens in __VA_ARG__>].
            if (t1->is_vararg && token_vec_len(r) > 0 && is_keyword(token_vec_tail(r), ',')) {
                if (token_vec_len(arg) > 0)
                    token_vec_append(r, arg);
                else
                    token_vec_pop(r);
            } else if (token_vec_len(arg) > 0) {
                glue_push(r, token_vec_head(arg));
                for (int i = 1; i < token_vec_len(arg); i++)
                    token_vec_push(r, token_vec_get(arg, i));
            }
            i++;
            continue;
        }
        if (is_keyword(t0, KHASHHASH) && t1) {
            hideset = t1->hideset;
            glue_push(r, t1);
            i++;
            continue;
        }
        if (t0_param && t1 && is_keyword(t1, KHASHHASH)) {
            hideset = t1->hideset;
            token_vec *arg = arg_vec_get(args, t0->position);
            if (token_vec_len(arg) == 0)
                i++;
            else
                token_vec_append(r, arg);
            continue;
        }
        if (t0_param) {
            token_vec *arg = arg_vec_get(args, t0->position);
            token_vec_append(r, expand_all(arg, t0));
            continue;
        }
        token_vec_push(r, t0);
    }
    return add_hide_set(r, hideset);
}

static void unget_all(token_vec *tokens) {
    for (int i = token_vec_len(tokens) - 1; i >= 0; i--)
        unget_token(token_vec_get(tokens, i));
}

// This is "expand" function in the Dave Prosser's document.
static Token *read_expand_newline() {
    Token *tok = lex();
    if (tok->kind != TIDENT)
        return tok;
    char *name = tok->sval;
    Macro *macro = macro_map_get(macros, name);
    if (!macro || set_has(tok->hideset, name))
        return tok;

    switch (macro->kind) {
    case MACRO_OBJ: {
        Set *hideset = set_add(tok->hideset, name);
        token_vec *tokens = subst(macro, NULL, hideset);
        propagate_space(tokens, tok);
        unget_all(tokens);
        return read_expand();
    }
    case MACRO_FUNC: {
        if (!next('('))
            return tok;
        arg_vec *args = read_args(tok, macro);
        Token *rparen = peek_token();
        expect(')');
        Set *hideset = set_add(set_intersection(tok->hideset, rparen->hideset), name);
        token_vec *tokens = subst(macro, args, hideset);
        propagate_space(tokens, tok);
        unget_all(tokens);
        return read_expand();
    }
    case MACRO_SPECIAL:
        macro->fn(tok);
        return read_expand();
    default:
        error("internal error");
    }
}

static Token *read_expand() {
    for (;;) {
        Token *tok = read_expand_newline();
        if (tok->kind != TNEWLINE)
            return tok;
    }
}

static bool read_funclike_macro_params(Token *name, token_map *param) {
    int pos = 0;
    for (;;) {
        Token *tok = lex();
        if (is_keyword(tok, ')'))
            return false;
        if (pos) {
            if (!is_keyword(tok, ','))
                errort(tok, ", expected, but got %s", tok2s(tok));
            tok = lex();
        }
        if (tok->kind == TNEWLINE)
            errort(name, "missing ')' in macro parameter list");
        if (is_keyword(tok, KELLIPSIS)) {
            token_map_put(param, "__VA_ARGS__", make_macro_token(pos++, true));
            expect(')');
            return true;
        }
        if (tok->kind != TIDENT)
            errort(tok, "identifier expected, but got %s", tok2s(tok));
        char *arg = tok->sval;
        if (next(KELLIPSIS)) {
            expect(')');
            token_map_put(param, arg, make_macro_token(pos++, true));
            return true;
        }
        token_map_put(param, arg, make_macro_token(pos++, false));
    }
}

static void hashhash_check(token_vec *v) {
    if (token_vec_len(v) == 0)
        return;
    if (is_keyword(token_vec_head(v), KHASHHASH))
        errort(token_vec_head(v),
               "'##' cannot appear at start of macro expansion");
    if (is_keyword(token_vec_tail(v), KHASHHASH))
        errort(token_vec_tail(v),
               "'##' cannot appear at end of macro expansion");
}

static token_vec *read_funclike_macro_body(token_map *param) {
    token_vec *r = make_token_vec();
    for (;;) {
        Token *tok = lex();
        if (tok->kind == TNEWLINE)
            return r;
        if (tok->kind == TIDENT) {
            Token *subst = token_map_get(param, tok->sval);
            if (subst) {
                subst = copy_token(subst);
                subst->space = tok->space;
                token_vec_push(r, subst);
                continue;
            }
        }
        token_vec_push(r, tok);
    }
}

static void read_funclike_macro(Token *name) {
    token_map *param = make_token_map();
    bool is_varg = read_funclike_macro_params(name, param);
    token_vec *body = read_funclike_macro_body(param);
    hashhash_check(body);
    Macro *macro = make_func_macro(body, token_map_len(param), is_varg);
    macro_map_put(macros, name->sval, macro);
}

static void read_obj_macro(char *name) {
    token_vec *body = make_token_vec();
    for (;;) {
        Token *tok = lex();
        if (tok->kind == TNEWLINE)
            break;
        token_vec_push(body, tok);
    }
    hashhash_check(body);
    macro_map_put(macros, name, make_obj_macro(body));
}

/*
 * #define
 */

static void read_define() {
    Token *name = read_ident();
    Token *tok = lex();
    if (is_keyword(tok, '(') && !tok->space) {
        read_funclike_macro(name);
        return;
    }
    unget_token(tok);
    read_obj_macro(name->sval);
}

/*
 * #undef
 */

static void read_undef() {
    Token *name = read_ident();
    expect_newline();
    macro_map_remove(macros, name->sval);
}

/*
 * #if and the like
 */

static Token *read_defined_op() {
    Token *tok = lex();
    if (is_keyword(tok, '(')) {
        tok = lex();
        expect(')');
    }
    if (tok->kind != TIDENT)
        errort(tok, "identifier expected, but got %s", tok2s(tok));
    return macro_map_get(macros, tok->sval) ? cpp_token_one : cpp_token_zero;
}

static token_vec *read_intexpr_line() {
    token_vec *r = make_token_vec();
    for (;;) {
        Token *tok = read_expand_newline();
        if (tok->kind == TNEWLINE)
            return r;
        if (is_ident(tok, "defined")) {
            token_vec_push(r, read_defined_op());
        } else if (tok->kind == TIDENT) {
            // C11 6.10.1.4 says that remaining identifiers
            // should be replaced with pp-number 0.
            token_vec_push(r, cpp_token_zero);
        } else {
            token_vec_push(r, tok);
        }
    }
}

static bool read_constexpr() {
    token_buffer_stash(token_vec_reverse(read_intexpr_line()));
    Node *expr = read_expr();
    Token *tok = lex();
    if (tok->kind != TEOF)
        errort(tok, "stray token: %s", tok2s(tok));
    token_buffer_unstash();
    return eval_intexpr(expr, NULL);
}

static void do_read_if(bool istrue) {
    cond_incl_vec_push(cond_incl_stack, make_cond_incl(istrue));
    if (!istrue)
        skip_cond_incl();
}

static void read_if() {
    do_read_if(read_constexpr());
}

static void read_ifdef() {
    Token *tok = lex();
    if (tok->kind != TIDENT)
        errort(tok, "identifier expected, but got %s", tok2s(tok));
    expect_newline();
    do_read_if(macro_map_get(macros, tok->sval));
}

static void read_ifndef() {
    Token *tok = lex();
    if (tok->kind != TIDENT)
        errort(tok, "identifier expected, but got %s", tok2s(tok));
    expect_newline();
    do_read_if(!macro_map_get(macros, tok->sval));
    if (tok->count == 2) {
        // "ifndef" is the second token in this file.
        // Prepare to detect an include guard.
        CondIncl *ci = cond_incl_vec_tail(cond_incl_stack);
        ci->include_guard = tok->sval;
        ci->file = tok->file;
    }
}

static void read_else(Token *hash) {
    if (cond_incl_vec_len(cond_incl_stack) == 0)
        errort(hash, "stray #else");
    CondIncl *ci = cond_incl_vec_tail(cond_incl_stack);
    if (ci->ctx == IN_ELSE)
        errort(hash, "#else appears in #else");
    expect_newline();
    ci->ctx = IN_ELSE;
    ci->include_guard = NULL;
    if (ci->wastrue)
        skip_cond_incl();
}

static void read_elif(Token *hash) {
    if (cond_incl_vec_len(cond_incl_stack) == 0)
        errort(hash, "stray #elif");
    CondIncl *ci = cond_incl_vec_tail(cond_incl_stack);
    if (ci->ctx == IN_ELSE)
        errort(hash, "#elif after #else");
    ci->ctx = IN_ELIF;
    ci->include_guard = NULL;
    if (ci->wastrue || !read_constexpr()) {
        skip_cond_incl();
        return;
    }
    ci->wastrue = true;
}

// Skips all newlines and returns the first non-newline token.
static Token *skip_newlines() {
    Token *tok = lex();
    while (tok->kind == TNEWLINE)
        tok = lex();
    unget_token(tok);
    return tok;
}

static void read_endif(Token *hash) {
    if (cond_incl_vec_len(cond_incl_stack) == 0)
        errort(hash, "stray #endif");
    CondIncl *ci = cond_incl_vec_pop(cond_incl_stack);
    expect_newline();

    // Detect an #ifndef and #endif pair that guards the entire
    // header file. Remember the macro name guarding the file
    // so that we can skip the file next time.
    if (!ci->include_guard || ci->file != hash->file)
        return;
    Token *last = skip_newlines();
    if (ci->file != last->file)
        guard_map_put(include_guard, ci->file->name, ci->include_guard);
}

/*
 * #error and #warning
 */

static char *read_error_message() {
    Buffer *b = make_buffer();
    for (;;) {
        Token *tok = lex();
        if (tok->kind == TNEWLINE)
            return buf_body(b);
        if (buf_len(b) != 0 && tok->space)
            buf_write(b, ' ');
        buf_printf(b, "%s", tok2s(tok));
    }
}

static void read_error(Token *hash) {
    errort(hash, "#error: %s", read_error_message());
}

static void read_warning(Token *hash) {
    warnt(hash, "#warning: %s", read_error_message());
}

/*
 * #include
 */

static char *join_paths(token_vec *args) {
    Buffer *b = make_buffer();
    for (int i = 0; i < token_vec_len(args); i++)
        buf_printf(b, "%s", tok2s(token_vec_get(args, i)));
    return buf_body(b);
}

static char *read_cpp_header_name(Token *hash, bool *std) {
    // Try reading a filename using a special tokenizer for #include.
    char *path = read_header_file_name(std);
    if (path)
        return path;

    // If a token following #include does not start with < nor ",
    // try to read the token as a regular token. Macro-expanded
    // form may be a valid header file path.
    Token *tok = read_expand_newline();
    if (tok->kind == TNEWLINE)
        errort(hash, "expected filename, but got newline");
    if (tok->kind == TSTRING) {
        *std = false;
        return tok->sval;
    }
    if (!is_keyword(tok, '<'))
        errort(tok, "< expected, but got %s", tok2s(tok));
    token_vec *tokens = make_token_vec();
    for (;;) {
        Token *tok = read_expand_newline();
        if (tok->kind == TNEWLINE)
            errort(hash, "premature end of header name");
        if (is_keyword(tok, '>'))
            break;
        token_vec_push(tokens, tok);
    }
    *std = true;
    return join_paths(tokens);
}

static bool guarded(char *path) {
    char *guard = guard_map_get(include_guard, path);
    bool r = (guard && macro_map_get(macros, guard));
    define_obj_macro("__8cc_include_guard", r ? cpp_token_one : cpp_token_zero);
    return r;
}

static bool try_include(char *dir, char *filename, bool isimport) {
    char *path = fullpath(format("%s/%s", dir, filename));
    if (once_map_get(once, path))
        return true;
    if (guarded(path))
        return true;
    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;
    if (isimport)
        once_map_put(once, path, once_map_token);
    stream_push(make_file(fp, path));
    return true;
}

static void read_include(Token *hash, File *file, bool isimport) {
    bool std;
    char *filename = read_cpp_header_name(hash, &std);
    expect_newline();
    if (filename[0] == '/') {
        if (try_include("/", filename, isimport))
            return;
        goto err;
    }
    if (!std) {
        char *dir = file->name ? dirname(strdup(file->name)) : ".";
        if (try_include(dir, filename, isimport))
            return;
    }
    for (int i = path_vec_len(std_include_path) - 1; i >= 0; i--)
        if (try_include(path_vec_get(std_include_path, i), filename, isimport))
            return;
  err:
    errort(hash, "cannot find header file: %s", filename);
}

static void read_include_next(Token *hash, File *file) {
    // [GNU] #include_next is a directive to include the "next" file
    // from the search path. This feature is used to override a
    // header file without getting into infinite inclusion loop.
    // This directive doesn't distinguish <> and "".
    bool std;
    char *filename = read_cpp_header_name(hash, &std);
    expect_newline();
    if (filename[0] == '/') {
        if (try_include("/", filename, false))
            return;
        goto err;
    }
    char *cur = fullpath(file->name);
    int i = path_vec_len(std_include_path) - 1;
    for (; i >= 0; i--) {
        char *dir = path_vec_get(std_include_path, i);
        if (!strcmp(cur, fullpath(format("%s/%s", dir, filename))))
            break;
    }
    for (i--; i >= 0; i--)
        if (try_include(path_vec_get(std_include_path, i), filename, false))
            return;
  err:
    errort(hash, "cannot find header file: %s", filename);
}

/*
 * #pragma
 */

static void parse_pragma_operand(Token *tok) {
    char *s = tok->sval;
    if (!strcmp(s, "once")) {
        char *path = fullpath(tok->file->name);
        once_map_put(once, path, once_map_token);
    } else if (!strcmp(s, "enable_warning")) {
        enable_warning = true;
    } else if (!strcmp(s, "disable_warning")) {
        enable_warning = false;
    } else {
        errort(tok, "unknown #pragma: %s", s);
    }
}

static void read_pragma() {
    Token *tok = read_ident();
    parse_pragma_operand(tok);
}

/*
 * #line
 */

static bool is_digit_sequence(char *p) {
    for (; *p; p++)
        if (!isdigit(*p))
            return false;
    return true;
}

static void read_line() {
    Token *tok = read_expand_newline();
    if (tok->kind != TNUMBER || !is_digit_sequence(tok->sval))
        errort(tok, "number expected after #line, but got %s", tok2s(tok));
    int line = atoi(tok->sval);
    tok = read_expand_newline();
    char *filename = NULL;
    if (tok->kind == TSTRING) {
        filename = tok->sval;
        expect_newline();
    } else if (tok->kind != TNEWLINE) {
        errort(tok, "newline or a source name are expected, but got %s", tok2s(tok));
    }
    File *f = current_file();
    f->line = line;
    if (filename)
        f->name = filename;
}

// GNU CPP outputs "# linenum filename flags" to preserve original
// source file information. This function reads them. Flags are ignored.
static void read_linemarker(Token *tok) {
    if (!is_digit_sequence(tok->sval))
        errort(tok, "line number expected, but got %s", tok2s(tok));
    int line = atoi(tok->sval);
    tok = lex();
    if (tok->kind != TSTRING)
        errort(tok, "filename expected, but got %s", tok2s(tok));
    char *filename = tok->sval;
    do {
        tok = lex();
    } while (tok->kind != TNEWLINE);
    File *file = current_file();
    file->line = line;
    file->name = filename;
}

/*
 * #-directive
 */

static void read_directive(Token *hash) {
    Token *tok = lex();
    if (tok->kind == TNEWLINE)
        return;
    if (tok->kind == TNUMBER) {
        read_linemarker(tok);
        return;
    }
    if (tok->kind != TIDENT)
        goto err;
    char *s = tok->sval;
    if (!strcmp(s, "define"))            read_define();
    else if (!strcmp(s, "elif"))         read_elif(hash);
    else if (!strcmp(s, "else"))         read_else(hash);
    else if (!strcmp(s, "endif"))        read_endif(hash);
    else if (!strcmp(s, "error"))        read_error(hash);
    else if (!strcmp(s, "if"))           read_if();
    else if (!strcmp(s, "ifdef"))        read_ifdef();
    else if (!strcmp(s, "ifndef"))       read_ifndef();
    else if (!strcmp(s, "import"))       read_include(hash, tok->file, true);
    else if (!strcmp(s, "include"))      read_include(hash, tok->file, false);
    else if (!strcmp(s, "include_next")) read_include_next(hash, tok->file);
    else if (!strcmp(s, "line"))         read_line();
    else if (!strcmp(s, "pragma"))       read_pragma();
    else if (!strcmp(s, "undef"))        read_undef();
    else if (!strcmp(s, "warning"))      read_warning(hash);
    else goto err;
    return;

  err:
    errort(hash, "unsupported preprocessor directive: %s", tok2s(tok));
}

/*
 * Special macros
 */

static void make_token_pushback(Token *tmpl, int kind, char *sval) {
    Token *tok = copy_token(tmpl);
    tok->kind = kind;
    tok->sval = sval;
    tok->slen = strlen(sval) + 1;
    tok->enc = ENC_NONE;
    unget_token(tok);
}

static void handle_date_macro(Token *tmpl) {
    char buf[20];
    strftime(buf, sizeof(buf), "%b %e %Y", &now);
    make_token_pushback(tmpl, TSTRING, strdup(buf));
}

static void handle_time_macro(Token *tmpl) {
    char buf[10];
    strftime(buf, sizeof(buf), "%T", &now);
    make_token_pushback(tmpl, TSTRING, strdup(buf));
}

static void handle_timestamp_macro(Token *tmpl) {
    // [GNU] __TIMESTAMP__ is expanded to a string that describes the date
    // and time of the last modification time of the current source file.
    char buf[30];
    strftime(buf, sizeof(buf), "%a %b %e %T %Y", localtime(&tmpl->file->mtime));
    make_token_pushback(tmpl, TSTRING, strdup(buf));
}

static void handle_file_macro(Token *tmpl) {
    make_token_pushback(tmpl, TSTRING, tmpl->file->name);
}

static void handle_line_macro(Token *tmpl) {
    make_token_pushback(tmpl, TNUMBER, format("%d", tmpl->file->line));
}

static void handle_pragma_macro(Token *tmpl) {
    expect('(');
    Token *operand = read_token();
    if (operand->kind != TSTRING)
        errort(operand, "_Pragma takes a string literal, but got %s", tok2s(operand));
    expect(')');
    parse_pragma_operand(operand);
    make_token_pushback(tmpl, TNUMBER, "1");
}

static void handle_base_file_macro(Token *tmpl) {
    make_token_pushback(tmpl, TSTRING, get_base_file());
}

static void handle_counter_macro(Token *tmpl) {
    static int counter = 0;
    make_token_pushback(tmpl, TNUMBER, format("%d", counter++));
}

static void handle_include_level_macro(Token *tmpl) {
    make_token_pushback(tmpl, TNUMBER, format("%d", stream_depth() - 1));
}

/*
 * Initializer
 */

void add_include_path(char *path) {
    path_vec_push(std_include_path, path);
}

static void define_obj_macro(char *name, Token *value) {
    macro_map_put(macros, name, make_obj_macro(make_token_vec_1(value)));
}

static void define_special_macro(char *name, SpecialMacroHandler *fn) {
    macro_map_put(macros, name, make_special_macro(fn));
}

static void init_keywords() {
#define op_id(id)   op_##id
#define kw_id(id)   kw_##id

#define op(id, str)         static int op_id(id) = id;
#define keyword(id, str, _) static int kw_id(id) = id;
#include "keyword.inc"
#undef keyword
#undef op

#define op(id, str)         keyword_map_put(keywords, str, &op_id(id));
#define keyword(id, str, _) keyword_map_put(keywords, str, &kw_id(id));
#include "keyword.inc"
#undef keyword
#undef op

#undef op_id
#undef kw_id
}

static void init_predefined_macros() {
    path_vec_push(std_include_path, "/usr/include/x86_64-linux-gnu");
    path_vec_push(std_include_path, "/usr/include/linux");
    path_vec_push(std_include_path, "/usr/include");
    path_vec_push(std_include_path, "/usr/local/include");
    path_vec_push(std_include_path, "/usr/local/lib/8cc/include");
    path_vec_push(std_include_path, BUILD_DIR "/include");

    define_special_macro("__DATE__", handle_date_macro);
    define_special_macro("__TIME__", handle_time_macro);
    define_special_macro("__FILE__", handle_file_macro);
    define_special_macro("__LINE__", handle_line_macro);
    define_special_macro("_Pragma",  handle_pragma_macro);
    // [GNU] Non-standard macros
    define_special_macro("__BASE_FILE__", handle_base_file_macro);
    define_special_macro("__COUNTER__", handle_counter_macro);
    define_special_macro("__INCLUDE_LEVEL__", handle_include_level_macro);
    define_special_macro("__TIMESTAMP__", handle_timestamp_macro);

    read_from_string("#include <" BUILD_DIR "/include/8cc.h>");
}

void init_now() {
    time_t timet = time(NULL);
    localtime_r(&timet, &now);
}

void cpp_init() {
    setlocale(LC_ALL, "C");
    init_keywords();
    init_now();
    init_predefined_macros();
}

/*
 * Public intefaces
 */

static Token *maybe_convert_keyword(Token *tok) {
    if (tok->kind != TIDENT)
        return tok;
    int *id = keyword_map_get(keywords, tok->sval);
    if (!id)
        return tok;
    Token *r = copy_token(tok);
    r->kind = TKEYWORD;
    r->id = *id;
    return r;
}

// Reads from a string as if the string is a content of input file.
// Convenient for evaluating small string snippet contaiing preprocessor macros.
void read_from_string(char *buf) {
    stream_stash(make_file_string(buf));
    node_vec *toplevels = read_toplevels();
    for (int i = 0; i < node_vec_len(toplevels); i++)
        emit_toplevel(node_vec_get(toplevels, i));
    stream_unstash();
}

Token *peek_token() {
    Token *r = read_token();
    unget_token(r);
    return r;
}

Token *read_token() {
    Token *tok;
    for (;;) {
        tok = read_expand();
        if (tok->bol && is_keyword(tok, '#') && tok->hideset == NULL) {
            read_directive(tok);
            continue;
        }
        assert(tok->kind < MIN_CPP_TOKEN);
        return maybe_convert_keyword(tok);
    }
}

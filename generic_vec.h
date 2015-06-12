// Copyright 2012 Rui Ueyama. Released under the MIT license.
// Generic version by Gergö Barany, 2015

// NO include guards because this is meant to be included several times,
// with different value types and vector names.

// Vectors are containers of void pointers that can change in size.
// This is a generic implementation that expects users to define the
// following macros:
// - VALUE_T (type expression such that "VALUE_T *x" is valid syntax)
// - VEC_NAME

#ifndef OMIT_TYPE_REDEF
typedef struct VEC_NAME {
    VALUE_T *body;
    int len;
    int nalloc;
} VEC_NAME;
#endif

#define empty_vector(VEC_NAME)  ((VEC_NAME){})

#define make_fun_(VEC_NAME)     make_##VEC_NAME
#define make_1_fun_(VEC_NAME)   make_##VEC_NAME##_1
#define copy_fun_(VEC_NAME)     VEC_NAME##_copy
#define push_fun_(VEC_NAME)     VEC_NAME##_push
#define append_fun_(VEC_NAME)   VEC_NAME##_append
#define pop_fun_(VEC_NAME)      VEC_NAME##_pop
#define get_fun_(VEC_NAME)      VEC_NAME##_get
#define set_fun_(VEC_NAME)      VEC_NAME##_set
#define head_fun_(VEC_NAME)     VEC_NAME##_head
#define tail_fun_(VEC_NAME)     VEC_NAME##_tail
#define reverse_fun_(VEC_NAME)  VEC_NAME##_reverse
#define body_fun_(VEC_NAME)     VEC_NAME##_body
#define reverse_fun_(VEC_NAME)  VEC_NAME##_reverse
#define len_fun_(VEC_NAME)      VEC_NAME##_len

#define make_fun(VEC_NAME)      make_fun_(VEC_NAME)
#define make_1_fun(VEC_NAME)    make_1_fun_(VEC_NAME)
#define copy_fun(VEC_NAME)      copy_fun_(VEC_NAME)
#define push_fun(VEC_NAME)      push_fun_(VEC_NAME)
#define append_fun(VEC_NAME)    append_fun_(VEC_NAME)
#define pop_fun(VEC_NAME)       pop_fun_(VEC_NAME)
#define get_fun(VEC_NAME)       get_fun_(VEC_NAME)
#define set_fun(VEC_NAME)       set_fun_(VEC_NAME)
#define head_fun(VEC_NAME)      head_fun_(VEC_NAME)
#define tail_fun(VEC_NAME)      tail_fun_(VEC_NAME)
#define reverse_fun(VEC_NAME)   reverse_fun_(VEC_NAME)
#define body_fun(VEC_NAME)      body_fun_(VEC_NAME)
#define reverse_fun(VEC_NAME)   reverse_fun_(VEC_NAME)
#define len_fun(VEC_NAME)       len_fun_(VEC_NAME)

VEC_NAME *make_fun(VEC_NAME)(void);
VEC_NAME *make_1_fun(VEC_NAME)(VALUE_T e);
VEC_NAME *copy_fun(VEC_NAME)(VEC_NAME *src);
void push_fun(VEC_NAME)(VEC_NAME *vec, VALUE_T elem);
void append_fun(VEC_NAME)(VEC_NAME *a, VEC_NAME *b);
VALUE_T pop_fun(VEC_NAME)(VEC_NAME *vec);
VALUE_T get_fun(VEC_NAME)(VEC_NAME *vec, int index);
void set_fun(VEC_NAME)(VEC_NAME *vec, int index, VALUE_T val);
VALUE_T head_fun(VEC_NAME)(VEC_NAME *vec);
VALUE_T tail_fun(VEC_NAME)(VEC_NAME *vec);
VEC_NAME *reverse_fun(VEC_NAME)(VEC_NAME *vec);
VALUE_T *body_fun(VEC_NAME)(VEC_NAME *vec);
int len_fun(VEC_NAME)(VEC_NAME *vec);

#ifdef DECLS_ONLY

#undef make_fun_
#undef make_1_fun_
#undef copy_fun_
#undef push_fun_
#undef append_fun_
#undef pop_fun_
#undef get_fun_
#undef set_fun_
#undef head_fun_
#undef tail_fun_
#undef reverse_fun_
#undef body_fun_
#undef reverse_fun_
#undef len_fun_

#undef make_fun
#undef make_1_fun
#undef copy_fun
#undef push_fun
#undef append_fun
#undef pop_fun
#undef get_fun
#undef set_fun
#undef head_fun
#undef tail_fun
#undef reverse_fun
#undef body_fun
#undef reverse_fun
#undef len_fun

#endif

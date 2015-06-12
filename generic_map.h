// Copyright 2014 Rui Ueyama. Released under the MIT license.
// Generic version by Gergö Barany, 2015

// NO include guards because this is meant to be included several times,
// with different value types and map names.
// Users are expected to define the following macros:
// - VALUE_T (should expand to a typedef name or a type of the form T*)
// - MAP_NAME

#ifndef OMIT_TYPE_REDEF
typedef struct MAP_NAME {
    struct MAP_NAME *parent;
    char **key;
    VALUE_T *val;
    int size;
    int nelem;
    int nused;
} MAP_NAME;
#endif

#define empty_map(MAP_NAME)         ((MAP_NAME){})

#define make_fun_(MAP_NAME)         make_##MAP_NAME
#define make_parent_fun_(MAP_NAME)  make_##MAP_NAME##_parent
#define get_fun_(MAP_NAME)          MAP_NAME##_get
#define put_fun_(MAP_NAME)          MAP_NAME##_put
#define remove_fun_(MAP_NAME)       MAP_NAME##_remove
#define len_fun_(MAP_NAME)          MAP_NAME##_len

#define make_fun(MAP_NAME)          make_fun_(MAP_NAME)
#define make_parent_fun(MAP_NAME)   make_parent_fun_(MAP_NAME)
#define get_fun(MAP_NAME)           get_fun_(MAP_NAME)
#define put_fun(MAP_NAME)           put_fun_(MAP_NAME)
#define remove_fun(MAP_NAME)        remove_fun_(MAP_NAME)
#define len_fun(MAP_NAME)           len_fun_(MAP_NAME)

MAP_NAME *make_fun(MAP_NAME)(void);
MAP_NAME *make_parent_fun(MAP_NAME)(MAP_NAME *parent);
VALUE_T get_fun(MAP_NAME)(MAP_NAME *m, char *key);
void put_fun(MAP_NAME)(MAP_NAME *m, char *key, VALUE_T val);
void remove_fun(MAP_NAME)(MAP_NAME *m, char *key);
size_t len_fun(MAP_NAME)(MAP_NAME *m);

#ifdef DECLS_ONLY

#undef make_fun_
#undef make_parent_fun_
#undef get_fun_
#undef put_fun_
#undef remove_fun_
#undef len_fun_

#undef make_fun
#undef make_parent_fun
#undef get_fun
#undef put_fun
#undef remove_fun
#undef len_fun

#endif

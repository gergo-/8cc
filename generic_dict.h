// Copyright 2014 Rui Ueyama. Released under the MIT license.
// Generic version by Gergö Barany, 2015

// NO include guards because this is meant to be included several times,
// with different value types and dict names.
// Users are expected the following macros:
// - VALUE_T (should expand to a typedef name or a type of the form T*)
// - MAP_NAME (name of a previously defined generic map with same VALUE_T)
// - DICT_NAME

#ifndef OMIT_TYPE_REDEF
typedef struct {
    MAP_NAME *map;
    key_vec *key;
} DICT_NAME;
#endif

#define make_fun_(DICT_NAME)    make_##DICT_NAME
#define get_fun_(DICT_NAME)     DICT_NAME##_get
#define put_fun_(DICT_NAME)     DICT_NAME##_put
#define keys_fun_(DICT_NAME)    DICT_NAME##_keys

#define make_fun(DICT_NAME)     make_fun_(DICT_NAME)
#define get_fun(DICT_NAME)      get_fun_(DICT_NAME)
#define put_fun(DICT_NAME)      put_fun_(DICT_NAME)
#define keys_fun(DICT_NAME)     keys_fun_(DICT_NAME)

DICT_NAME *make_fun(DICT_NAME)(void);
VALUE_T get_fun(DICT_NAME)(DICT_NAME *dict, char *key);
void put_fun(DICT_NAME)(DICT_NAME *dict, char *key, VALUE_T val);
key_vec *keys_fun(DICT_NAME)(DICT_NAME *dict);

#ifdef DECLS_ONLY

#undef make_fun_
#undef get_fun_
#undef put_fun_
#undef keys_fun_

#undef make_fun
#undef get_fun
#undef put_fun
#undef keys_fun

#endif

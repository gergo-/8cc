// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include <stdlib.h>
#include "8cc.h"

Dict *make_dict() {
    Dict *r = malloc(sizeof(Dict));
    r->map = make_map();
    r->key = make_key_vec();
    return r;
}

void *dict_get(Dict *dict, char *key) {
    return map_get(dict->map, key);
}

void dict_put(Dict *dict, char *key, void *val) {
    map_put(dict->map, key, val);
    key_vec_push(dict->key, key);
}

key_vec *dict_keys(Dict *dict) {
    return dict->key;
}

#define VEC_NAME    key_vec
#define VALUE_T     char*
#define OMIT_TYPE_REDEF
#include "generic_vec.h"
#include "generic_vec.c"
#undef VEC_NAME
#undef VALUE_T
#undef OMIT_TYPE_REDEF

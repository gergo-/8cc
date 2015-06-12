// Copyright 2012 Rui Ueyama. Released under the MIT license.
// Generic version by Gergö Barany, 2015

#include <stdlib.h>
#include "8cc.h"

#define map_make_fun_(MAP_NAME)     make_##MAP_NAME
#define map_get_fun_(MAP_NAME)      MAP_NAME##_get
#define map_put_fun_(MAP_NAME)      MAP_NAME##_put

#define map_make_fun(MAP_NAME)      map_make_fun_(MAP_NAME)
#define map_get_fun(MAP_NAME)       map_get_fun_(MAP_NAME)
#define map_put_fun(MAP_NAME)       map_put_fun_(MAP_NAME)

DICT_NAME *make_fun(DICT_NAME)() {
    DICT_NAME *r = malloc(sizeof(DICT_NAME));
    r->map = map_make_fun(MAP_NAME)();
    r->key = make_key_vec();
    return r;
}

VALUE_T get_fun(DICT_NAME)(DICT_NAME *dict, char *key) {
    return map_get_fun(MAP_NAME)(dict->map, key);
}

void put_fun(DICT_NAME)(DICT_NAME *dict, char *key, VALUE_T val) {
    map_put_fun(MAP_NAME)(dict->map, key, val);
    key_vec_push(dict->key, key);
}

key_vec *keys_fun(DICT_NAME)(DICT_NAME *dict) {
    return dict->key;
}

#undef make_fun_
#undef get_fun_
#undef put_fun_
#undef keys_fun_
#undef map_make_fun_
#undef map_get_fun_
#undef map_put_fun_

#undef make_fun
#undef get_fun
#undef put_fun
#undef keys_fun
#undef map_make_fun
#undef map_get_fun
#undef map_put_fun

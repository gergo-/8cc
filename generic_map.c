// Copyright 2014 Rui Ueyama. Released under the MIT license.
// Generic version by Gergö Barany, 2015

// This is an implementation of hash table.

// This is the generic version. Users are expected to define the following
// macros:
// - VALUE_T (should expand to a typedef name or a type of the form T*)
// - MAP_NAME

#include <stdlib.h>
#include <string.h>
#include "8cc.h"

#define INIT_SIZE 16
#define tombstone_object_(MAP_NAME) MAP_NAME##_tombstone_object
#define tombstone_object(MAP_NAME)  tombstone_object_(MAP_NAME)
static char tombstone_object(MAP_NAME);
#define TOMBSTONE (&tombstone_object(MAP_NAME))

#ifndef GENERIC_MAP_HASH_DEFINED
#define GENERIC_MAP_HASH_DEFINED
// including this several times is a bit of a hack
static uint32_t hash(char *p) {
    // FNV hash
    uint32_t r = 2166136261;
    for (; *p; p++) {
        r ^= *p;
        r *= 16777619;
    }
    return r;
}
#endif

#define do_make_fun_(MAP_NAME)  do_make_##MAP_NAME
#define do_make_fun(MAP_NAME)   do_make_fun_(MAP_NAME)
static MAP_NAME *do_make_fun(MAP_NAME)(MAP_NAME *parent, int size) {
    MAP_NAME *r = malloc(sizeof(MAP_NAME));
    r->parent = parent;
    r->key = calloc(size, sizeof(char *));
    r->val = calloc(size, sizeof(VALUE_T));
    r->size = size;
    r->nelem = 0;
    r->nused = 0;
    return r;
}

#define maybe_rehash_fun_(MAP_NAME) maybe_rehash_##MAP_NAME
#define maybe_rehash_fun(MAP_NAME)  maybe_rehash_fun_(MAP_NAME)
static void maybe_rehash_fun(MAP_NAME)(MAP_NAME *m) {
    if (!m->key) {
        m->key = calloc(INIT_SIZE, sizeof(char *));
        m->val = calloc(INIT_SIZE, sizeof(VALUE_T));
        m->size = INIT_SIZE;
        return;
    }
    if (m->nused < m->size * 0.7)
        return;
    int newsize = (m->nelem < m->size * 0.35) ? m->size : m->size * 2;
    char **k = calloc(newsize, sizeof(char *));
    VALUE_T *v = calloc(newsize, sizeof(VALUE_T));
    int mask = newsize - 1;
    for (int i = 0; i < m->size; i++) {
        if (m->key[i] == NULL || m->key[i] == TOMBSTONE)
            continue;
        int j = hash(m->key[i]) & mask;
        for (;; j = (j + 1) & mask) {
            if (k[j] != NULL)
                continue;
            k[j] = m->key[i];
            v[j] = m->val[i];
            break;
        }
    }
    m->key = k;
    m->val = v;
    m->size = newsize;
    m->nused = m->nelem;
}

MAP_NAME *make_fun(MAP_NAME)() {
    return do_make_fun(MAP_NAME)(NULL, INIT_SIZE);
}

MAP_NAME *make_parent_fun(MAP_NAME)(MAP_NAME *parent) {
    return do_make_fun(MAP_NAME)(parent, INIT_SIZE);
}

#define get_nostack_fun_(MAP_NAME)  MAP_NAME##_get_nostack
#define get_nostack_fun(MAP_NAME)   get_nostack_fun_(MAP_NAME)
static VALUE_T get_nostack_fun(MAP_NAME)(MAP_NAME *m, char *key) {
    if (!m->key)
        return NULL;
    int mask = m->size - 1;
    int i = hash(key) & mask;
    for (; m->key[i] != NULL; i = (i + 1) & mask)
        if (m->key[i] != TOMBSTONE && !strcmp(m->key[i], key))
            return m->val[i];
    return NULL;
}

VALUE_T get_fun(MAP_NAME)(MAP_NAME *m, char *key) {
    VALUE_T r = get_nostack_fun(MAP_NAME)(m, key);
    if (r)
        return r;
    // Map is stackable. If no value is found,
    // continue searching from the parent.
    if (m->parent)
        return get_fun(MAP_NAME)(m->parent, key);
    return NULL;
}

void put_fun(MAP_NAME)(MAP_NAME *m, char *key, VALUE_T val) {
    maybe_rehash_fun(MAP_NAME)(m);
    int mask = m->size - 1;
    int i = hash(key) & mask;
    for (;; i = (i + 1) & mask) {
        char *k = m->key[i];
        if (k == NULL || k == TOMBSTONE) {
            m->key[i] = key;
            m->val[i] = val;
            m->nelem++;
            if (k == NULL)
                m->nused++;
            return;
        }
        if (!strcmp(k, key)) {
            m->val[i] = val;
            return;
        }
    }
}

void remove_fun(MAP_NAME)(MAP_NAME *m, char *key) {
    if (!m->key)
        return;
    int mask = m->size - 1;
    int i = hash(key) & mask;
    for (; m->key[i] != NULL; i = (i + 1) & mask) {
        if (m->key[i] == TOMBSTONE || strcmp(m->key[i], key))
            continue;
        m->key[i] = TOMBSTONE;
        m->val[i] = NULL;
        m->nelem--;
        return;
    }
}

size_t len_fun(MAP_NAME)(MAP_NAME *m) {
    return m->nelem;
}

#undef make_fun_
#undef do_make_fun_
#undef make_parent_fun_
#undef get_fun_
#undef get_nostack_fun_
#undef maybe_rehash_fun_
#undef put_fun_
#undef remove_fun_
#undef len_fun_

#undef make_fun
#undef do_make_fun
#undef make_parent_fun
#undef get_fun
#undef get_nostack_fun
#undef maybe_rehash_fun
#undef put_fun
#undef remove_fun
#undef len_fun

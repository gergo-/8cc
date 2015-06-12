// Copyright 2012 Rui Ueyama. Released under the MIT license.
// Generic version by Gergö Barany, 2015

// Vectors are containers of void pointers that can change in size.
// This is a generic implementation that expects users to define the
// following macros:
// - VALUE_T (type expression such that "VALUE_T *x" is valid syntax)
// - VEC_NAME

#include <stdlib.h>
#include <string.h>
#include "8cc.h"

#define MIN_SIZE 8

// vector.c
extern int max(int a, int b);
extern int roundup(int n);

#define do_make_fun_(VEC_NAME)  do_make_##VEC_NAME
#define do_make_fun(VEC_NAME)   do_make_fun_(VEC_NAME)
static VEC_NAME *do_make_fun(VEC_NAME)(int size) {
    VEC_NAME *r = malloc(sizeof(VEC_NAME));
    size = roundup(size);
    if (size > 0)
        r->body = malloc(sizeof(VALUE_T) * size);
    r->len = 0;
    r->nalloc = size;
    return r;
}

VEC_NAME *make_fun(VEC_NAME)() {
    return do_make_fun(VEC_NAME)(0);
}

#define extend_fun_(VEC_NAME)   extend_##VEC_NAME
#define extend_fun(VEC_NAME)    extend_fun_(VEC_NAME)
static void extend_fun(VEC_NAME)(VEC_NAME *vec, int delta) {
    if (vec->len + delta <= vec->nalloc)
        return;
    int nelem = max(roundup(vec->len + delta), MIN_SIZE);
    VALUE_T *newbody = malloc(sizeof(VALUE_T) * nelem);
    memcpy(newbody, vec->body, sizeof(VALUE_T) * vec->len);
    vec->body = newbody;
    vec->nalloc = nelem;
}

VEC_NAME *make_1_fun(VEC_NAME)(VALUE_T e) {
    VEC_NAME *r = do_make_fun(VEC_NAME)(0);
    push_fun(VEC_NAME)(r, e);
    return r;
}

VEC_NAME *copy_fun(VEC_NAME)(VEC_NAME *src) {
    VEC_NAME *r = do_make_fun(VEC_NAME)(src->len);
    memcpy(r->body, src->body, sizeof(VALUE_T) * src->len);
    r->len = src->len;
    return r;
}

void push_fun(VEC_NAME)(VEC_NAME *vec, VALUE_T elem) {
    extend_fun(VEC_NAME)(vec, 1);
    vec->body[vec->len++] = elem;
}

void append_fun(VEC_NAME)(VEC_NAME *a, VEC_NAME *b) {
    extend_fun(VEC_NAME)(a, b->len);
    memcpy(a->body + a->len, b->body, sizeof(VALUE_T) * b->len);
    a->len += b->len;
}

VALUE_T pop_fun(VEC_NAME)(VEC_NAME *vec) {
    assert(vec->len > 0);
    return vec->body[--vec->len];
}

VALUE_T get_fun(VEC_NAME)(VEC_NAME *vec, int index) {
    assert(0 <= index && index < vec->len);
    return vec->body[index];
}

void set_fun(VEC_NAME)(VEC_NAME *vec, int index, VALUE_T val) {
    assert(0 <= index && index < vec->len);
    vec->body[index] = val;
}

VALUE_T head_fun(VEC_NAME)(VEC_NAME *vec) {
    assert(vec->len > 0);
    return vec->body[0];
}

VALUE_T tail_fun(VEC_NAME)(VEC_NAME *vec) {
    assert(vec->len > 0);
    return vec->body[vec->len - 1];
}

VEC_NAME *reverse_fun(VEC_NAME)(VEC_NAME *vec) {
    VEC_NAME *r = do_make_fun(VEC_NAME)(vec->len);
    for (int i = 0; i < vec->len; i++)
        r->body[i] = vec->body[vec->len - i - 1];
    r->len = vec->len;
    return r;
}

VALUE_T *body_fun(VEC_NAME)(VEC_NAME *vec) {
    return vec->body;
}

int len_fun(VEC_NAME)(VEC_NAME *vec) {
    return vec->len;
}

#undef make_fun_
#undef do_make_fun_
#undef extend_fun_
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
#undef do_make_fun
#undef extend_fun
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

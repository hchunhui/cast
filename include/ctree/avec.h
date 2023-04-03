/**
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef AVEC_H
#define AVEC_H

#include "vec.h"

#define avec_t(T) vec_t(T)
#define avec_init(v) vec_init(v)
#define avec_deinit(v) avec_init(v)

#define avec_push(v, val)\
  ( avec_expand_(vec_unpack_(v)) ? -1 :\
    ((v)->data[(v)->length++] = (val), 0), 0 )

#define avec_emplace(v)\
  ( avec_expand_(vec_unpack_(v)) ? NULL :\
    &((v)->data[(v)->length++]) )

#define avec_pop(v)\
  (v)->data[--(v)->length]

#define avec_splice(v, start, count) vec_splice(v, start, count)
#define avec_swapsplice(v, start, count) vec_swapsplice(v, start, count)

#define avec_insert(v, idx, val)\
  ( avec_insert_(vec_unpack_(v), idx) ? -1 :\
    ((v)->data[idx] = (val), 0), (v)->length++, 0 )

#define avec_sort(v, fn) vec_sort(v, fn)
#define avec_swap(v, idx1, idx2) vec_swap(v, idx1, idx2)
#define avec_truncate(v, len) vec_truncate(v, len)
#define avec_clear(v) vec_clear(v)
#define avec_first(v) vec_first(v)
#define avec_last(v) vec_last(v)

#define avec_reserve(v, n)\
  avec_reserve_(vec_unpack_(v), n)

#define avec_compact(v)

#define avec_pusharr(v, arr, count)\
  do {\
    int i__, n__ = (count);\
    if (avec_reserve_po2_(vec_unpack_(v), (v)->length + n__) != 0) break;\
    for (i__ = 0; i__ < n__; i__++) {\
      (v)->data[(v)->length++] = (arr)[i__];\
    }\
  } while (0)

#define avec_extend(v, v2)\
  avec_pusharr((v), (v2)->data, (v2)->length)

#define avec_find(v, val, idx) vec_find(v, val, idx)
#define avec_remove(v, val) vec_remove(v, val)
#define avec_reverse(v) vec_reverse(v)
#define avec_foreach(v, var, iter) vec_foreach(v, var, iter)
#define avec_foreach_rev(v, var, iter) vec_foreach_rev(v, var, iter)
#define avec_foreach_ptr(v, var, iter) vec_foreach_ptr(v, var, iter)
#define avec_foreach_ptr_rev(v, var, iter) vec_foreach_ptr_rev(v, var, iter)

BEGIN_MANAGED
int avec_expand_(char **data, int *length, int *capacity, int memsz);
int avec_reserve_(char **data, int *length, int *capacity, int memsz, int n);
int avec_reserve_po2_(char **data, int *length, int *capacity, int memsz,
                     int n);
int avec_insert_(char **data, int *length, int *capacity, int memsz,
                int idx);
END_MANAGED
#endif

/* Copyright (C) 2021-2023 Isaac Liu
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>. */

#ifndef __UTIL_H
#define __UTIL_H

#define likely(x)               __builtin_expect (!!(x), 1)
#define unlikely(x)             __builtin_expect (!!(x), 0)

#define log(fmt, ...)           printf ("e2fsmac: " fmt "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define log_debug(fmt, ...) \
  printf ("e2fsmac: " fmt " (%s:%d)\n", ##__VA_ARGS__, __func__, __LINE__)
#else
#define log_debug(fmt, ...) ((void) 0)
#endif

#define kassert(x) (x) ? (void) 0 : panic ("e2fsmac: assertion failed: " #x)

void *kmalloc (size_t size, int flags);
void *krealloc (void *ptr, size_t old, size_t new, int flags);
void kfree (void *ptr);
#ifdef DEBUG
void kmemassert (void);
#endif

#endif

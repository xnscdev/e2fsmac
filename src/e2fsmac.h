/* e2fsmac.h -- This file is part of e2fsmac.
   Copyright (C) 2021 XNSC

   e2fsmac is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   e2fsmac is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with e2fsmac. If not, see <https://www.gnu.org/licenses/>. */

#ifndef _E2FSMAC_H
#define _E2FSMAC_H

#include <kern/debug.h>
#include <libkern/libkern.h>
#include <mach/mach_types.h>

#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)

#define log(fmt, ...) printf (KEXT_NAME ": " fmt "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define log_debug(fmt, ...) \
  printf (KEXTNAME ": " fmt " (%s:%d)\n", ##__VA_ARGS__, __func__, __LINE__)
#else
#define log_debug(fmt, ...) ((void) 0)
#endif

#endif

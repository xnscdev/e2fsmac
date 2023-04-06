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

#include <libkern/OSAtomic.h>
#include <sys/systm.h>
#include "util.h"

#ifdef DEBUG
static volatile SInt64 refcnt;
#endif

void *
kmalloc (size_t size, int flags)
{
  void *addr = _MALLOC (size, M_TEMP, flags);
#ifdef DEBUG
  if (likely (addr))
    OSIncrementAtomic64 (&refcnt);
#endif
  return addr;
}

void *
krealloc (void *ptr, size_t old, size_t new, int flags)
{
  void *addr;
  if (old == new)
    return ptr;
  if (!ptr)
    return kmalloc (new, flags);
  addr = _MALLOC (new, M_TEMP, flags);
  if (unlikely (!addr))
    return NULL;
  memcpy (addr, ptr, MIN (old, new));
  kfree (ptr);
  return addr;
}

void
kfree (void *ptr)
{
#ifdef DEBUG
  if (ptr)
    OSDecrementAtomic64 (&refcnt);
#endif
  _FREE (ptr, M_TEMP);
}

#ifdef DEBUG
void
kmemassert (void)
{
  if (refcnt)
    log_debug ("memory leak, %lld referenced allocations", refcnt);
}
#endif

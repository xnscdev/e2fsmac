/* init.c -- This file is part of e2fsmac.
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

#include <libkern/libkern.h>
#include <mach/mach_types.h>

kern_return_t
e2fsmac_start (kmod_info_t *kinfo, void *data)
{
  return KERN_SUCCESS;
}

kern_return_t
e2fsmac_stop (kmod_info_t *kinfo, void *data)
{
  return KERN_SUCCESS;
}

#ifdef NO_XCODE

kern_return_t _start (kmod_info_t *kinfo, void *data);
kern_return_t _stop (kmod_info_t *kinfo, void *data);

KMOD_EXPLICIT_DECL (KEXT_BUNDLE, KEXT_BUILD, _start, _stop)
  __attribute__ ((visibility ("default")))

__private_extern__ kmod_start_func_t *_realmain = e2fsmac_start;
__private_extern__ kmod_start_func_t *_antimain = e2fsmac_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;

#endif

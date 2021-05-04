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

#include "e2fsmac.h"

lck_grp_t *e2fs_lck_grp;

static vfstable_t ext2_vfstable_ref;

static struct vfs_fsentry ext2_vfsentry = {
  &ext2_vfsops,
  sizeof (ext2_vnopv_desc_list) / sizeof (ext2_vnopv_desc_list[0]),
  ext2_vnopv_desc_list,
  0,
  EXT2_NAME,
  E2FS_VFS_FLAGS,
  {NULL, NULL}
};

kern_return_t
e2fsmac_start (kmod_info_t *kinfo, void *data)
{
  kern_return_t err;
  log ("starting");
  log_debug ("built with Apple LLVM %s", __clang_version__);

  e2fs_lck_grp = lck_grp_alloc_init (E2FS_LCK_GRP_NAME, LCK_GRP_ATTR_NULL);
  if (e2fs_lck_grp == NULL)
    {
      log ("failed to allocate lock group");
      return KERN_FAILURE;
    }

  err = vfs_fsadd (&ext2_vfsentry, &ext2_vfstable_ref);
  if (err != 0)
    {
      log ("failed to register ext2 filesystem (errno %d)", err);
      lck_grp_free (e2fs_lck_grp);
      return KERN_FAILURE;
    }
  log_debug ("successfully registered ext2 filesystem");
  return KERN_SUCCESS;
}

kern_return_t
e2fsmac_stop (kmod_info_t *kinfo, void *data)
{
  kern_return_t err;
  log ("stopping");

  err = vfs_fsremove (ext2_vfstable_ref);
  if (err != 0)
    {
      log ("failed to remove filesystem (errno %d)", err);
      return KERN_FAILURE;
    }

  lck_grp_free (e2fs_lck_grp);
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

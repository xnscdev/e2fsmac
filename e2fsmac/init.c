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

#include <mach/mach_types.h>
#include "e2fsmac.h"

#define E2FS_VFS_FLAGS          (VFS_TBLTHREADSAFE	\
				 | VFS_TBLFSNODELOCK	\
				 | VFS_TBLNOTYPENUM	\
				 | VFS_TBLLOCALVOL	\
				 | VFS_TBL64BITREADY)

lck_grp_t *ext2_lck_grp;

static vfstable_t ext2_vfstable_ref;

static struct vfs_fsentry ext2_vfsentry =
  {
    &ext2_vfsops,
    sizeof ext2_vnopv_desc_list / sizeof *ext2_vnopv_desc_list,
    ext2_vnopv_desc_list,
    0,
    "ext2",
    E2FS_VFS_FLAGS,
    {NULL, NULL}
  };

kern_return_t
e2fsmac_start (kmod_info_t *ki, void *d)
{
  kern_return_t err;
  ext2_lck_grp = lck_grp_alloc_init ("e2fsmac.lock", LCK_GRP_ATTR_NULL);
  if (!ext2_lck_grp)
    {
      log ("failed to allocate lock group");
      goto err0;
    }

  err = vfs_fsadd (&ext2_vfsentry, &ext2_vfstable_ref);
  if (err)
    {
      log ("failed to register ext2 filesystem (errno %d)", err);
      goto err1;
    }

  log ("successfully registered ext2 filesystem");
  return KERN_SUCCESS;

 err1:
  lck_grp_free (ext2_lck_grp);
 err0:
  return KERN_FAILURE;
}

kern_return_t
e2fsmac_stop(kmod_info_t *ki, void *d)
{
  kern_return_t err = vfs_fsremove (ext2_vfstable_ref);
  if (err)
    {
      log ("failed to remove filesystem (errno %d)", err);
      return KERN_FAILURE;
    }

  lck_grp_free (ext2_lck_grp);
#ifdef DEBUG
  kmemassert ();
#endif
  return KERN_SUCCESS;
}

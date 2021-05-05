/* vfsops.c -- This file is part of e2fsmac.
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

#include <sys/systm.h>
#include "e2fsmac.h"

struct vfsops ext2_vfsops = {
  .vfs_mount = ext2_mount,
  .vfs_start = ext2_start,
  .vfs_unmount = ext2_unmount,
  .vfs_root = ext2_root,
  .vfs_getattr = ext2_getvfsattr
};

int
ext2_mount (struct mount *mp, vnode_t devvp, user_addr_t data,
	    vfs_context_t ctx)
{
  errno_t err;
  struct ext2_args args;
  struct ext2_mount *emp;
  struct vfsstatfs *st;

  if (vfs_isupdate (mp) || vfs_iswriteupgrade (mp))
    {
      err = ENOTSUP;
      log ("update mounting is currently unsupported");
      goto exit;
    }

  err = copyin (data, &args, sizeof (struct ext2_args));
  if (err != 0)
    {
      log_debug ("failed to copy mount arguments (errno %d)", err);
      goto exit;
    }

  emp = kmalloc (sizeof (struct ext2_mount), M_ZERO);
  if (unlikely (emp == NULL))
    {
      err = ENOMEM;
      log_debug ("failed to allocate private mount structure");
      goto exit;
    }
  vfs_setfsprivate (mp, emp);

  err = vnode_ref (devvp);
  if (err != 0)
    {
      log_debug ("vnode_ref() returned nonzero value (errno %d)", err);
      goto exit;
    }
  emp->devvp = devvp;
  emp->devid = vnode_specrdev (devvp);

  emp->mtx_root = lck_mtx_alloc_init (e2fs_lck_grp, NULL);
  if (unlikely (emp->mtx_root == NULL))
    {
      err = ENOMEM;
      log_debug ("failed to allocate lock");
      goto exit;
    }

  vfs_setflags (mp, MNT_RDONLY);
  emp->mp = mp;

  st = vfs_statfs (mp);
  /* TODO Fill statfs */

 exit:
  if (err != 0)
    ext2_unmount (mp, MNT_FORCE, ctx);
  return err;
}

int
ext2_start (struct mount *mp, int flags, vfs_context_t ctx)
{
  return 0;
}

int
ext2_unmount (struct mount *mp, int flags, vfs_context_t ctx)
{
  errno_t err;
  struct ext2_mount *emp;

  err = vflush (mp, NULL, flags & MNT_FORCE ? FORCECLOSE : 0);
  if (err != 0)
    {
      log_debug ("vflush() returned nonzero value (errno %d)", err);
      return err;
    }

  emp = vfs_fsprivate (mp);
  if (emp == NULL)
    return 0;

  if (emp->devvp != NULL)
    {
      vnode_rele (emp->devvp);
      emp->devvp = NULL;
      emp->devid = 0;
    }

  kassert (!emp->attach_root);
  kassert (!emp->wait_root);
  kassert (emp->rootvp == NULL);

  if (emp->mtx_root != NULL)
    lck_mtx_free (emp->mtx_root, e2fs_lck_grp);
  kfree (emp);
  return 0;
}

int
ext2_root (struct mount *mp, vnode_t *vpp, vfs_context_t ctx)
{
  return ENOTSUP;
}

int
ext2_getvfsattr (struct mount *mp, struct vfs_attr *attr, vfs_context_t ctx)
{
  return ENOTSUP;
}

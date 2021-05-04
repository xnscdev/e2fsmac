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

#include "e2fsmac.h"

static int
ext2_mount (struct mount *mp, vnode_t devvp, user_addr_t data,
	    vfs_context_t ctx)
{
  return ENOTSUP;
}

static int
ext2_start (struct mount *mp, int flags, vfs_context_t ctx)
{
  return 0;
}

static int
ext2_unmount (struct mount *mp, int flags, vfs_context_t ctx)
{
  return ENOTSUP;
}

static int
ext2_root (struct mount *mp, vnode_t *vpp, vfs_context_t ctx)
{
  return ENOTSUP;
}

static int
ext2_getattr (struct mount *mp, struct vfs_attr *attr, vfs_context_t ctx)
{
  return ENOTSUP;
}

struct vfsops ext2_vfsops = {
  .vfs_mount = ext2_mount,
  .vfs_start = ext2_start,
  .vfs_unmount = ext2_unmount,
  .vfs_root = ext2_root,
  .vfs_getattr = ext2_getattr
};

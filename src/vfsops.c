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

static void
ext2_init_volattrs (struct ext2_mount *emp, vfs_context_t ctx)
{
  vol_capabilities_attr_t *cap;
  vol_attributes_attr_t *attr;

  cap = &emp->attr.f_capabilities;
  attr = &emp->attr.f_attributes;

  cap->capabilities[VOL_CAPABILITIES_FORMAT] = VOL_CAP_FMT_SYMBOLICLINKS
    | VOL_CAP_FMT_HARDLINKS
    | VOL_CAP_FMT_CASE_SENSITIVE
    | VOL_CAP_FMT_CASE_PRESERVING
    | VOL_CAP_FMT_2TB_FILESIZE
    | VOL_CAP_FMT_OPENDENYMODES;
  cap->valid[VOL_CAPABILITIES_FORMAT] = 0xffffffff;

  cap->capabilities[VOL_CAPABILITIES_INTERFACES] = VOL_CAP_INT_ATTRLIST;
  cap->valid[VOL_CAPABILITIES_INTERFACES] = 0xffffffff;

  attr->validattr.commonattr = ATTR_CMN_NAME
    | ATTR_CMN_DEVID
    | ATTR_CMN_FSID
    | ATTR_CMN_OBJTYPE
    | ATTR_CMN_OBJID
    | ATTR_CMN_PAROBJID
    | ATTR_CMN_CRTIME
    | ATTR_CMN_MODTIME
    | ATTR_CMN_CHGTIME
    | ATTR_CMN_ACCTIME
    | ATTR_CMN_OWNERID
    | ATTR_CMN_GRPID
    | ATTR_CMN_ACCESSMASK
    | ATTR_CMN_FLAGS;
  attr->validattr.dirattr = ATTR_DIR_LINKCOUNT
    | ATTR_DIR_ALLOCSIZE
    | ATTR_DIR_IOBLOCKSIZE
    | ATTR_DIR_DATALENGTH;
  attr->validattr.fileattr = ATTR_FILE_LINKCOUNT
    | ATTR_FILE_TOTALSIZE
    | ATTR_FILE_ALLOCSIZE
    | ATTR_FILE_IOBLOCKSIZE
    | ATTR_FILE_DEVTYPE
    | ATTR_FILE_DATALENGTH
    | ATTR_FILE_DATAALLOCSIZE;
  attr->validattr.forkattr = 0;
  attr->validattr.volattr = ATTR_VOL_FSTYPE
    | ATTR_VOL_SIZE
    | ATTR_VOL_SPACEFREE
    | ATTR_VOL_SPACEAVAIL
    | ATTR_VOL_IOBLOCKSIZE
    | ATTR_VOL_OBJCOUNT
    | ATTR_VOL_FILECOUNT
    | ATTR_VOL_DIRCOUNT
    | ATTR_VOL_MAXOBJCOUNT
    | ATTR_VOL_MOUNTPOINT
    | ATTR_VOL_NAME
    | ATTR_VOL_MOUNTFLAGS
    | ATTR_VOL_MOUNTEDDEVICE
    | ATTR_VOL_CAPABILITIES
    | ATTR_VOL_UUID
    | ATTR_VOL_RESERVED_SIZE
    | ATTR_VOL_ATTRIBUTES;
  memcpy (&attr->nativeattr, &attr->validattr, sizeof (attribute_set_t));
}

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

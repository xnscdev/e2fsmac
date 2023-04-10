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

#ifndef __E2FSMAC_H
#define __E2FSMAC_H

#include <libkern/locks.h>
#include <sys/mount.h>
#include "ext2_args.h"
#include "ext2fs.h"

#define EXT2_VOLNAME_MAXLEN 16

struct ext2_mount
{
  int magic;
  mount_t mp;
  dev_t devid;
  vnode_t devvp;
  char volname[EXT2_VOLNAME_MAXLEN];
  struct vfs_attr attr;
  lck_mtx_t *mtx_root;
  unsigned char attach_root;
  unsigned char wait_root;
  vnode_t rootvp;
  uid_t uid;
  gid_t gid;
  ext2_filsys fs;
};

struct ext2_fsnode
{
  ext2_file_t file;
  ext2_ino_t ino;
  struct ext2_inode *inode;
};

extern lck_grp_t *ext2_lck_grp;

extern struct vfsops ext2_vfsops;
extern struct vnodeopv_desc *ext2_vnopv_desc_list[1];
extern int (**ext2_vnop_p) (void *);

int ext2_create_vnode (struct ext2_mount *emp, ext2_ino_t ino, vnode_t dvp,
		       vnode_t *vpp);
int ext2_open_vnode (struct ext2_mount *emp, vnode_t vp, int flags);

#endif

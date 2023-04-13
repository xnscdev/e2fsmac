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

#include "e2fsmac.h"
#include "ext2fs.h"

int
ext2_create_vnode (struct ext2_mount *emp, ext2_ino_t ino, vnode_t dvp,
		   vnode_t *vpp)
{
  struct vnode_fsparam param;
  ext2_file_t file;
  struct ext2_inode *inode;
  struct ext2_fsnode *fsnode;
  enum vtype vmode;
  vnode_t vp = NULL;
  int ret;

  ret = ext2fs_file_open (emp->fs, ino, 0, &file);
  if (ret)
    return ret;

  inode = ext2fs_file_get_inode (file);
  if (LINUX_S_ISREG (inode->i_mode))
    vmode = VREG;
  else if (LINUX_S_ISDIR (inode->i_mode))
    vmode = VDIR;
  else if (LINUX_S_ISBLK (inode->i_mode))
    vmode = VBLK;
  else if (LINUX_S_ISCHR (inode->i_mode))
    vmode = VCHR;
  else if (LINUX_S_ISLNK (inode->i_mode))
    vmode = VLNK;
  else if (LINUX_S_ISSOCK (inode->i_mode))
    vmode = VSOCK;
  else if (LINUX_S_ISFIFO (inode->i_mode))
    vmode = VFIFO;
  else
    {
      ret = EFTYPE;
      goto err0;
    }

  fsnode = e2fsmac_malloc (sizeof *fsnode, M_ZERO);
  if (unlikely (!fsnode))
    {
      ret = ENOMEM;
      log_debug ("e2fsmac_malloc(): errno %d", ret);
      goto err0;
    }
  fsnode->ino = ino;

  param.vnfs_mp = emp->mp;
  param.vnfs_vtype = vmode;
  param.vnfs_str = "ext2";
  param.vnfs_dvp = dvp;
  param.vnfs_fsnode = fsnode;
  param.vnfs_vops = ext2_vnop_p;
  param.vnfs_markroot = ino == EXT2_ROOT_INO;
  param.vnfs_marksystem = 0;
  param.vnfs_rdev = 0;
  param.vnfs_filesize = ext2fs_file_get_size (file);
  param.vnfs_cnp = NULL;
  param.vnfs_flags = VNFS_NOCACHE | VNFS_CANTCACHE;
  ret = vnode_create (VNCREATE_FLAVOR, sizeof param, &param, &vp);
  if (!ret)
    log_debug ("created vnode %#x for ino %u, fsnode %p",
	       vnode_vid (vp), ino, vnode_fsnode (vp));
  *vpp = vp;

 err0:
  ext2fs_file_close (file);
  return ret;
}

int
ext2_open_vnode (struct ext2_mount *emp, vnode_t vp, int flags)
{
  struct ext2_fsnode *fsnode = vnode_fsnode (vp);
  int ret;

  ret = ext2fs_file_open (emp->fs, fsnode->ino, flags, &fsnode->file);
  if (ret)
    return ret;
  fsnode->inode = ext2fs_file_get_inode (fsnode->file);
  fsnode->flags = flags;
  log_debug ("opened vnode %#x, fsnode %p", vnode_vid (vp), vnode_fsnode (vp));
  return 0;
}

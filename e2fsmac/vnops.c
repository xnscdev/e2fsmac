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

#include <sys/fcntl.h>
#include <sys/dirent.h>
#include "e2fsmac.h"

int (**ext2_vnop_p) (void *);

static int
uiomove_atomic (void *addr, size_t size, uio_t uio)
{
  int ret;
  if (unlikely (size > INT_MAX))
    ret = ERANGE;
  else if (unlikely (size > (user_size_t) uio_resid (uio)))
    ret = ENOBUFS;
  else
    ret = uiomove (addr, (int) size, uio);

  if (ret)
    log ("uiomove_atomic(): size: %zu, resid: %lld, errno: %d",
	 size, uio_resid (uio), ret);
  return ret;
}

static void
ext2_detach_root_vnode (struct ext2_mount *emp, vnode_t vp)
{
  int ret;
  lck_mtx_lock (emp->mtx_root);
  if (emp->attach_root)
    kassert (!emp->rootvp);
  if (emp->rootvp)
    {
      kassert (emp->rootvp == vp);
      ret = vnode_removefsref (emp->rootvp);
      kassert (!ret);
      emp->rootvp = NULL;
    }
  lck_mtx_unlock (emp->mtx_root);
}

static int
ext2_vnop_default (struct vnop_generic_args *args)
{
  return ENOTSUP;
}

static int
ext2_vnop_lookup (struct vnop_lookup_args *args)
{
  int ret = 0;
  vnode_t dvp = args->a_dvp;
  vnode_t *vpp = args->a_vpp;
  struct componentname *cnp = args->a_cnp;
  struct ext2_mount *emp = vfs_fsprivate (vnode_mount (dvp));
  struct ext2_fsnode *fsnode;
  vnode_t vp = NULL;
  char name[EXT2_NAME_LEN + 1];
  ext2_ino_t ino;

  kassert (vnode_isdir (dvp));
  fsnode = vnode_fsnode (dvp);

  strlcpy (name, cnp->cn_nameptr, cnp->cn_namelen + 1);

  if ((name[0] == '.' && name[1] == '\0')
      || ((cnp->cn_flags & ISDOTDOT) && vnode_isvroot (dvp)))
    {
      ret = vnode_get (dvp);
      if (!ret)
	vp = dvp;
    }
  else if (cnp->cn_flags & ISDOTDOT)
    {
      vp = vnode_getparent (dvp);
      if (!vp)
	ret = ENOENT;
    }
  else
    {
      ret = ext2fs_namei (emp->fs, EXT2_ROOT_INO, fsnode->ino, name, &ino);
      if (ret)
	{
	  ret = ENOENT;
	  log_debug ("lookup failed: name: %s, cnp: %s, errno: %d",
		     name, cnp->cn_nameptr, ret);
	  goto out;
	}
      ret = ext2_create_vnode (emp, ino, dvp, &vp);
      if (ret)
	{
	  ret = EIO;
	  log_debug ("ext2_create_vnode(): errno %d", ret);
	  goto out;
	}
      ret = ext2_open_vnode (emp, vp, 0);
      if (ret)
	{
	  ret = EIO;
	  log_debug ("ext2_open_vnode(): errno %d", ret);
	  vnode_put (vp);
	  goto out;
	}
    }

  *vpp = vp;
 out:
  if (!ret)
    log_debug ("lookup: dvp: %#x, nameptr: %s, name: %s, vpp: %#x",
	       vnode_vid (dvp), cnp->cn_nameptr, name, vnode_vid (vp));
  return ret;
}

static int
ext2_vnop_open (struct vnop_open_args *args)
{
  vnode_t vp = args->a_vp;
  int mode = args->a_mode;
  struct ext2_mount *emp = vfs_fsprivate (vnode_mount (vp));
  struct ext2_fsnode *fsnode = vnode_fsnode (vp);
  int flags = (mode & FWRITE) ? EXT2_FILE_WRITE : 0;
  int ret = 0;

  kassert (fsnode);
  if (fsnode->file && fsnode->flags != flags)
    {
      log_debug ("open: freeing ext2_file_t in wrong mode: %p", fsnode->file);
      ext2fs_file_close (fsnode->file);
      fsnode->file = NULL;
    }
  if (!fsnode->file)
    {
      ret = ext2_open_vnode (emp, vp, flags);
      if (ret)
	log_debug ("ext2_open_vnode(): errno %d", ret);
    }
  log_debug ("open: vnode: %#x, fsnode: %p, mode:%s%s", vnode_vid (vp),
	     vnode_fsnode (vp),
	     (mode & FREAD) ? " FREAD" : "",
	     (mode & FWRITE) ? " FWRITE" : "");
  return ret;
}

static int
ext2_vnop_close (struct vnop_close_args *args)
{
  vnode_t vp = args->a_vp;
  log_debug ("close: vnode: %#x", vnode_vid (vp));
  return 0;
}

static int
ext2_vnop_getattr (struct vnop_getattr_args *args)
{
  vnode_t vp = args->a_vp;
  struct vnode_attr *vap = args->a_vap;
  struct ext2_fsnode *fsnode = vnode_fsnode (vp);
  struct ext2_mount *emp = vfs_fsprivate (vnode_mount (vp));
  struct timespec create_time = { fsnode->inode->i_ctime, 0 };
  struct timespec access_time = { fsnode->inode->i_atime, 0 };
  struct timespec modify_time = { fsnode->inode->i_mtime, 0 };

  VATTR_RETURN (vap, va_rdev, 0);
  VATTR_RETURN (vap, va_nlink, fsnode->inode->i_links_count);
  VATTR_RETURN (vap, va_data_size, ext2fs_file_get_size (fsnode->file));
  VATTR_RETURN (vap, va_mode, fsnode->inode->i_mode);
  VATTR_RETURN (vap, va_uid, fsnode->inode->i_uid);
  VATTR_RETURN (vap, va_gid, fsnode->inode->i_gid);
  VATTR_RETURN (vap, va_create_time, create_time);
  VATTR_RETURN (vap, va_access_time, access_time);
  VATTR_RETURN (vap, va_modify_time, modify_time);
  VATTR_RETURN (vap, va_change_time, modify_time);
  VATTR_RETURN (vap, va_iosize, emp->fs->blocksize);
  VATTR_RETURN (vap, va_fileid, fsnode->ino);
  VATTR_RETURN (vap, va_fsid, emp->devid);
  log_debug ("getattr: vnode: %#x", vnode_vid (vp));
  return 0;
}

static int
ext2_vnop_readdir (struct vnop_readdir_args *args)
{
  static int known_flags = VNODE_READDIR_EXTENDED
    | VNODE_READDIR_REQSEEKOFF
    | VNODE_READDIR_SEEKOFF32
    | VNODE_READDIR_NAMEMAX;
  int err = 0;
  vnode_t vp = args->a_vp;
  struct uio *uio = args->a_uio;
  int flags = args->a_flags;
  int *eofflag = args->a_eofflag;
  int *numdirent = args->a_numdirent;
  int eof = 0;
  int num = 0;
  struct dirent di;
  off_t index;

  if (flags & known_flags)
    {
      err = EINVAL;
      log_debug ("found NFS-exported readdir flags %#x: errno %d",
		 flags & known_flags, err);
      goto err0;
    }

  di.d_fileno = 2;
  di.d_reclen = sizeof di;
  di.d_type = DT_DIR;

  kassert (uio_offset (uio) % 7 == 0);
  index = uio_offset (uio) / 7;

  if (!index)
    {
      di.d_namlen = (uint8_t) strlen (".");
      strlcpy (di.d_name, ".", sizeof di.d_name);
      err = uiomove_atomic (&di, sizeof di, uio);
      if (!err)
	{
	  num++;
	  index++;
	}
    }

  if (!err && index == 1)
    {
      di.d_namlen = (uint8_t) strlen ("..");
      strlcpy (di.d_name, "..", sizeof di.d_name);
      err = uiomove_atomic (&di, sizeof di, uio);
      if (!err)
	{
	  num++;
	  index++;
	}
    }

  if (err == ENOBUFS)
    err = 0;
  else if (err)
    goto err0;

  uio_setoffset (uio, index * 7);
  eof = index > 1;

  if (eofflag)
    *eofflag = eof;
  if (numdirent)
    *numdirent = num;

  log_debug ("readdir: vnode: %#x, eofflag: %d, numdirent: %d",
	     vnode_vid (vp), eof, num);

 err0:
  return err;
}

static int
ext2_vnop_reclaim (struct vnop_reclaim_args *args)
{
  vnode_t vp = args->a_vp;
  struct ext2_mount *emp = vfs_fsprivate (vnode_mount (vp));
  struct ext2_fsnode *fsnode = vnode_fsnode (vp);

  if (fsnode)
    {
      if (fsnode->file)
	{
	  log_debug ("close: freeing ext2_file_t: %p", fsnode->file);
	  ext2fs_file_close (fsnode->file);
	  fsnode->file = NULL;
	}
      vnode_clearfsnode (vp);
    }

  if (vnode_isvroot (vp))
    ext2_detach_root_vnode (emp, vp);

  log_debug ("reclaim: vnode: %#x", vnode_vid (vp));
  return 0;
}

static struct vnodeopv_entry_desc ext2_vnopv_entry_desc_list[] =
  {
    {&vnop_default_desc, (int (*) (void *)) ext2_vnop_default},
    {&vnop_lookup_desc, (int (*) (void *)) ext2_vnop_lookup},
    {&vnop_open_desc, (int (*) (void *)) ext2_vnop_open},
    {&vnop_close_desc, (int (*) (void *)) ext2_vnop_close},
    {&vnop_getattr_desc, (int (*) (void *)) ext2_vnop_getattr},
    {&vnop_readdir_desc, (int (*) (void *)) ext2_vnop_readdir},
    {&vnop_reclaim_desc, (int (*) (void *)) ext2_vnop_reclaim},
    {NULL, NULL}
  };

static struct vnodeopv_desc ext2_vnopv_desc =
  {
    &ext2_vnop_p,
    ext2_vnopv_entry_desc_list
  };

struct vnodeopv_desc *ext2_vnopv_desc_list[1] =
  {
    &ext2_vnopv_desc
  };

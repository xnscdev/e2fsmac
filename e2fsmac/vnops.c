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

struct ext2_readdir_private
{
  uio_t uio;
  int num;
  int stopped;
  off_t start;
  struct ext2_super_block *super;
};

int (**ext2_vnop_p) (void *);

#define EXT4_EPOCH_BITS         2
#define EXT4_EPOCH_MASK         ((1 << EXT4_EPOCH_BITS) - 1)
#define EXT4_NSEC_MASK          (~0UL << EXT4_EPOCH_BITS)

static inline uint32_t
ext4_encode_extra_time (const struct timespec *tp)
{
  uint32_t extra = sizeof tp->tv_sec > 4 ?
    ((tp->tv_sec - (int) tp->tv_sec) >> 32) & EXT4_EPOCH_MASK : 0;
  return extra | (tp->tv_nsec << EXT4_EPOCH_BITS);
}

static inline void
ext4_decode_extra_time (struct timespec *tp, uint32_t extra)
{
  if (sizeof tp->tv_sec > 4 && (extra & EXT4_EPOCH_MASK))
    {
      uint64_t extra_bits = extra & EXT4_EPOCH_MASK;
      tp->tv_sec += extra_bits << 32;
    }
  tp->tv_nsec = (extra & EXT4_NSEC_MASK) >> EXT4_EPOCH_BITS;
}

#define EXT4_FITS_IN_INODE(inode, field)				\
  (offsetof (typeof (*(inode)), field) + sizeof ((inode)->field)	\
   <= (size_t) EXT2_GOOD_OLD_INODE_SIZE + (inode)->i_extra_isize)

#define EXT4_INODE_GET_XTIME(xtime, timespec, raw_inode)		\
  do									\
    {									\
      (timespec)->tv_sec = (raw_inode)->xtime;				\
      if (EXT4_FITS_IN_INODE (raw_inode, xtime ## _extra))		\
	ext4_decode_extra_time (timespec, (raw_inode)->xtime ## _extra); \
      else								\
	(timespec)->tv_nsec = 0;					\
    }									\
  while (0)

#define EXT4_INODE_SET_XTIME(xtime, timespec, raw_inode)		\
  do									\
    {									\
      (raw_inode)->xtime = (timespec)->tv_sec;				\
      if (EXT4_FITS_IN_INODE (raw_inode, xtime ## _extra))		\
	(raw_inode)->xtime ## _extra = ext4_encode_extra_time (timespec); \
    }									\
  while (0)

static int
update_large_inode (ext2_filsys fs, struct ext2_fsnode *fsnode,
		    struct ext2_inode_large *inode)
{
  int ret;
  ret = ext2fs_write_inode_full (fs, fsnode->ino, (struct ext2_inode *) inode,
				 sizeof *inode);
  if (ret)
    return ret;
  memcpy (fsnode->inode, inode, sizeof *fsnode->inode);
  return 0;
}

static int
update_atime (ext2_filsys fs, struct ext2_fsnode *fsnode)
{
  struct ext2_inode_large inode;
  struct ext2_inode_large *pinode;
  struct timespec atime;
  struct timespec mtime;
  struct timespec now;
  int ret;

  if (!(fs->flags & EXT2_FLAG_RW))
    return 0;
  memset (&inode, 0, sizeof inode);
  ret = ext2fs_read_inode_full (fs, fsnode->ino, (struct ext2_inode *) &inode,
				sizeof inode);
  if (ret)
    return ret;

  pinode = &inode;
  EXT4_INODE_GET_XTIME (i_atime, &atime, pinode);
  EXT4_INODE_GET_XTIME (i_mtime, &mtime, pinode);
  nanotime (&now);

  if (atime.tv_sec >= mtime.tv_sec && atime.tv_sec >= now.tv_sec - 30)
    return 0;
  EXT4_INODE_SET_XTIME (i_atime, &now, pinode);
  ret = update_large_inode (fs, fsnode, pinode);
  if (ret)
    return ret;
  return 0;
}

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

static int
ext2_readdir_process (struct ext2_dir_entry *dirent, int offset, int blocksize,
		      char *buffer, void *data)
{
  struct ext2_readdir_private *priv = data;
  struct dirent di;
  int ret;
  int file_type = DT_UNKNOWN;

  uio_setoffset (priv->uio, offset);
  if (offset < priv->start)
    return 0;

  di.d_fileno = dirent->inode;
  di.d_reclen = sizeof di;

  if (ext2fs_has_feature_filetype (priv->super))
    {
      switch (ext2fs_dirent_file_type (dirent))
	{
	case EXT2_FT_REG_FILE:
	  file_type = DT_REG;
	  break;
	case EXT2_FT_DIR:
	  file_type = DT_DIR;
	  break;
	case EXT2_FT_CHRDEV:
	  file_type = DT_CHR;
	  break;
	case EXT2_FT_BLKDEV:
	  file_type = DT_BLK;
	  break;
	case EXT2_FT_FIFO:
	  file_type = DT_FIFO;
	  break;
	case EXT2_FT_SOCK:
	  file_type = DT_SOCK;
	  break;
	case EXT2_FT_SYMLINK:
	  file_type = DT_LNK;
	  break;
	}
    }
  di.d_type = file_type;

  di.d_namlen = ext2fs_dirent_name_len (dirent);
  strlcpy (di.d_name, dirent->name, di.d_namlen + 1);

  ret = uiomove_atomic (&di, sizeof di, priv->uio);
  if (ret == ENOBUFS)
    {
      priv->stopped = 1;
      return DIRENT_ABORT;
    }
  if (ret)
    return DIRENT_ERROR | DIRENT_ABORT;

  priv->num++;
  log_debug ("readdir: entry #%d: %s, offset: %d",
	     priv->num, di.d_name, offset);
  return 0;
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
      ret = ext2fs_lookup (emp->fs, fsnode->ino, name, cnp->cn_namelen,
			   NULL, &ino);
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
  struct timespec create_time;
  struct timespec access_time;
  struct timespec modify_time;
  int ret = 0;

  if (EXT2_INODE_SIZE (emp->fs->super) > EXT2_GOOD_OLD_INODE_SIZE)
    {
      struct ext2_inode_large inode;
      struct ext2_inode_large *pinode;
      memset (&inode, 0, sizeof inode);
      ret = ext2fs_read_inode_full (emp->fs, fsnode->ino,
				    (struct ext2_inode *) &inode, sizeof inode);
      if (ret)
	return ret;
      pinode = &inode;
      EXT4_INODE_GET_XTIME (i_ctime, &create_time, pinode);
      EXT4_INODE_GET_XTIME (i_atime, &access_time, pinode);
      EXT4_INODE_GET_XTIME (i_mtime, &modify_time, pinode);
    }
  else
    {
      create_time.tv_sec = fsnode->inode->i_ctime;
      access_time.tv_sec = fsnode->inode->i_atime;
      modify_time.tv_sec = fsnode->inode->i_mtime;
      create_time.tv_nsec = access_time.tv_nsec = modify_time.tv_nsec = 0;
    }

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
  return ret;
}

static int
ext2_vnop_readdir (struct vnop_readdir_args *args)
{
  static int known_flags =
    VNODE_READDIR_EXTENDED
    | VNODE_READDIR_REQSEEKOFF
    | VNODE_READDIR_SEEKOFF32
    | VNODE_READDIR_NAMEMAX;
  int ret = 0;
  vnode_t vp = args->a_vp;
  uio_t uio = args->a_uio;
  int flags = args->a_flags;
  int *eofflag = args->a_eofflag;
  int *numdirent = args->a_numdirent;
  struct ext2_mount *emp = vfs_fsprivate (vnode_mount (vp));
  struct ext2_fsnode *fsnode = vnode_fsnode (vp);
  struct ext2_readdir_private priv;

  if (flags & known_flags)
    {
      ret = EINVAL;
      log_debug ("found NFS-exported readdir flags %#x: errno %d",
		 flags & known_flags, ret);
      return ret;
    }

  priv.uio = uio;
  priv.num = 0;
  priv.stopped = 0;
  priv.start = uio_offset (uio);
  priv.super = emp->fs->super;

  ret = ext2fs_dir_iterate (emp->fs, fsnode->ino, 0, NULL,
			    ext2_readdir_process, &priv);
  if (ret)
    return ret;

  if (emp->fs->flags & EXT2_FLAG_RW)
    {
      ret = update_atime (emp->fs, fsnode);
      if (ret)
	return ret;
    }

  if (eofflag)
    *eofflag = !priv.stopped;
  if (numdirent)
    *numdirent = priv.num;

  log_debug ("readdir: vnode: %#x, eofflag: %d, numdirent: %d",
	     vnode_vid (vp), !priv.stopped, priv.num);
  return ret;
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

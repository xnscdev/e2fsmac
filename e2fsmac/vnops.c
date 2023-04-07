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
  int err;
  if (unlikely (size > INT_MAX))
    err = ERANGE;
  else if (unlikely (size > (user_size_t) uio_resid (uio)))
    err = ENOBUFS;
  else
    err = uiomove (addr, (int) size, uio);

  if (err)
    log ("uiomove_atomic(): size: %zu, resid: %lld, errno: %d",
	 size, uio_resid (uio), err);
  return err;
}

static void
ext2_assert_valid_vnode (vnode_t vp)
{
#ifdef DEBUG
  struct ext2_mount *emp = vfs_fsprivate (vnode_mount (vp));
  int valid;
  lck_mtx_lock (emp->mtx_root);
  valid = vp == emp->rootvp;
  lck_mtx_unlock (emp->mtx_root);
  kassert (valid);
#endif
}

static void
ext2_detach_root_vnode (struct ext2_mount *emp, vnode_t vp)
{
  int err;
  lck_mtx_lock (emp->mtx_root);
  if (emp->attach_root)
    kassert (!emp->rootvp);
  if (emp->rootvp)
    {
      kassert (emp->rootvp == vp);
      err = vnode_removefsref (emp->rootvp);
      kassert (!err);
      emp->rootvp = NULL;
    }
  lck_mtx_unlock (emp->mtx_root);
}

static int ext2_vnop_default (struct vnop_generic_args *args)
{
  return ENOTSUP;
}

static int ext2_vnop_lookup (struct vnop_lookup_args *args)
{
  errno_t err;
  vnode_t dvp = args->a_dvp;
  vnode_t *vpp = args->a_vpp;
  struct componentname *cnp = args->a_cnp;
  vnode_t vp = NULL;
  kassert (vnode_isdir (dvp));

  if ((cnp->cn_flags & ISDOTDOT) || !strcmp (cnp->cn_nameptr, "."))
    {
      err = vnode_get (dvp);
      if (!err)
	vp = dvp;
    }
  else
    err = ENOENT;

  *vpp = vp;
  return err;
}

static int
ext2_vnop_open (struct vnop_open_args *args)
{
  vnode_t vp = args->a_vp;
  log_debug ("ext2 open: vnode %#x", vnode_vid (vp));
  return 0;
}

static int
ext2_vnop_close (struct vnop_close_args *args)
{
  vnode_t vp = args->a_vp;
  log_debug ("ext2 close: vnode %#x", vnode_vid (vp));
  return 0;
}

static int
ext2_vnop_getattr (struct vnop_getattr_args *args)
{
  vnode_t vp = args->a_vp;
  struct vnode_attr *vap = args->a_vap;
  struct ext2_mount *emp = vfs_fsprivate (vnode_mount (vp));

  VATTR_RETURN (vap, va_rdev, 0);
  VATTR_RETURN (vap, va_nlink, 2);
  VATTR_RETURN (vap, va_data_size, sizeof (struct dirent) << 1);
  VATTR_RETURN (vap, va_mode, S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP
		| S_IROTH | S_IXOTH);
  VATTR_RETURN (vap, va_create_time, emp->attr.f_create_time);
  VATTR_RETURN (vap, va_access_time, emp->attr.f_access_time);
  VATTR_RETURN (vap, va_modify_time, emp->attr.f_modify_time);
  VATTR_RETURN (vap, va_change_time, emp->attr.f_modify_time);
  VATTR_RETURN (vap, va_fileid, 2);
  VATTR_RETURN (vap, va_fsid, emp->devid);
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

  log_debug ("ext2 readdir: eofflag: %d, numdirent: %d", eof, num);

 err0:
  return err;
}

static int
ext2_vnop_reclaim (struct vnop_reclaim_args *args)
{
  vnode_t vp = args->a_vp;
  struct ext2_mount *emp = vfs_fsprivate (vnode_mount (vp));
  ext2_detach_root_vnode (emp, vp);
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

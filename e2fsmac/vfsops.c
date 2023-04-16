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

#include <sys/kauth.h>
#include <sys/systm.h>
#include "e2fsmac.h"
#include "ext2fs.h"

static int ext2_vfsop_unmount (struct mount *mp, int flags, vfs_context_t ctx);

static void
ext2_init_volattrs (struct vfs_attr *vfs_attr)
{
  vol_capabilities_attr_t *cap = &vfs_attr->f_capabilities;
  vol_attributes_attr_t *attr = &vfs_attr->f_attributes;
  cap->capabilities[VOL_CAPABILITIES_FORMAT] =
    VOL_CAP_FMT_NO_ROOT_TIMES
    | VOL_CAP_FMT_CASE_SENSITIVE
    | VOL_CAP_FMT_CASE_PRESERVING
    | VOL_CAP_FMT_FAST_STATFS
    | VOL_CAP_FMT_2TB_FILESIZE
    | VOL_CAP_FMT_NO_PERMISSIONS;

  cap->valid[VOL_CAPABILITIES_FORMAT] = (__typeof (*(cap->valid))) -1;

  cap->capabilities[VOL_CAPABILITIES_INTERFACES] = VOL_CAP_INT_ATTRLIST;
  cap->valid[VOL_CAPABILITIES_INTERFACES] = (__typeof (*(cap->valid))) -1;

  attr->validattr.commonattr =
    ATTR_CMN_NAME
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

  attr->validattr.dirattr = 0;

  attr->validattr.fileattr =
    ATTR_FILE_LINKCOUNT
    | ATTR_FILE_TOTALSIZE
    | ATTR_FILE_IOBLOCKSIZE
    | ATTR_FILE_DATALENGTH
    | ATTR_FILE_DATAALLOCSIZE;

  attr->validattr.forkattr = 0;

  attr->validattr.volattr =
    ATTR_VOL_FSTYPE
    | ATTR_VOL_SIZE
    | ATTR_VOL_SPACEFREE
    | ATTR_VOL_SPACEAVAIL
    | ATTR_VOL_IOBLOCKSIZE
    | ATTR_VOL_MOUNTPOINT
    | ATTR_VOL_NAME
    | ATTR_VOL_MOUNTFLAGS
    | ATTR_VOL_MOUNTEDDEVICE
    | ATTR_VOL_CAPABILITIES
    | ATTR_VOL_UUID
    | ATTR_VOL_ATTRIBUTES;

  bcopy (&attr->validattr, &attr->nativeattr, sizeof attr->validattr);
}

static int
ext2_get_root_vnode (struct ext2_mount *emp, vnode_t *vpp)
{
  int ret;
  vnode_t vp = NULL;
  uint32_t vid;

  lck_mtx_lock (emp->mtx_root);

  do
    {
      kassert (!vp);
      lck_mtx_assert (emp->mtx_root, LCK_MTX_ASSERT_OWNED);

      if (emp->attach_root)
	{
	  emp->wait_root = 1;
	  msleep (&emp->rootvp, emp->mtx_root, PINOD, NULL, NULL);
	  kassert (!emp->wait_root);
	  ret = EAGAIN;
	}
      else if (emp->rootvp == NULLVP)
	{
	  struct ext2_fsnode *fsnode;
	  emp->attach_root = 1;
	  lck_mtx_unlock (emp->mtx_root);

	  ret = ext2_create_vnode (emp, EXT2_ROOT_INO, NULL, &vp);
	  if (!ret)
	    {
	      kassert (vp);
	      log_debug ("ext2_create_vnode() ok: vid %#x", vnode_vid (vp));
	    }
	  else
	    {
	      kassert (!vp);
	      log ("ext2_create_vnode(): vid %#x, errno %d",
		   vnode_vid (vp), ret);
	      return ret;
	    }

	  fsnode = vnode_fsnode (vp);
	  kassert (fsnode);
	  if (!fsnode->file)
	    {
	      ret = ext2_open_vnode (emp, vp, 0);
	      if (!ret)
		log_debug ("ext2_open_vnode() ok");
	      else
		{
		  log ("ext2_open_vnode(): errno %d", ret);
		  return ret;
		}
	    }

	  lck_mtx_lock (emp->mtx_root);
	  if (!ret)
	    {
	      int ret2;
	      kassert (!emp->rootvp);
	      emp->rootvp = vp;
	      ret2 = vnode_addfsref (vp);
	      kassert (!ret2);
	      kassert (emp->attach_root);
	      emp->attach_root = 0;
	      if (emp->wait_root)
		{
		  emp->wait_root = 0;
		  wakeup (&emp->rootvp);
		}
	    }
	}
      else
	{
	  vp = emp->rootvp;
	  kassert (vp);
	  vid = vnode_vid (vp);
	  lck_mtx_unlock (emp->mtx_root);

	  ret = vnode_getwithvid (vp, vid);
	  if (ret)
	    {
	      log ("vnode_getwithvid(): errno %d", ret);
	      vp = NULL;
	      ret = EAGAIN;
	    }

	  lck_mtx_lock (emp->mtx_root);
	}
    }
  while (ret == EAGAIN);

  lck_mtx_unlock (emp->mtx_root);
  if (!ret)
    *vpp = vp;
  return ret;
}

static int
ext2_vfsop_mount (struct mount *mp, vnode_t devvp, user_addr_t data,
		  vfs_context_t ctx)
{
  int ret;
  struct ext2_args args;
  struct ext2_mount *emp;
  struct vfsstatfs *st;
  kauth_cred_t cred;
  int flags;
  int mp_flags;

  if (vfs_isupdate (mp) || vfs_iswriteupgrade (mp))
    {
      ret = ENOTSUP;
      log ("update mounting is unsupported");
      goto err0;
    }

  ret = copyin (data, &args, sizeof args);
  if (ret)
    {
      log ("copyin(): errno %d", ret);
      goto err0;
    }

  if (args.magic != EXT2_ARGS_MAGIC)
    {
      ret = EINVAL;
      log ("bad mount magic number: %#x", args.magic);
      goto err0;
    }

  flags = 0;
  if (!args.readonly)
    flags |= EXT2_FLAG_RW;
  mp_flags = MNT_NOSUID | MNT_NODEV;
  if (args.readonly)
    mp_flags |= MNT_RDONLY;

  emp = e2fsmac_malloc (sizeof *emp, M_ZERO);
  if (unlikely (!emp))
    {
      ret = ENOMEM;
      log ("e2fsmac_malloc(): errno %d", ret);
      goto err0;
    }

  vfs_setfsprivate (mp, emp);

  ret = vnode_ref (devvp);
  if (ret)
    {
      log ("vnode_ref(): errno %d", ret);
      goto err0;
    }
  emp->devvp = devvp;
  emp->devid = vnode_specrdev (devvp);

  emp->mtx_root = lck_mtx_alloc_init (ext2_lck_grp, NULL);
  if (unlikely (!emp->mtx_root))
    {
      ret = ENOMEM;
      log ("lck_mtx_alloc_init(): errno %d", ret);
      goto err0;
    }

  emp->magic = EXT2_ARGS_MAGIC;
  emp->mp = mp;

  kassert (!emp->attach_root);
  kassert (!emp->wait_root);
  kassert (!emp->rootvp);

  ret = ext2fs_open (emp->devvp, flags, 0, 0, default_io_manager, &emp->fs);
  if (ret)
    {
      log ("ext2fs_open(): errno %d", ret);
      goto err0;
    }

  if (flags & EXT2_FLAG_RW)
    {
      emp->fs->super->s_mtime = get_time ();
      ext2fs_mark_super_dirty (emp->fs);
    }

  st = vfs_statfs (mp);
  kassert (st);
  kassert (!strcmp (st->f_fstypename, "ext2"));

  cred = vfs_context_ucred (ctx);
  kassert (cred);
  emp->uid = kauth_cred_getuid (cred);
  emp->gid = kauth_cred_getgid (cred);

  vfs_setflags (mp, mp_flags);
  log ("mount: devid: %#x", emp->devid);
  return 0;

 err0:
  kassert (!ext2_vfsop_unmount (mp, MNT_FORCE, ctx));
  return TRANSLATE_ERROR (emp->fs, ret, EXT2_ROOT_INO);
}

static int
ext2_vfsop_start (struct mount *mp, int flags, vfs_context_t ctx)
{
  log_debug ("start: flags: %#x", flags);
  return 0;
}

static int
ext2_vfsop_unmount (struct mount *mp, int flags, vfs_context_t ctx)
{
  int ret;
  int flush_flags;
  struct ext2_mount *emp;

  flush_flags = (flags & MNT_FORCE) ? FORCECLOSE : 0;
  ret = vflush (mp, NULL, flush_flags);
  if (ret)
    {
      log_debug ("vflush(): errno %d", ret);
      goto err0;
    }

  emp = vfs_fsprivate (mp);
  if (!emp)
    goto err0;

  log_debug ("unmount: emp exists");

  if (emp->fs)
    {
      ret = ext2fs_close (emp->fs);
      if (ret)
	goto err0;
      emp->fs = NULL;
    }

  if (emp->devvp)
    {
      vnode_rele (emp->devvp);
      emp->devvp = NULL;
      emp->devid = 0;
    }

  kassert (!emp->attach_root);
  kassert (!emp->wait_root);
  kassert (!emp->rootvp);

  if (emp->mtx_root)
    lck_mtx_free (emp->mtx_root, ext2_lck_grp);

  emp->magic = 0;
  log ("unmount: emp: %p", emp);
  e2fsmac_free (emp);
  return 0;

 err0:
  return TRANSLATE_ERROR (emp->fs, ret, EXT2_ROOT_INO);
}

static int
ext2_vfsop_root (struct mount *mp, vnode_t *vpp, vfs_context_t ctx)
{
  int ret;
  vnode_t vp = NULL;
  struct ext2_mount *emp;

  emp = vfs_fsprivate (mp);
  ret = ext2_get_root_vnode (emp, &vp);
  *vpp = vp;
  if (!ret)
    log_debug ("root: vnode: %#x", vnode_vid (vp));
  return TRANSLATE_ERROR (emp->fs, ret, EXT2_ROOT_INO);
}

static int
ext2_vfsop_getattr (struct mount *mp, struct vfs_attr *attr, vfs_context_t ctx)
{
  struct ext2_mount *emp = vfs_fsprivate (mp);
  struct ext2_super_block *super = emp->fs->super;
  struct timespec create_time = { 0, 0 };
  struct timespec modify_time = { 0, 0 };
  struct timespec access_time = { 0, 0 };
  fsid_t fsid;
  blk64_t blocks_cnt;
  blk64_t free_cnt;
  blk64_t res_cnt;
  blk64_t avail_cnt;

  ext2_init_volattrs (attr);

  fsid.val[0] = emp->devid;
  fsid.val[1] = vfs_typenum (emp->mp);

  blocks_cnt = ext2fs_blocks_count (super);
  free_cnt = ext2fs_free_blocks_count (super);
  res_cnt = ext2fs_r_blocks_count (super);
  avail_cnt = free_cnt >= res_cnt ? free_cnt - res_cnt : 0;

  if (super->s_rev_level == EXT2_DYNAMIC_REV)
    {
      create_time.tv_sec = super->s_mkfs_time;
      VFSATTR_RETURN (attr, f_create_time, create_time);
    }
  modify_time.tv_sec = super->s_wtime;
  access_time.tv_sec = super->s_mtime;

  VFSATTR_RETURN (attr, f_bsize, emp->fs->blocksize);
  VFSATTR_RETURN (attr, f_iosize, emp->fs->blocksize);
  VFSATTR_RETURN (attr, f_blocks, blocks_cnt);
  VFSATTR_RETURN (attr, f_bfree, free_cnt);
  VFSATTR_RETURN (attr, f_bavail, avail_cnt);
  VFSATTR_RETURN (attr, f_bused, blocks_cnt - free_cnt);
  VFSATTR_RETURN (attr, f_files, super->s_inodes_count);
  VFSATTR_RETURN (attr, f_ffree, super->s_free_inodes_count);
  VFSATTR_RETURN (attr, f_fsid, fsid);
  VFSATTR_RETURN (attr, f_owner, emp->uid);
  VFSATTR_RETURN (attr, f_modify_time, modify_time);
  VFSATTR_RETURN (attr, f_access_time, access_time);
  VFSATTR_RETURN (attr, f_fssubtype, 0);

  if (VFSATTR_IS_ACTIVE (attr, f_vol_name))
    {
      strncpy (attr->f_vol_name, (char *) super->s_volume_name, EXT2_LABEL_LEN);
      VFSATTR_SET_SUPPORTED (attr, f_vol_name);
    }

  if (VFSATTR_IS_ACTIVE (attr, f_uuid))
    {
      memcpy (attr->f_uuid, super->s_uuid, sizeof attr->f_uuid);
      VFSATTR_SET_SUPPORTED (attr, f_uuid);
    }
  log_debug ("getattr: emp: %p", emp);
  return 0;
}

static int
ext2_vfsop_sync (struct mount *mp, int waitfor, vfs_context_t ctx)
{
  int ret;
  struct ext2_mount *emp = vfs_fsprivate (mp);
  kassert (emp);

  ret = ext2fs_flush (emp->fs);
  return TRANSLATE_ERROR (emp->fs, ret, EXT2_ROOT_INO);
}

struct vfsops ext2_vfsops =
  {
    .vfs_mount = ext2_vfsop_mount,
    .vfs_start = ext2_vfsop_start,
    .vfs_unmount = ext2_vfsop_unmount,
    .vfs_root = ext2_vfsop_root,
    .vfs_getattr = ext2_vfsop_getattr,
    .vfs_sync = ext2_vfsop_sync
  };

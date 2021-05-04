/* e2fsmac.h -- This file is part of e2fsmac.
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

#ifndef _E2FSMAC_H
#define _E2FSMAC_H

#include <kern/debug.h>
#include <libkern/libkern.h>
#include <libkern/locks.h>
#include <mach/mach_types.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#define E2FS_VFS_FLAGS (VFS_TBLTHREADSAFE | \
			VFS_TBLFSNODELOCK | \
			VFS_TBLNOTYPENUM  | \
			VFS_TBLLOCALVOL   | \
			VFS_TBL64BITREADY)

#define E2FS_LCK_GRP_NAME KEXT_BUNDLEID ".lock"

#define EXT2_NAME           "ext2"
#define EXT2_VOLNAME_MAXLEN 16

#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)

#define log(fmt, ...) printf (KEXT_NAME ": " fmt "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define log_debug(fmt, ...) \
  printf (KEXTNAME ": " fmt " (%s:%d)\n", ##__VA_ARGS__, __func__, __LINE__)
#else
#define log_debug(fmt, ...) ((void) 0)
#endif

#define kassert(x) (x) ? (void) 0 : panic ("Assertion failed: " #x)

struct ext2_super_block
{
  uint32_t s_inodes_count;
  uint32_t s_blocks_count;
  uint32_t s_r_blocks_count;
  uint32_t s_free_blocks_count;
  uint32_t s_free_inodes_count;
  uint32_t s_first_data_block;
  uint32_t s_log_block_size;
  uint32_t s_log_frag_size;
  uint32_t s_blocks_per_group;
  uint32_t s_frags_per_group;
  uint32_t s_inodes_per_group;
  uint32_t s_mtime;
  uint32_t s_wtime;
  uint16_t s_mnt_count;
  uint16_t s_max_mnt_count;
  uint16_t s_magic;
  uint16_t s_state;
  uint16_t s_errors;
  uint16_t s_minor_rev_level;
  uint32_t s_lastcheck;
  uint32_t s_checkinterval;
  uint32_t s_creator_os;
  uint32_t s_rev_level;
  uint16_t s_def_resuid;
  uint16_t s_def_resgid;
  uint32_t s_first_ino;
  uint16_t s_inode_size;
  uint16_t s_block_group_nr;
  uint32_t s_feature_compat;
  uint32_t s_feature_incompat;
  uint32_t s_feature_ro_compat;
  uint8_t s_uuid[16];
  unsigned char s_volume_name[EXT2_VOLNAME_MAXLEN];
  unsigned char s_last_mounted[64];
  uint32_t s_algo_bitmap;
  uint8_t s_prealloc_blocks;
  uint8_t s_prealloc_dir_blocks;
  uint8_t s_padding1[2];
  uint8_t s_journal_uuid[16];
  uint32_t s_journal_inum;
  uint32_t s_journal_dev;
  uint32_t s_last_orphan;
  uint32_t s_hash_seed[4];
  uint8_t s_def_hash_version;
  uint8_t s_padding2[3];
  uint32_t s_default_mount_options;
  uint32_t s_first_meta_bg;
  uint8_t s_padding3[760];
};

struct ext2_block_group
{
  uint32_t bg_block_bitmap;
  uint32_t bg_inode_bitmap;
  uint32_t bg_inode_table;
  uint16_t bg_free_blocks_count;
  uint16_t bg_free_inodes_count;
  uint16_t bg_used_dirs_count;
  uint16_t bg_pad;
  uint8_t bg_reserved[12];
};

struct ext2_inode
{
  uint16_t i_mode;
  uint16_t i_uid;
  uint32_t i_size;
  uint32_t i_atime;
  uint32_t i_ctime;
  uint32_t i_mtime;
  uint32_t i_dtime;
  uint16_t i_gid;
  uint16_t i_links_count;
  uint16_t i_blocks;
  uint32_t i_flags;
  uint32_t i_osd1;
  uint32_t i_block[15];
  uint32_t i_generation;
  uint32_t i_file_acl;
  uint32_t i_dir_acl;
  uint32_t i_faddr;
  uint8_t i_osd2[12];
};

struct ext2_mount
{
  uint32_t magic;
  mount_t mp;
  dev_t devid;
  vnode_t devvp;
  char volname[EXT2_VOLNAME_MAXLEN];
  struct vfs_attr attr;
  lck_mtx_t *mtx_root;
  unsigned char attach_root;
  unsigned char wait_root;
  vnode_t rootvp;
};

struct ext2_args
{
#ifndef KERNEL
  char *fspec;
#endif
  uid_t resuid;
  gid_t resgid;
};

extern lck_grp_t *e2fs_lck_grp;

extern struct vfsops ext2_vfsops;
extern struct vnodeopv_desc *ext2_vnopv_desc_list[1];
extern int (**ext2_vnop_p) (void *);

#endif

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

#include <sys/types.h>
#include <mach/mach_types.h>
#include <sys/malloc.h>
#include <libkern/libkern.h>
#include <libkern/locks.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include "ext2_args.h"

#define EXT2_VOLNAME_MAXLEN 16

#define likely(x)               __builtin_expect (!!(x), 1)
#define unlikely(x)             __builtin_expect (!!(x), 0)

#define log(fmt, ...)           printf ("e2fsmac: " fmt "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define log_debug(fmt, ...)						\
  printf ("e2fsmac: " fmt " (%s:%d)\n", ##__VA_ARGS__, __func__, __LINE__)
#else
#define log_debug(fmt, ...) ((void) 0)
#endif

#define kassert(x) (x) ? (void) 0 : panic ("e2fsmac: assertion failed: " #x \
					   " at %s:%d", __func__, __LINE__)

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
  uid_t resuid;
  gid_t resgid;
};

extern lck_grp_t *ext2_lck_grp;

extern struct vfsops ext2_vfsops;
extern struct vnodeopv_desc *ext2_vnopv_desc_list[1];
extern int (**ext2_vnop_p) (void *);

void *kmalloc (size_t size, int flags);
void *krealloc (void *ptr, size_t old, size_t new, int flags);
void kfree (void *ptr);
#ifdef DEBUG
void kmemassert (void);
#endif

#endif

/* vnops.c -- This file is part of e2fsmac.
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

static struct vnodeopv_entry_desc ext2_vnopv_entry_desc_list[] = {
  {&vnop_default_desc, (int (*) (void *)) vn_default_error},
  {&vnop_lookup_desc, (int (*) (void *)) ext2_lookup},
  {&vnop_open_desc, (int (*) (void *)) ext2_open},
  {&vnop_close_desc, (int (*) (void *)) ext2_close},
  {&vnop_getattr_desc, (int (*) (void *)) ext2_getattr},
  {&vnop_readdir_desc, (int (*) (void *)) ext2_readdir},
  {&vnop_reclaim_desc, (int (*) (void *)) ext2_reclaim},
  {NULL, NULL}
};

static struct vnodeopv_desc ext2_vnopv_desc = {
  &ext2_vnop_p,
  ext2_vnopv_entry_desc_list
};

struct vnodeopv_desc *ext2_vnopv_desc_list[1] = {
  &ext2_vnopv_desc
};

int (**ext2_vnop_p) (void *);

int
ext2_lookup (struct vnop_lookup_args *args)
{
  return ENOTSUP;
}

int
ext2_open (struct vnop_open_args *args)
{
  return ENOTSUP;
}

int
ext2_close (struct vnop_close_args *args)
{
  return ENOTSUP;
}

int
ext2_getattr (struct vnop_getattr_args *args)
{
  return ENOTSUP;
}

int
ext2_readdir (struct vnop_readdir_args *args)
{
  return ENOTSUP;
}

int
ext2_reclaim (struct vnop_reclaim_args *args)
{
  return ENOTSUP;
}

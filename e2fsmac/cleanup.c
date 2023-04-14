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

#include "util.h"

/* Static buffers in the original libext2fs should be explicitly freed */

extern char *__ext2fs_group_desc_buf;
extern void *__ext2fs_zero_blocks2_buf;

void
cleanup (void)
{
  e2fsmac_free (__ext2fs_group_desc_buf);
  e2fsmac_free (__ext2fs_zero_blocks2_buf);
}

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

#ifndef __EXT2_ARGS_H
#define __EXT2_ARGS_H

#include <sys/types.h>

/* According to ChatGPT, this is a totally random number with
   no special meaning. */
#define EXT2_ARGS_MAGIC         0x7afcd982

struct ext2_args
{
#ifndef KERNEL
  char *fspec;
#endif
  int magic;
  int readonly;
};

#endif

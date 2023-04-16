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

int
translate_error (ext2_filsys fs, int err, ext2_ino_t ino, const char *file,
		 int line)
{
  time_t now;
  int ret = err;
  int is_err = 0;

  if (err < EXT2_ET_BASE)
    goto no_translation;

  switch (err)
    {
    case EXT2_ET_NO_MEMORY:
    case EXT2_ET_TDB_ERR_OOM:
      ret = ENOMEM;
      break;
    case EXT2_ET_INVALID_ARGUMENT:
    case EXT2_ET_LLSEEK_FAILED:
      ret = EINVAL;
      break;
    case EXT2_ET_NO_DIRECTORY:
      ret = ENOTDIR;
      break;
    case EXT2_ET_FILE_NOT_FOUND:
      ret = ENOENT;
      break;
    case EXT2_ET_DIR_NO_SPACE:
      is_err = 1;
      /* fallthrough */
    case EXT2_ET_TOOSMALL:
    case EXT2_ET_BLOCK_ALLOC_FAIL:
    case EXT2_ET_INODE_ALLOC_FAIL:
    case EXT2_ET_EA_NO_SPACE:
      ret = ENOSPC;
      break;
    case EXT2_ET_SYMLINK_LOOP:
      ret = EMLINK;
      break;
    case EXT2_ET_FILE_TOO_BIG:
      ret = EFBIG;
      break;
    case EXT2_ET_TDB_ERR_EXISTS:
    case EXT2_ET_FILE_EXISTS:
      ret = EEXIST;
      break;
    case EXT2_ET_MMP_FAILED:
    case EXT2_ET_MMP_FSCK_ON:
      ret = EBUSY;
      break;
    case EXT2_ET_EA_KEY_NOT_FOUND:
      ret = ENOENT;
      break;
    case EXT2_ET_MAGIC_EXT2_FILE:
      ret = EFAULT;
      break;
    case EXT2_ET_UNIMPLEMENTED:
      ret = ENOTSUP;
      break;
    default:
      is_err = 1;
      ret = EIO;
      break;
    }

 no_translation:
  if (!is_err)
    return ret;

  now = get_time ();
  fs->super->s_last_error_time = now;
  fs->super->s_last_error_ino = ino;
  fs->super->s_last_error_line = line;
  fs->super->s_last_error_block = err;
  strncpy ((char *) fs->super->s_last_error_func, file,
	   sizeof fs->super->s_last_error_func);

  if (fs->super->s_first_error_time == 0)
    {
      fs->super->s_first_error_time = now;
      fs->super->s_first_error_ino = ino;
      fs->super->s_first_error_line = line;
      fs->super->s_first_error_block = err;
      strncpy ((char *) fs->super->s_first_error_func, file,
	       sizeof fs->super->s_first_error_func);
    }

  fs->super->s_error_count++;
  ext2fs_mark_super_dirty (fs);
  ext2fs_flush (fs);
  return ret;
}

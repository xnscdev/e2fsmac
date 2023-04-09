/*
 * mkjournal.c --- make a journal for a filesystem
 *
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#include <string.h>
#if HAVE_UNISTD_H
#include <sys/unistd.h>
#endif
#if HAVE_ERRNO_H
#include <sys/errno.h>
#endif
#include <sys/fcntl.h>
#include <sys/time.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

/*
 * Convenience function which zeros out _num_ blocks starting at
 * _blk_.  In case of an error, the details of the error is returned
 * via _ret_blk_ and _ret_count_ if they are non-NULL pointers.
 * Returns 0 on success, and an error code on an error.
 *
 * As a special case, if the first argument is NULL, then it will
 * attempt to free the static zeroizing buffer.  (This is to keep
 * programs that check for memory leaks happy.)
 */
#define MAX_STRIDE_LENGTH (4194304 / (int) fs->blocksize)
errcode_t ext2fs_zero_blocks2(ext2_filsys fs, blk64_t blk, int num,
			      blk64_t *ret_blk, int *ret_count)
{
	int		j, count;
	static void	*buf;
	static int	stride_length;
	errcode_t	retval;

	/* If fs is null, clean up the static buffer and return */
	if (!fs) {
		if (buf) {
			kfree(buf);
			buf = 0;
			stride_length = 0;
		}
		return 0;
	}

	/* Deal with zeroing less than 1 block */
	if (num <= 0)
		return 0;

	/* Try a zero out command, if supported */
	retval = io_channel_zeroout(fs->io, blk, num);
	if (retval == 0)
		return 0;

	/* Allocate the zeroizing buffer if necessary */
	if (num > stride_length && stride_length < MAX_STRIDE_LENGTH) {
		void *p;
		int new_stride = num;

		if (new_stride > MAX_STRIDE_LENGTH)
			new_stride = MAX_STRIDE_LENGTH;
		p = krealloc(buf, fs->blocksize * stride_length, fs->blocksize * new_stride, 0);
		if (!p)
			return EXT2_ET_NO_MEMORY;
		buf = p;
		stride_length = new_stride;
		memset(buf, 0, fs->blocksize * stride_length);
	}
	/* OK, do the write loop */
	j=0;
	while (j < num) {
		if (blk % stride_length) {
			count = stride_length - (blk % stride_length);
			if (count > (num - j))
				count = num - j;
		} else {
			count = num - j;
			if (count > stride_length)
				count = stride_length;
		}
		retval = io_channel_write_blk64(fs->io, blk, count, buf);
		if (retval) {
			if (ret_count)
				*ret_count = count;
			if (ret_blk)
				*ret_blk = blk;
			return retval;
		}
		j += count; blk += count;
	}
	return 0;
}

errcode_t ext2fs_zero_blocks(ext2_filsys fs, blk_t blk, int num,
			     blk_t *ret_blk, int *ret_count)
{
	blk64_t ret_blk2;
	errcode_t retval;

	retval = ext2fs_zero_blocks2(fs, blk, num, &ret_blk2, ret_count);
	if (retval)
		*ret_blk = (blk_t) ret_blk2;
	return retval;
}

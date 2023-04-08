/*
 * xnu_io.c --- This is the I/O manager for XNU kernel. Based from unix_io.c
 * from original libext2fs, rewritten for e2fsmac.
 *
 * Implements a one-block write-through cache.
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *	2002 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "e2fsmac.h"
#include "ext2fs.h"

#define EXT2_CHECK_MAGIC(struct, code)		\
  if ((struct)->magic != (code)) return (code)

struct xnu_cache
{
  char *buf;
  unsigned long long block;
  int access_time;
  unsigned int dirty : 1;
  unsigned int in_use : 1;
  unsigned int write_err : 1;
};

#define CACHE_SIZE              8
#define WRITE_DIRECT_SIZE       4
#define READ_DIRECT_SIZE        4

struct xnu_private_data
{
  int magic;
  vnode_t vp;
  int flags;
  int align;
  int access_time;
  ext2_loff_t offset;
  struct xnu_cache cache[CACHE_SIZE];
  void *bounce;
  struct struct_io_stats io_stats;
};

#define IS_ALIGNED(n, align)					\
  ((((uintptr_t) n) & ((uintptr_t) ((align) - 1))) == 0)

static errcode_t
raw_read_blk (io_channel channel, struct xnu_private_data *data,
	      unsigned long long block, int count, void *bufv)
{
  errcode_t retval;
  ssize_t size;
  ext2_loff_t location;
  int actual = 0;
  unsigned char *buf = bufv;
  ssize_t really_read = 0;
  unsigned long long aligned_blk;
  int align_size;
  int offset;
  off_t llseek_off;

  size = count < 0 ? -count : (ext2_loff_t) count * channel->block_size;
  data->io_stats.bytes_read += size;
  location = (ext2_loff_t) block * channel->block_size + data->offset;

  if (data->flags & IO_FLAG_FORCE_BOUNCE)
    goto bounce_read;

  if (sizeof (off_t) >= sizeof (ext2_loff_t)
      && (channel->align == 0
	  || (IS_ALIGNED (buf, channel->align)
	      && IS_ALIGNED (location, channel->align)
	      && IS_ALIGNED (size, channel->align))))
    {
      actual = vpread (data->vp, buf, size, location);
      if (actual == size)
	return 0;
    }

  if (channel->align == 0
      || (IS_ALIGNED (buf, channel->align)
	  && IS_ALIGNED (location, channel->align)
	  && IS_ALIGNED (size, channel->align)))
    {
      actual = vpread (data->vp, buf, size, location);
      if (actual != size)
	{
	short_read:
	  if (actual < 0)
	    {
	      retval = EIO;
	      actual = 0;
	    }
	  else
	    retval = EXT2_ET_SHORT_READ;
	  goto error;
	}
      goto success;
    }

 bounce_read:
  if (channel->align == 0)
    channel->align = 1;
  if (channel->block_size > channel->align
      && channel->block_size % channel->align == 0)
    align_size = channel->block_size;
  else
    align_size = channel->align;
  aligned_blk = location / align_size;
  offset = location % align_size;
  llseek_off = aligned_blk * align_size;

  while (size > 0)
    {
      actual = vpread (data->vp, data->bounce, align_size, llseek_off);
      if (actual >= 0)
	llseek_off += actual;
      if (actual != align_size)
	{
	  actual = really_read;
	  buf -= really_read;
	  size += really_read;
	  goto short_read;
	}
      if (actual + offset > align_size)
	actual = align_size - offset;
      if (actual > size)
	actual = size;
      memcpy (buf, (char *) data->bounce + offset, actual);

      really_read += actual;
      size -= actual;
      buf += actual;
      offset = 0;
      aligned_blk++;
    }

 success:
  return 0;

 error:
  if (actual >= 0 && actual < size)
    memset ((char *) buf + actual, 0, size - actual);
  if (channel->read_error)
    retval = channel->read_error (channel, block, count, buf, size, actual,
				  retval);
  return retval;
}

#define RAW_WRITE_NO_HANDLER    1

static errcode_t
raw_write_blk (io_channel channel, struct xnu_private_data *data,
	       unsigned long long block, int count, const void *bufv, int flags)
{
  ssize_t size;
  ext2_loff_t location;
  int actual = 0;
  errcode_t retval;
  const unsigned char *buf = bufv;
  unsigned long long aligned_blk;
  int align_size;
  int offset;

  if (count == 1)
    size = channel->block_size;
  else if (count < 0)
    size = -count;
  else
    size = (ext2_loff_t) count * channel->block_size;
  data->io_stats.bytes_written += size;

  location = (ext2_loff_t) block * channel->block_size + data->offset;

  if (data->flags & IO_FLAG_FORCE_BOUNCE)
    goto bounce_write;

  if (sizeof (off_t) >= sizeof (ext2_loff_t)
      && (channel->align == 0
	  || (IS_ALIGNED (buf, channel->align)
	      && IS_ALIGNED (location, channel->align)
	      && IS_ALIGNED (size, channel->align))))
    {
      actual = vpwrite (data->vp, buf, size, location);
      if (actual == size)
	return 0;
    }

  if (channel->align == 0
      || (IS_ALIGNED (buf, channel->align)
	  && IS_ALIGNED (location, channel->align)
	  && IS_ALIGNED (size, channel->align)))
    {
      actual = vpwrite (data->vp, buf, size, location);
      if (actual < 0)
	{
	  retval = EIO;
	  goto error_out;
	}
      if (actual != size)
	{
	short_write:
	  retval = EXT2_ET_SHORT_WRITE;
	  goto error_out;
	}
      return 0;
    }

 bounce_write:
  if (channel->align == 0)
    channel->align = 1;
  if (channel->block_size > channel->align
      && channel->block_size % channel->align == 0)
    align_size = channel->block_size;
  else
    align_size = channel->align;
  aligned_blk = location / align_size;
  offset = location % align_size;

  while (size > 0)
    {
      int actual_w;
      if (size < align_size || offset)
	{
	  actual = vpread (data->vp, data->bounce, align_size,
			   aligned_blk * align_size);
	  if (actual != align_size)
	    {
	      if (actual < 0)
		{
		  retval = EIO;
		  goto error_out;
		}
	      memset ((char *) data->bounce + actual, 0, align_size - actual);
	    }
	}
      actual = size;
      if (actual + offset > align_size)
	actual = align_size - offset;
      if (actual > size)
	actual = size;
      memcpy ((char *) data->bounce + offset, buf, actual);
      actual_w = vpwrite (data->vp, data->bounce, align_size,
			  aligned_blk * align_size);
      if (actual_w < 0)
	{
	  retval = EIO;
	  goto error_out;
	}
      if (actual_w != align_size)
	goto short_write;
      size -= actual;
      buf += actual;
      location += actual;
      aligned_blk++;
      offset = 0;
    }
  return 0;

 error_out:
  if ((flags & RAW_WRITE_NO_HANDLER) == 0 && channel->write_error)
    retval = channel->write_error (channel, block, count, buf, size, actual,
				   retval);
  return retval;
}

static errcode_t
alloc_cache (io_channel channel, struct xnu_private_data *data)
{
  errcode_t retval = 0;
  struct xnu_cache *cache;
  int i;

  data->access_time = 0;
  for (i = 0, cache = data->cache; i < CACHE_SIZE; i++, cache++)
    {
      cache->block = 0;
      cache->access_time = 0;
      cache->dirty = 0;
      cache->in_use = 0;
      if (cache->buf)
	ext2fs_free_mem (&cache->buf);
      retval = io_channel_alloc_buf (channel, 0, &cache->buf);
      if (retval)
	return retval;
    }
  if (channel->align || data->flags & IO_FLAG_FORCE_BOUNCE)
    {
      if (data->bounce)
	ext2fs_free_mem (&data->bounce);
      retval = io_channel_alloc_buf (channel, 0, &data->bounce);
    }
  return retval;
}

static void
free_cache (struct xnu_private_data *data)
{
  struct xnu_cache *cache;
  int i;
  data->access_time = 0;
  for (i = 0, cache = data->cache; i < CACHE_SIZE; i++, cache++)
    {
      cache->block = 0;
      cache->access_time = 0;
      cache->dirty = 0;
      cache->in_use = 0;
      if (cache->buf)
	ext2fs_free_mem (&cache->buf);
    }
  if (data->bounce)
    ext2fs_free_mem (&data->bounce);
}

#ifndef NO_IO_CACHE
/*
 * Try to find a block in the cache.  If the block is not found, and
 * eldest is a non-zero pointer, then fill in eldest with the cache
 * entry to that should be reused.
 */
static struct xnu_cache *
find_cached_block (struct xnu_private_data *data, unsigned long long block,
		   struct xnu_cache **eldest)
{
  struct xnu_cache *cache;
  struct xnu_cache *unused_cache;
  struct xnu_cache *oldest_cache;
  int i;

  unused_cache = oldest_cache = 0;
  for (i = 0, cache = data->cache; i < CACHE_SIZE; i++, cache++)
    {
      if (!cache->in_use)
	{
	  if (!unused_cache)
	    unused_cache = cache;
	  continue;
	}
      if (cache->block == block)
	{
	  cache->access_time = ++data->access_time;
	  return cache;
	}
      if (!oldest_cache ||
	  cache->access_time < oldest_cache->access_time)
	oldest_cache = cache;
    }
  if (eldest)
    *eldest = unused_cache ? unused_cache : oldest_cache;
  return 0;
}

/*
 * Reuse a particular cache entry for another block.
 */
static errcode_t
reuse_cache (io_channel channel, struct xnu_private_data *data,
	     struct xnu_cache *cache, unsigned long long block)
{
  if (cache->dirty && cache->in_use)
    {
      errcode_t retval;
      retval = raw_write_blk (channel, data, cache->block, 1,
			      cache->buf, RAW_WRITE_NO_HANDLER);
      if (retval)
	{
	  cache->write_err = 1;
	  return retval;
	}
    }

  cache->in_use = 1;
  cache->dirty = 0;
  cache->write_err = 0;
  cache->block = block;
  cache->access_time = ++data->access_time;
  return 0;
}

#define FLUSH_INVALIDATE	0x01
#define FLUSH_NOLOCK		0x02

/*
 * Flush all of the blocks in the cache
 */
static errcode_t
flush_cached_blocks (io_channel channel, struct xnu_private_data *data,
		     int flags)
{
  struct xnu_cache *cache;
  errcode_t retval;
  errcode_t retval2 = 0;
  int i;
  int errors_found = 0;

  for (i = 0, cache = data->cache; i < CACHE_SIZE; i++, cache++)
    {
      if (!cache->in_use || !cache->dirty)
	continue;
      retval = raw_write_blk (channel, data, cache->block, 1, cache->buf,
			      RAW_WRITE_NO_HANDLER);
      if (retval)
	{
	  cache->write_err = 1;
	  errors_found = 1;
	  retval2 = retval;
	}
      else
	{
	  cache->dirty = 0;
	  cache->write_err = 0;
	  if (flags & FLUSH_INVALIDATE)
	    cache->in_use = 0;
	}
    }

 retry:
  while (errors_found)
    {
      errors_found = 0;
      for (i = 0, cache = data->cache; i < CACHE_SIZE; i++, cache++)
	{
	  if (!cache->in_use || !cache->write_err)
	    continue;
	  errors_found = 1;
	  if (cache->write_err && channel->write_error)
	    {
	      char *err_buf = NULL;
	      unsigned long long err_block = cache->block;

	      cache->dirty = 0;
	      cache->in_use = 0;
	      cache->write_err = 0;
	      if (io_channel_alloc_buf (channel, 0, &err_buf))
		err_buf = NULL;
	      else
		memcpy (err_buf, cache->buf, channel->block_size);
	      channel->write_error (channel, err_block, 1, err_buf,
				    channel->block_size, -1, retval2);
	      if (err_buf)
		ext2fs_free_mem (&err_buf);
	      goto retry;
	    }
	  else
	    cache->write_err = 0;
	}
    }
  return retval2;
}
#endif /* NO_IO_CACHE */

static errcode_t
xnu_open (vnode_t vp, const char *name, int flags, io_channel *channel)
{
  io_channel io = NULL;
  struct xnu_private_data *data = NULL;
  errcode_t retval;

  retval = ext2fs_get_mem (sizeof (struct struct_io_channel), &io);
  if (retval)
    goto cleanup;
  memset (io, 0, sizeof (struct struct_io_channel));
  io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
  retval = ext2fs_get_mem (sizeof (struct xnu_private_data), &data);
  if (retval)
    goto cleanup;

  io->manager = xnu_io_manager;
  retval = ext2fs_get_mem (strlen (name) + 1, &io->name);
  if (retval)
    goto cleanup;

  strlcpy (io->name, name, strlen (name) + 1);
  io->private_data = data;
  io->block_size = 1024;
  io->read_error = 0;
  io->write_error = 0;
  io->refcount = 1;
  io->flags = 0;

  memset (data, 0, sizeof (struct xnu_private_data));
  data->magic = EXT2_ET_MAGIC_UNIX_IO_CHANNEL;
  data->io_stats.num_fields = 2;
  data->flags = flags;
  data->vp = vp;

  if (vnode_isblk (vp))
    io->flags |= CHANNEL_FLAGS_BLOCK_DEVICE;
  else
    io->flags |= CHANNEL_FLAGS_DISCARD_ZEROES;

  if ((retval = alloc_cache (io, data)))
    goto cleanup;

  *channel = io;
  return 0;

 cleanup:
  if (data)
    {
      free_cache (data);
      ext2fs_free_mem (&data);
    }
  if (io)
    {
      if (io->name)
	ext2fs_free_mem (&io->name);
      ext2fs_free_mem (&io);
    }
  return retval;
}

static errcode_t
xnu_close (io_channel channel)
{
  struct xnu_private_data *data;
  errcode_t retval = 0;

  EXT2_CHECK_MAGIC (channel, EXT2_ET_MAGIC_IO_CHANNEL);
  data = (struct xnu_private_data *) channel->private_data;
  EXT2_CHECK_MAGIC (data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

  if (--channel->refcount > 0)
    return 0;

#ifndef NO_IO_CACHE
  retval = flush_cached_blocks (channel, data, 0);
#endif

  free_cache (data);
  ext2fs_free_mem (&channel->private_data);
  if (channel->name)
    ext2fs_free_mem (&channel->name);
  ext2fs_free_mem (&channel);
  return retval;
}

static errcode_t
xnu_set_blksize (io_channel channel, int blksize)
{
  struct xnu_private_data *data;
  errcode_t retval = 0;

  EXT2_CHECK_MAGIC (channel, EXT2_ET_MAGIC_IO_CHANNEL);
  data = (struct xnu_private_data *) channel->private_data;
  EXT2_CHECK_MAGIC (data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

  if (channel->block_size != blksize)
    {
#ifndef NO_IO_CACHE
      if ((retval = flush_cached_blocks (channel, data, FLUSH_NOLOCK)))
	return retval;
#endif

      channel->block_size = blksize;
      free_cache (data);
      retval = alloc_cache (channel, data);
    }
  return retval;
}

static errcode_t
xnu_read_blk64 (io_channel channel, unsigned long long block,
		int count, void *buf)
{
  struct xnu_private_data *data;
  struct xnu_cache *cache;
  errcode_t retval;
  char *cp;
  int i;
  int j;

  EXT2_CHECK_MAGIC (channel, EXT2_ET_MAGIC_IO_CHANNEL);
  data = (struct xnu_private_data *) channel->private_data;
  EXT2_CHECK_MAGIC (data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

#ifdef NO_IO_CACHE
  return raw_read_blk (channel, data, block, count, buf);
#else
  if (data->flags & IO_FLAG_NOCACHE)
    return raw_read_blk (channel, data, block, count, buf);
  /*
   * If we're doing an odd-sized read or a very large read,
   * flush out the cache and then do a direct read.
   */
  if (count < 0 || count > WRITE_DIRECT_SIZE)
    {
      if ((retval = flush_cached_blocks (channel, data, 0)))
	return retval;
      return raw_read_blk (channel, data, block, count, buf);
    }

  cp = buf;
  while (count > 0)
    {
      /* If it's in the cache, use it! */
      if ((cache = find_cached_block (data, block, NULL)))
	{
#ifdef DEBUG
	  log_debug ("Using cached block %llu\n", block);
#endif
	  memcpy (cp, cache->buf, channel->block_size);
	  count--;
	  block++;
	  cp += channel->block_size;
	  continue;
	}

      /*
       * Find the number of uncached blocks so we can do a
       * single read request
       */
      for (i = 1; i < count; i++)
	{
	  if (find_cached_block (data, block+i, NULL))
	    break;
	}
#ifdef DEBUG
      log_debug ("Reading %d blocks starting at %llu\n", i, block);
#endif
      if ((retval = raw_read_blk (channel, data, block, i, cp)))
	return retval;

      /* Save the results in the cache */
      for (j = 0; j < i; j++)
	{
	  if (!find_cached_block (data, block, &cache))
	    {
	      retval = reuse_cache (channel, data, cache, block);
	      if (retval)
		goto call_write_handler;
	      memcpy (cache->buf, cp, channel->block_size);
	    }
	  count--;
	  block++;
	  cp += channel->block_size;
	}
    }
  return 0;

 call_write_handler:
  if (cache->write_err && channel->write_error)
    {
      char *err_buf = NULL;
      unsigned long long err_block = cache->block;

      cache->dirty = 0;
      cache->in_use = 0;
      cache->write_err = 0;
      if (io_channel_alloc_buf (channel, 0, &err_buf))
	err_buf = NULL;
      else
	memcpy (err_buf, cache->buf, channel->block_size);
      channel->write_error (channel, err_block, 1, err_buf,
			    channel->block_size, -1,
			    retval);
      if (err_buf)
	ext2fs_free_mem (&err_buf);
    }
  return retval;
#endif /* NO_IO_CACHE */
}

static errcode_t
xnu_read_blk (io_channel channel, unsigned long block, int count, void *buf)
{
  return xnu_read_blk64 (channel, block, count, buf);
}

static errcode_t
xnu_write_blk64 (io_channel channel, unsigned long long block, int count,
		 const void *buf)
{
  struct xnu_private_data *data;
  struct xnu_cache *cache;
  struct xnu_cache *reuse;
  errcode_t retval = 0;
  const char *cp;
  int writethrough;

  EXT2_CHECK_MAGIC (channel, EXT2_ET_MAGIC_IO_CHANNEL);
  data = (struct xnu_private_data *) channel->private_data;
  EXT2_CHECK_MAGIC (data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

#ifdef NO_IO_CACHE
  return raw_write_blk (channel, data, block, count, buf, 0);
#else
  if (data->flags & IO_FLAG_NOCACHE)
    return raw_write_blk (channel, data, block, count, buf, 0);
  /*
   * If we're doing an odd-sized write or a very large write,
   * flush out the cache completely and then do a direct write.
   */
  if (count < 0 || count > WRITE_DIRECT_SIZE)
    {
      if ((retval = flush_cached_blocks (channel, data, FLUSH_INVALIDATE)))
	return retval;
      return raw_write_blk (channel, data, block, count, buf, 0);
    }

  /*
   * For a moderate-sized multi-block write, first force a write
   * if we're in write-through cache mode, and then fill the
   * cache with the blocks.
   */
  writethrough = channel->flags & CHANNEL_FLAGS_WRITETHROUGH;
  if (writethrough)
    retval = raw_write_blk (channel, data, block, count, buf, 0);

  cp = buf;
  while (count > 0)
    {
      cache = find_cached_block (data, block, &reuse);
      if (!cache)
	{
	  errcode_t err;
	  cache = reuse;
	  err = reuse_cache (channel, data, cache, block);
	  if (err)
	    goto call_write_handler;
	}
      if (cache->buf != cp)
	memcpy (cache->buf, cp, channel->block_size);
      cache->dirty = !writethrough;
      count--;
      block++;
      cp += channel->block_size;
    }
  return retval;

 call_write_handler:
  if (cache->write_err && channel->write_error)
    {
      char *err_buf = NULL;
      unsigned long long err_block = cache->block;

      cache->dirty = 0;
      cache->in_use = 0;
      cache->write_err = 0;
      if (io_channel_alloc_buf (channel, 0, &err_buf))
	err_buf = NULL;
      else
	memcpy (err_buf, cache->buf, channel->block_size);
      channel->write_error (channel, err_block, 1, err_buf,
			    channel->block_size, -1,
			    retval);
      if (err_buf)
	ext2fs_free_mem (&err_buf);
    }
  return retval;
#endif /* NO_IO_CACHE */
}

static errcode_t
xnu_write_blk (io_channel channel, unsigned long block, int count,
	       const void *buf)
{
  return xnu_write_blk64 (channel, block, count, buf);
}

static errcode_t
xnu_cache_readahead (io_channel channel, unsigned long long block,
		     unsigned long long count)
{
  return EXT2_ET_OP_NOT_SUPPORTED;
}

static errcode_t
xnu_flush (io_channel channel)
{
  struct xnu_private_data *data;
  errcode_t retval = 0;

  EXT2_CHECK_MAGIC (channel, EXT2_ET_MAGIC_IO_CHANNEL);
  data = (struct xnu_private_data *) channel->private_data;
  EXT2_CHECK_MAGIC (data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

#ifndef NO_IO_CACHE
  retval = flush_cached_blocks (channel, data, 0);
#endif
  return retval;
}

static errcode_t
xnu_write_byte (io_channel channel, unsigned long offset, int size,
		const void *buf)
{
  struct xnu_private_data *data;
  errcode_t retval = 0;
  ssize_t actual;

  EXT2_CHECK_MAGIC (channel, EXT2_ET_MAGIC_IO_CHANNEL);
  data = (struct xnu_private_data *) channel->private_data;
  EXT2_CHECK_MAGIC (data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

  if (channel->align != 0)
    {
#ifdef ALIGN_DEBUG
      log_debug ("xnu_write_byte: O_DIRECT fallback\n");
#endif
      return EXT2_ET_UNIMPLEMENTED;
    }

#ifndef NO_IO_CACHE
  /*
   * Flush out the cache completely
   */
  if ((retval = flush_cached_blocks (channel, data, FLUSH_INVALIDATE)))
    return retval;
#endif

  actual = vpwrite (data->vp, buf, size, offset + data->offset);
  if (actual < 0)
    return EIO;
  if (actual != size)
    return EXT2_ET_SHORT_WRITE;
  return 0;
}

static errcode_t
xnu_set_option (io_channel channel, const char *option, const char *arg)
{
  struct xnu_private_data *data;
  unsigned long long tmp;
  errcode_t retval;
  char *end;

  EXT2_CHECK_MAGIC (channel, EXT2_ET_MAGIC_IO_CHANNEL);
  data = (struct xnu_private_data *) channel->private_data;
  EXT2_CHECK_MAGIC (data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

  if (!strcmp (option, "offset"))
    {
      if (!arg)
	return EXT2_ET_INVALID_ARGUMENT;

      tmp = strtoul (arg, &end, 0);
      if (*end)
	return EXT2_ET_INVALID_ARGUMENT;
      data->offset = tmp;
      if (data->offset < 0)
	return EXT2_ET_INVALID_ARGUMENT;
      return 0;
    }
  if (!strcmp (option, "cache"))
    {
      if (!arg)
	return EXT2_ET_INVALID_ARGUMENT;
      if (!strcmp (arg, "on"))
	{
	  data->flags &= ~IO_FLAG_NOCACHE;
	  return 0;
	}
      if (!strcmp (arg, "off"))
	{
	  retval = flush_cached_blocks (channel, data, 0);
	  data->flags |= IO_FLAG_NOCACHE;
	  return retval;
	}
      return EXT2_ET_INVALID_ARGUMENT;
    }
  return EXT2_ET_INVALID_ARGUMENT;
}

static errcode_t
xnu_get_stats (io_channel channel, io_stats *stats)
{
  errcode_t retval = 0;
  struct xnu_private_data *data;

  EXT2_CHECK_MAGIC (channel, EXT2_ET_MAGIC_IO_CHANNEL);
  data = (struct xnu_private_data *) channel->private_data;
  EXT2_CHECK_MAGIC (data, EXT2_ET_MAGIC_UNIX_IO_CHANNEL);

  if (stats)
    *stats = &data->io_stats;
  return retval;
}

static errcode_t
xnu_discard (io_channel channel, unsigned long long block,
	     unsigned long long count)
{
  return EXT2_ET_UNIMPLEMENTED;
}

static errcode_t
xnu_zeroout (io_channel channel, unsigned long long block,
	     unsigned long long count)
{
  return EXT2_ET_UNIMPLEMENTED;
}

static struct struct_io_manager struct_xnu_manager =
  {
    .magic = EXT2_ET_MAGIC_IO_MANAGER,
    .name = "XNU I/O Manager",
    .open = xnu_open,
    .close = xnu_close,
    .set_blksize = xnu_set_blksize,
    .read_blk64 = xnu_read_blk64,
    .read_blk = xnu_read_blk,
    .write_blk64 = xnu_write_blk64,
    .write_blk = xnu_write_blk,
    .cache_readahead = xnu_cache_readahead,
    .flush = xnu_flush,
    .write_byte = xnu_write_byte,
    .set_option = xnu_set_option,
    .get_stats = xnu_get_stats,
    .discard = xnu_discard,
    .zeroout = xnu_zeroout,
  };

io_manager xnu_io_manager = &struct_xnu_manager;

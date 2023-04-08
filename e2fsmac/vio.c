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

/* Implement vnode I/O similar to POSIX */

#include "e2fsmac.h"

ssize_t
vpread (vnode_t vp, void *buffer, size_t len, off_t offset)
{
  uio_t uio = uio_create (1, offset, UIO_SYSSPACE, UIO_READ);
  int ret = uio_addiov (uio, CAST_USER_ADDR_T (buffer), len);
  ssize_t resid;
  if (ret)
    return -1;
  ret = VNOP_READ (vp, uio, 0, vfs_context_current ());
  if (ret)
    return -1;
  resid = uio_resid (uio);
  uio_free (uio);
  return len - resid;
}

ssize_t
vpwrite (vnode_t vp, const void *buffer, size_t len, off_t offset)
{
  uio_t uio = uio_create (1, offset, UIO_SYSSPACE, UIO_WRITE);
  int ret = uio_addiov (uio, CAST_USER_ADDR_T (buffer), len);
  ssize_t resid;
  if (ret)
    return -1;
  ret = VNOP_WRITE (vp, uio, 0, vfs_context_current ());
  if (ret)
    return -1;
  resid = uio_resid (uio);
  uio_free (uio);
  return len - resid;
}

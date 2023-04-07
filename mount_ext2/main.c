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

#include <sys/mount.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ext2_args.h"

static struct option opts[] =
  {
    {"resuid", required_argument, NULL, 'u'},
    {"resgid", required_argument, NULL, 'g'},
    {"version", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'}
  };

static void
usage (void)
{
  fprintf (stderr, "Usage: mount_ext2 [-ug] fspec mp\n"
	   "       mount_ext2 -h\n"
	   "  -u, --resuid        Override UID for reserved blocks/inodes\n"
	   "  -g, --resgid        Override GID for reserved blocks/inodes\n"
	   "  -h, --help          Print help\n"
	   "  fspec               Special device to mount\n"
	   "  mp                  Mount point\n");
}

int
main (int argc, char **argv)
{
  int ch;
  uid_t resuid = 0;
  uid_t resgid = 0;
  char *fspec;
  char *mp;
  char *realmp;
  struct ext2_args args;
  int err;

  while ((ch = getopt_long (argc, argv, "ugh", opts, NULL)) != -1)
    {
      switch (ch)
	{
	case 'u':
	  resuid = (uid_t) strtol (optarg, NULL, 0);
	  break;
	case 'g':
	  resgid = (gid_t) strtol (optarg, NULL, 0);
	  break;
	case 'h':
	  usage ();
	  exit (0);
	default:
	  usage ();
	  exit (1);
	}
    }

  if (argc - optind != 2)
    {
      usage ();
      exit (1);
    }
  fspec = argv[optind];
  mp = argv[optind + 1];

  realmp = realpath (mp, NULL);
  if (!realmp)
    {
      fprintf (stderr, "Failed to determine real path of %s: %s\n",
	       mp, strerror (errno));
      exit (1);
    }

#ifndef KERNEL
  args.fspec = fspec;
#endif
  args.magic = EXT2_ARGS_MAGIC;
  args.resuid = resuid;
  args.resgid = resgid;

  err = mount ("ext2", realmp, 0, &args);
  free (realmp);
  if (err == -1)
    {
      fprintf (stderr, "Failed to mount: %s\n", strerror (errno));
      exit (1);
    }
  return 0;
}

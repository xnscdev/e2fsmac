/*
 * crc32c.c
 *
 * August 26, 2011 Darrick J. Wong <djwong at us.ibm.com>
 * Reuse Bob Pearson's slice-by-8 implementation for e2fsprogs.
 *
 * July 20, 2011 Bob Pearson <rpearson at systemfabricworks.com>
 * added slice by 8 algorithm to the existing conventional and
 * slice by 4 algorithms.
 *
 * Oct 15, 2000 Matt Domsch <Matt_Domsch@dell.com>
 * Nicer crc32 functions/docs submitted by linux@horizon.com.  Thanks!
 * Code was from the public domain, copyright abandoned.  Code was
 * subsequently included in the kernel, thus was re-licensed under the
 * GNU GPL v2.
 *
 * Oct 12, 2000 Matt Domsch <Matt_Domsch@dell.com>
 * Same crc32 function was used in 5 other places in the kernel.
 * I made one version, and deleted the others.
 * There are various incantations of crc32().  Some use a seed of 0 or ~0.
 * Some xor at the end with ~0.  The generic crc32() function takes
 * seed as an argument, and doesn't xor at the end.  Then individual
 * users can do whatever they need.
 *   drivers/net/smc9194.c uses seed ~0, doesn't xor with ~0.
 *   fs/jffs2 uses seed 0, doesn't xor with ~0.
 *   fs/partitions/efi.c uses seed ~0, xor's with ~0.
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */
#include "config.h"
#include <stdint.h>
#define PTR_ALIGN(p, a)		((__typeof__(p))ALIGN((unsigned long)(p), (a)))
#include "crc32c_defs.h"

#include "ext2fs.h"
#ifdef WORDS_BIGENDIAN
#define __constant_cpu_to_le32(x) ___constant_swab32((x))
#define __constant_cpu_to_be32(x) (x)
#define __be32_to_cpu(x) (x)
#define __cpu_to_be32(x) (x)
#define __cpu_to_le32(x) (ext2fs_cpu_to_le32((x)))
#define __le32_to_cpu(x) (ext2fs_le32_to_cpu((x)))
#else
#define __constant_cpu_to_le32(x) (x)
#define __constant_cpu_to_be32(x) ___constant_swab32((x))
#define __be32_to_cpu(x) (ext2fs_be32_to_cpu((x)))
#define __cpu_to_be32(x) (ext2fs_cpu_to_be32((x)))
#define __cpu_to_le32(x) (x)
#define __le32_to_cpu(x) (x)
#endif

#if CRC_LE_BITS > 8
# define tole(x) __constant_cpu_to_le32(x)
#else
# define tole(x) (x)
#endif

#if CRC_BE_BITS > 8
# define tobe(x) __constant_cpu_to_be32(x)
#else
# define tobe(x) (x)
#endif

#include "crc32c_table.h"

#if CRC_LE_BITS > 8 || CRC_BE_BITS > 8

#if CRC_LE_BITS < 64 && CRC_BE_BITS < 64
#define CRC_INLINE inline
#else
#define CRC_INLINE
#endif

/* implements slicing-by-4 or slicing-by-8 algorithm */
static CRC_INLINE uint32_t
crc32_body(uint32_t crc, unsigned char const *buf, size_t len,
	   const uint32_t (*tab)[256])
{
# ifndef WORDS_BIGENDIAN
#  define DO_CRC(x) (crc = t0[(crc ^ (x)) & 255] ^ (crc >> 8))
#  define DO_CRC4 (t3[(q) & 255] ^ t2[(q >> 8) & 255] ^ \
		   t1[(q >> 16) & 255] ^ t0[(q >> 24) & 255])
#  define DO_CRC8 (t7[(q) & 255] ^ t6[(q >> 8) & 255] ^ \
		   t5[(q >> 16) & 255] ^ t4[(q >> 24) & 255])
# else
#  define DO_CRC(x) (crc = t0[((crc >> 24) ^ (x)) & 255] ^ (crc << 8))
#  define DO_CRC4 (t0[(q) & 255] ^ t1[(q >> 8) & 255] ^ \
		   t2[(q >> 16) & 255] ^ t3[(q >> 24) & 255])
#  define DO_CRC8 (t4[(q) & 255] ^ t5[(q >> 8) & 255] ^ \
		   t6[(q >> 16) & 255] ^ t7[(q >> 24) & 255])
# endif
	const uint32_t *b;
	size_t rem_len;
	const uint32_t *t0 = tab[0], *t1 = tab[1], *t2 = tab[2], *t3 = tab[3];
	const uint32_t *t4 = tab[4], *t5 = tab[5], *t6 = tab[6], *t7 = tab[7];
	uint32_t q;

	/* Align it */
	if (unlikely((uintptr_t)buf & 3 && len)) {
		do {
			DO_CRC(*buf++);
		} while ((--len) && ((uintptr_t)buf)&3);
	}

# if CRC_LE_BITS == 32
	rem_len = len & 3;
	len = len >> 2;
# else
	rem_len = len & 7;
	len = len >> 3;
# endif

	b = (const uint32_t *)buf;
	for (--b; len; --len) {
		q = crc ^ *++b; /* use pre increment for speed */
# if CRC_LE_BITS == 32
		crc = DO_CRC4;
# else
		crc = DO_CRC8;
		q = *++b;
		crc ^= DO_CRC4;
# endif
	}
	len = rem_len;
	/* And the last few bytes */
	if (len) {
		const uint8_t *p = (const uint8_t *)(b + 1) - 1;
		do {
			DO_CRC(*++p); /* use pre increment for speed */
		} while (--len);
	}
	return crc;
#undef DO_CRC
#undef DO_CRC4
#undef DO_CRC8
}
#endif

/**
 * crc32_le() - Calculate bitwise little-endian Ethernet AUTODIN II CRC32
 * @crc: seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *	other uses, or the previous crc32 value if computing incrementally.
 * @p: pointer to buffer over which CRC is run
 * @len: length of buffer
 */
static inline uint32_t crc32_le_generic(uint32_t crc, unsigned char const *p,
					size_t len, const uint32_t (*tab)[256],
					uint32_t polynomial EXT2FS_ATTR((unused)))
{
#if CRC_LE_BITS == 1
	int i;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? polynomial : 0);
	}
# elif CRC_LE_BITS == 2
	while (len--) {
		crc ^= *p++;
		crc = (crc >> 2) ^ tab[0][crc & 3];
		crc = (crc >> 2) ^ tab[0][crc & 3];
		crc = (crc >> 2) ^ tab[0][crc & 3];
		crc = (crc >> 2) ^ tab[0][crc & 3];
	}
# elif CRC_LE_BITS == 4
	while (len--) {
		crc ^= *p++;
		crc = (crc >> 4) ^ tab[0][crc & 15];
		crc = (crc >> 4) ^ tab[0][crc & 15];
	}
# elif CRC_LE_BITS == 8
	/* aka Sarwate algorithm */
	while (len--) {
		crc ^= *p++;
		crc = (crc >> 8) ^ tab[0][crc & 255];
	}
# else
	crc = __cpu_to_le32(crc);
	crc = crc32_body(crc, p, len, tab);
	crc = __le32_to_cpu(crc);
#endif
	return crc;
}

uint32_t ext2fs_crc32c_le(uint32_t crc, unsigned char const *p, size_t len)
{
	return crc32_le_generic(crc, p, len, crc32ctable_le, CRC32C_POLY_LE);
}

/**
 * crc32_be() - Calculate bitwise big-endian Ethernet AUTODIN II CRC32
 * @crc: seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *	other uses, or the previous crc32 value if computing incrementally.
 * @p: pointer to buffer over which CRC is run
 * @len: length of buffer
 */
static inline uint32_t crc32_be_generic(uint32_t crc, unsigned char const *p,
					size_t len, const uint32_t (*tab)[256],
					uint32_t polynomial EXT2FS_ATTR((unused)))
{
#if CRC_BE_BITS == 1
	int i;
	while (len--) {
		crc ^= *p++ << 24;
		for (i = 0; i < 8; i++)
			crc =
			    (crc << 1) ^ ((crc & 0x80000000) ? polynomial :
					  0);
	}
# elif CRC_BE_BITS == 2
	while (len--) {
		crc ^= *p++ << 24;
		crc = (crc << 2) ^ tab[0][crc >> 30];
		crc = (crc << 2) ^ tab[0][crc >> 30];
		crc = (crc << 2) ^ tab[0][crc >> 30];
		crc = (crc << 2) ^ tab[0][crc >> 30];
	}
# elif CRC_BE_BITS == 4
	while (len--) {
		crc ^= *p++ << 24;
		crc = (crc << 4) ^ tab[0][crc >> 28];
		crc = (crc << 4) ^ tab[0][crc >> 28];
	}
# elif CRC_BE_BITS == 8
	while (len--) {
		crc ^= *p++ << 24;
		crc = (crc << 8) ^ tab[0][crc >> 24];
	}
# else
	crc = __cpu_to_be32(crc);
	crc = crc32_body(crc, p, len, tab);
	crc = __be32_to_cpu(crc);
# endif
	return crc;
}

uint32_t ext2fs_crc32_be(uint32_t crc, unsigned char const *p, size_t len)
{
	return crc32_be_generic(crc, p, len, crc32table_be, CRCPOLY_BE);
}

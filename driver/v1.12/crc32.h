#ifndef __SONIC_CRC32__
#define __SONIC_CRC32__

#define ENTRIES_PER_LINE 4

#define LE_TABLE_SIZE (1 << CRC_LE_BITS)
#define BE_TABLE_SIZE (1 << CRC_BE_BITS)

#define CRCPOLY_LE 0xedb88320
#define CRCPOLY_BE 0x04c11db7

/* How many bits at a time to use.  Requires a table of 4<<CRC_xx_BITS bytes. */
/* For less performance-sensitive, use 4 */
#ifndef CRC_LE_BITS 
# define CRC_LE_BITS 8
#endif
#ifndef CRC_BE_BITS
# define CRC_BE_BITS 8
#endif

#if !SONIC_KERNEL
#include <stdint.h>
#include <stdio.h>
#else /* SONIC_KERNEL */
#include <linux/types.h>
#endif /* SONIC_KERNEL */

void crc32init_le(void);

inline uint32_t
crc32_body(uint32_t crc, unsigned char const *buf, size_t len, const uint32_t (*tab)[256]);

uint32_t crc32_le(uint32_t crc, unsigned char const *p, size_t len);
#endif /* __SONIC_CRC32__ */

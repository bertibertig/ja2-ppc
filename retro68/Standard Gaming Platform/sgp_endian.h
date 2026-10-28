#ifndef SGP_ENDIAN_H
#define SGP_ENDIAN_H

/* JA2 PPC: every JA2 data file (SLF, STCI, maps, .edt text, save games) is
 * little-endian and was read on x86 as raw structs. Load sites decode
 * fields with these helpers instead; they compile to plain loads on a
 * little-endian host, so tools and host tests share the same code. */

#include "Types.h"

static inline UINT16 SGPReadLE16(const void *source)
{
	const UINT8 *bytes = (const UINT8 *)source;

	return (UINT16)(bytes[0] | (bytes[1] << 8));
}

static inline UINT32 SGPReadLE32(const void *source)
{
	const UINT8 *bytes = (const UINT8 *)source;

	return (UINT32)bytes[0] | ((UINT32)bytes[1] << 8) | ((UINT32)bytes[2] << 16) |
	       ((UINT32)bytes[3] << 24);
}

static inline void SGPWriteLE16(void *destination, UINT16 value)
{
	UINT8 *bytes = (UINT8 *)destination;

	bytes[0] = (UINT8)value;
	bytes[1] = (UINT8)(value >> 8);
}

static inline void SGPWriteLE32(void *destination, UINT32 value)
{
	UINT8 *bytes = (UINT8 *)destination;

	bytes[0] = (UINT8)value;
	bytes[1] = (UINT8)(value >> 8);
	bytes[2] = (UINT8)(value >> 16);
	bytes[3] = (UINT8)(value >> 24);
}

/* In-place conversion of a field that was read raw from a file. */
#if defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define SGP_SWAP_LE16(field) ((field) = (__typeof__(field))SGPReadLE16(&(field)))
#define SGP_SWAP_LE32(field) ((field) = (__typeof__(field))SGPReadLE32(&(field)))
#else
#define SGP_SWAP_LE16(field) ((void)0)
#define SGP_SWAP_LE32(field) ((void)0)
#endif

/* Swaps an array of 16-bit values (e.g. CHAR16 text read from disk). */
static inline void SGPSwapLE16Array(void *data, UINT32 count)
{
#if defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	UINT8 *bytes = (UINT8 *)data;
	UINT32 index;

	for (index = 0; index < count; ++index) {
		UINT8 low = bytes[index * 2];

		bytes[index * 2] = bytes[index * 2 + 1];
		bytes[index * 2 + 1] = low;
	}
#else
	(void)data;
	(void)count;
#endif
}

#endif

#ifndef SGP_BLIT_C_H
#define SGP_BLIT_C_H

/* JA2 PPC: C counterparts of the x86 inline-assembly loops in
 * vobject_blitters.c. Each blitter keeps its original prologue (pointer,
 * clipping and line-skip set-up) and replaces only its __asm block, so
 * these macros use the same control flow as the assembly, including its
 * quirks (e.g. the right-hand clip skip scans raw bytes for the 0 that
 * ends a line).
 *
 * ETRLE run bytes: bit 7 set = (n & 0x7f) transparent pixels, 0 = end of
 * line, otherwise n literal palette indices follow.
 *
 * LITERAL is a statement that may use d_ (UINT16 * destination pixel) and
 * px_ (UINT8 source index); TRANSPARENT may use d_ and n_ (run length) and
 * must not advance d_.
 */

#include "types.h"

void SGPBlitterNotPorted( const char *pName );

#define SGP_BLIT_NOT_PORTED( name ) \
	do { SGPBlitterNotPorted( name ); return( FALSE ); } while( 0 )

#define SGP_ETRLE_UNCLIPPED( pSrc, pDest, uiLines, uiLineSkip, LITERAL, TRANSPARENT ) \
	do \
	{ \
		UINT8 *s_ = (UINT8 *)( pSrc ); \
		UINT16 *d_ = (UINT16 *)( pDest ); \
		UINT32 h_ = (UINT32)( uiLines ); \
		for( ;; ) \
		{ \
			UINT32 n_ = *s_++; \
			if( n_ & 0x80 ) \
			{ \
				n_ &= 0x7f; \
				TRANSPARENT; \
				d_ += n_; \
			} \
			else if( n_ == 0 ) \
			{ \
				if( --h_ == 0 ) \
					break; \
				d_ = (UINT16 *)( (UINT8 *)d_ + ( uiLineSkip ) ); \
			} \
			else \
			{ \
				do \
				{ \
					UINT8 px_ = *s_++; \
					LITERAL; \
					d_++; \
				} while( --n_ ); \
			} \
		} \
	} while( 0 )

#define SGP_ETRLE_CLIPPED( pSrc, pDest, iTopSkip, iLeftSkip, iBlitLength, iBlitHeight, uiLineSkip, LITERAL, TRANSPARENT ) \
	do \
	{ \
		__label__ line_, setup_, dispatch_, literal_, transparent_, rightskip_; \
		UINT8 *s_ = (UINT8 *)( pSrc ); \
		UINT16 *d_ = (UINT16 *)( pDest ); \
		INT32 rows_ = (INT32)( iBlitHeight ); \
		INT32 top_ = (INT32)( iTopSkip ); \
		UINT32 count_, n_, unblitted_; \
		while( top_ > 0 ) \
		{ \
			n_ = *s_++; \
			if( n_ & 0x80 ) \
				continue; \
			if( n_ == 0 ) \
			{ \
				top_--; \
				continue; \
			} \
			s_ += n_; \
		} \
	line_: \
		unblitted_ = 0; \
		count_ = (UINT32)( iLeftSkip ); \
		while( count_ != 0 ) \
		{ \
			n_ = *s_++; \
			if( n_ & 0x80 ) \
			{ \
				n_ &= 0x7f; \
				if( n_ == count_ ) \
					goto setup_; \
				if( n_ < count_ ) \
				{ \
					count_ -= n_; \
					continue; \
				} \
				n_ -= count_; \
				count_ = (UINT32)( iBlitLength ); \
				unblitted_ = 0; \
				goto transparent_; \
			} \
			if( n_ == count_ ) \
			{ \
				s_ += n_; \
				goto setup_; \
			} \
			if( n_ < count_ ) \
			{ \
				s_ += n_; \
				count_ -= n_; \
				continue; \
			} \
			s_ += count_; \
			n_ -= count_; \
			count_ = (UINT32)( iBlitLength ); \
			unblitted_ = 0; \
			goto literal_; \
		} \
	setup_: \
		count_ = (UINT32)( iBlitLength ); \
		unblitted_ = 0; \
	dispatch_: \
		if( count_ == 0 ) \
			goto rightskip_; \
		n_ = *s_++; \
		if( n_ & 0x80 ) \
		{ \
			n_ &= 0x7f; \
			goto transparent_; \
		} \
	literal_: \
		if( n_ > count_ ) \
		{ \
			unblitted_ = n_ - count_; \
			n_ = count_; \
		} \
		count_ -= n_; \
		while( n_ ) \
		{ \
			UINT8 px_ = *s_++; \
			LITERAL; \
			d_++; \
			n_--; \
		} \
		s_ += unblitted_; \
		goto dispatch_; \
	transparent_: \
		if( n_ > count_ ) \
			n_ = count_; \
		count_ -= n_; \
		TRANSPARENT; \
		d_ += n_; \
		goto dispatch_; \
	rightskip_: \
		while( *s_++ != 0 ) \
		{ \
		} \
		if( --rows_ <= 0 ) \
			break; \
		d_ = (UINT16 *)( (UINT8 *)d_ + ( uiLineSkip ) ); \
		goto line_; \
	} while( 0 )

#define SGP_FILL_RUN( value ) \
	do \
	{ \
		UINT32 k_; \
		for( k_ = 0; k_ < n_; k_++ ) \
			d_[ k_ ] = ( value ); \
	} while( 0 )

#endif

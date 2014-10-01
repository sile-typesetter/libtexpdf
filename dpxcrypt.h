/* This is DVIPDFMx, an eXtended version of DVIPDFM by Mark A. Wicks.

    Copyright (C) 2003-2014 by Jin-Hwan Cho and Shunsaku Hirata,
    the dvipdfmx project team.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*/
/**
@file
@brief MD5 functions borrowed from libgcrypt.
*/

#ifndef _DPXCRYPT_H_
#define _DPXCRYPT_H_

#include <stdio.h>
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

/* libgcrypt md5 */
typedef struct {
  uint32_t A,B,C,D; /* chaining variables */
  unsigned long nblocks;
  unsigned char buf[64];
  int count;
} MD5_CONTEXT;

/** Initialize MD5 state. 
Caller provides memory-allocated struct.
*/
void texpdf_MD5_init (MD5_CONTEXT *ctx);
/** Add characters to MD5 digest.
The routine updates the message-digest context to
account for the presence of each of the characters `inBuf[0..inlen-1]`
in the message whose digest is being computed.*/
void texpdf_MD5_write (MD5_CONTEXT *ctx, const unsigned char *inbuf, unsigned long inlen);
/** Terminate message digest computation.
  The routine final terminates the message-digest computation and
  ends with the desired message digest in `ctx->digest[0...15]`.
  The handle is prepared for a new MD5 cycle.
  Returns 16 bytes representing the digest. */
void texpdf_MD5_final (unsigned char *outbuf, MD5_CONTEXT *ctx);

/* libgcrypt arcfour */
typedef struct {
  int idx_i, idx_j;
  unsigned char sbox[256];
} ARC4_KEY;

void ARC4 (ARC4_KEY *ctx, unsigned long len, const unsigned char *inbuf, unsigned char *outbuf);
void ARC4_set_key (ARC4_KEY *ctx, unsigned int keylen, const unsigned char *key);

#endif /* _DPXCRYPT_H_ */

/*
 * $Id: crypto.c 1.2 06/05/05 12:16:34+03:00 anttit@tcs.hut.fi $
 *
 * This file is part of the MIPL Mobile IPv6 for Linux.
 *
 * Copyright 2006 Helsinki University of Technology
 *
 * MIPL Mobile IPv6 for Linux is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; version 2 of
 * the License.
 *
 * MIPL Mobile IPv6 for Linux is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MIPL Mobile IPv6 for Linux; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 */

/*
 * Copyright (C) 1998, 2001, 2002, 2003 Free Software Foundation, Inc.
 *
 * This file contains parts of sha1.c, bithelp.h, g10lib.h, and misc.c
 * from the Libgcrypt software library package.  It has been modified
 * to provide standalone SHA-1 functionality.  Libgcrypt is licensed
 * under the LGPL.  See Libgcrypt home page for more information and
 * complete library: http://directory.fsf.org/security/libgcrypt.html
 *
 * HMAC-SHA1 functionality has been added.  This is a modified version
 * of HMAC-MD5 example in RFC2104 (code not from libgcrypt), and has
 * been tested with HMAC-SHA1 test cases as described in RFC2202.
 * Also, random_bytes() function was added.  These random bytes should
 * be good enough for MIPv6 use (no guarantees though).  Use Libgcrypt
 * or OpenSSL, if you need better.
 */

/*  SHA-1 Test vectors:
 *
 *  "abc"
 *  A999 3E36 4706 816A BA3E  2571 7850 C26C 9CD0 D89D
 *
 *  "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 *  8498 3E44 1C3B D26E BAAE  4AA1 F951 29E5 E546 70F1
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "crypto.h"

/****************
 * Rotate the 32 bit unsigned integer X by N bits left/right
 */
#if defined(__GNUC__) && defined(__i386__)
static inline uint32_t rol(uint32_t x, int n)
{
  __asm__("roll %%cl,%0"
          :"=r" (x)
          :"0" (x),"c" (n));
  return x;
}
#else
#define rol(x,n) ( ((x) << (n)) | ((x) >> (32-(n))) )
#endif

#if defined(__GNUC__) && defined(__i386__)
static inline uint32_t ror(uint32_t x, int n)
{
  __asm__("rorl %%cl,%0"
          :"=r" (x)
          :"0" (x),"c" (n));
  return x;
}
#else
#define ror(x,n) ( ((x) >> (n)) | ((x) << (32-(n))) )
#endif

/* To avoid that a compiler optimizes certain memset calls away, these
   macros may be used instead. */
#define wipememory2(_ptr,_set,_len) do { \
              volatile char *_vptr=(volatile char *)(_ptr); \
              size_t _vlen=(_len); \
              while(_vlen) { *_vptr=(_set); _vptr++; _vlen--; } \
                  } while(0)
#define wipememory(_ptr,_len) wipememory2(_ptr,0,_len)


static void _gcry_burn_stack(int bytes)
{
  char buf[64];

  wipememory(buf, sizeof buf);
  bytes -= sizeof buf;

  if (bytes > 0)
    _gcry_burn_stack(bytes);
}

void SHA1_init(SHA1_CTX *ctx)
{
  ctx->h0 = 0x67452301;
  ctx->h1 = 0xefcdab89;
  ctx->h2 = 0x98badcfe;
  ctx->h3 = 0x10325476;
  ctx->h4 = 0xc3d2e1f0;
  ctx->nblocks = 0;
  ctx->count = 0;
}

/****************
 * Transform the message X which consists of 16 32-bit-words
 */
static void transform(SHA1_CTX *hd, const uint8_t *data)
{
  register uint32_t a,b,c,d,e,tm;
  uint32_t x[16];

  /* Get values from the chaining vars. */
  a = hd->h0;
  b = hd->h1;
  c = hd->h2;
  d = hd->h3;
  e = hd->h4;

#ifdef WORDS_BIGENDIAN
  memcpy(x, data, 64);
#else
  {
    int i;
    uint8_t *p2;

    for (i = 0, p2 = (uint8_t *)x; i < 16; i++, p2 += 4) {
      p2[3] = *data++;
      p2[2] = *data++;
      p2[1] = *data++;
      p2[0] = *data++;
    }
  }
#endif

#define K1  0x5A827999L
#define K2  0x6ED9EBA1L
#define K3  0x8F1BBCDCL
#define K4  0xCA62C1D6L
#define F1(x,y,z)   ( z ^ ( x & ( y ^ z ) ) )
#define F2(x,y,z)   ( x ^ y ^ z )
#define F3(x,y,z)   ( ( x & y ) | ( z & ( x | y ) ) )
#define F4(x,y,z)   ( x ^ y ^ z )


#define M(i) ( tm =   x[i&0x0f] ^ x[(i-14)&0x0f] \
        ^ x[(i-8)&0x0f] ^ x[(i-3)&0x0f] \
         , (x[i&0x0f] = rol(tm, 1)) )

#define R(a,b,c,d,e,f,k,m)  do { e += rol( a, 5 )     \
              + f( b, c, d )  \
              + k       \
              + m;        \
         b = rol( b, 30 );    \
             } while(0)
  R( a, b, c, d, e, F1, K1, x[ 0] );
  R( e, a, b, c, d, F1, K1, x[ 1] );
  R( d, e, a, b, c, F1, K1, x[ 2] );
  R( c, d, e, a, b, F1, K1, x[ 3] );
  R( b, c, d, e, a, F1, K1, x[ 4] );
  R( a, b, c, d, e, F1, K1, x[ 5] );
  R( e, a, b, c, d, F1, K1, x[ 6] );
  R( d, e, a, b, c, F1, K1, x[ 7] );
  R( c, d, e, a, b, F1, K1, x[ 8] );
  R( b, c, d, e, a, F1, K1, x[ 9] );
  R( a, b, c, d, e, F1, K1, x[10] );
  R( e, a, b, c, d, F1, K1, x[11] );
  R( d, e, a, b, c, F1, K1, x[12] );
  R( c, d, e, a, b, F1, K1, x[13] );
  R( b, c, d, e, a, F1, K1, x[14] );
  R( a, b, c, d, e, F1, K1, x[15] );
  R( e, a, b, c, d, F1, K1, M(16) );
  R( d, e, a, b, c, F1, K1, M(17) );
  R( c, d, e, a, b, F1, K1, M(18) );
  R( b, c, d, e, a, F1, K1, M(19) );
  R( a, b, c, d, e, F2, K2, M(20) );
  R( e, a, b, c, d, F2, K2, M(21) );
  R( d, e, a, b, c, F2, K2, M(22) );
  R( c, d, e, a, b, F2, K2, M(23) );
  R( b, c, d, e, a, F2, K2, M(24) );
  R( a, b, c, d, e, F2, K2, M(25) );
  R( e, a, b, c, d, F2, K2, M(26) );
  R( d, e, a, b, c, F2, K2, M(27) );
  R( c, d, e, a, b, F2, K2, M(28) );
  R( b, c, d, e, a, F2, K2, M(29) );
  R( a, b, c, d, e, F2, K2, M(30) );
  R( e, a, b, c, d, F2, K2, M(31) );
  R( d, e, a, b, c, F2, K2, M(32) );
  R( c, d, e, a, b, F2, K2, M(33) );
  R( b, c, d, e, a, F2, K2, M(34) );
  R( a, b, c, d, e, F2, K2, M(35) );
  R( e, a, b, c, d, F2, K2, M(36) );
  R( d, e, a, b, c, F2, K2, M(37) );
  R( c, d, e, a, b, F2, K2, M(38) );
  R( b, c, d, e, a, F2, K2, M(39) );
  R( a, b, c, d, e, F3, K3, M(40) );
  R( e, a, b, c, d, F3, K3, M(41) );
  R( d, e, a, b, c, F3, K3, M(42) );
  R( c, d, e, a, b, F3, K3, M(43) );
  R( b, c, d, e, a, F3, K3, M(44) );
  R( a, b, c, d, e, F3, K3, M(45) );
  R( e, a, b, c, d, F3, K3, M(46) );
  R( d, e, a, b, c, F3, K3, M(47) );
  R( c, d, e, a, b, F3, K3, M(48) );
  R( b, c, d, e, a, F3, K3, M(49) );
  R( a, b, c, d, e, F3, K3, M(50) );
  R( e, a, b, c, d, F3, K3, M(51) );
  R( d, e, a, b, c, F3, K3, M(52) );
  R( c, d, e, a, b, F3, K3, M(53) );
  R( b, c, d, e, a, F3, K3, M(54) );
  R( a, b, c, d, e, F3, K3, M(55) );
  R( e, a, b, c, d, F3, K3, M(56) );
  R( d, e, a, b, c, F3, K3, M(57) );
  R( c, d, e, a, b, F3, K3, M(58) );
  R( b, c, d, e, a, F3, K3, M(59) );
  R( a, b, c, d, e, F4, K4, M(60) );
  R( e, a, b, c, d, F4, K4, M(61) );
  R( d, e, a, b, c, F4, K4, M(62) );
  R( c, d, e, a, b, F4, K4, M(63) );
  R( b, c, d, e, a, F4, K4, M(64) );
  R( a, b, c, d, e, F4, K4, M(65) );
  R( e, a, b, c, d, F4, K4, M(66) );
  R( d, e, a, b, c, F4, K4, M(67) );
  R( c, d, e, a, b, F4, K4, M(68) );
  R( b, c, d, e, a, F4, K4, M(69) );
  R( a, b, c, d, e, F4, K4, M(70) );
  R( e, a, b, c, d, F4, K4, M(71) );
  R( d, e, a, b, c, F4, K4, M(72) );
  R( c, d, e, a, b, F4, K4, M(73) );
  R( b, c, d, e, a, F4, K4, M(74) );
  R( a, b, c, d, e, F4, K4, M(75) );
  R( e, a, b, c, d, F4, K4, M(76) );
  R( d, e, a, b, c, F4, K4, M(77) );
  R( c, d, e, a, b, F4, K4, M(78) );
  R( b, c, d, e, a, F4, K4, M(79) );

  /* Update chaining vars. */
  hd->h0 += a;
  hd->h1 += b;
  hd->h2 += c;
  hd->h3 += d;
  hd->h4 += e;
}


/* Update the message digest with the contents of BUF with length LEN.
 */
void SHA1_update(SHA1_CTX *ctx, const uint8_t *buf, size_t len)
{
  if (ctx->count == 64) { /* flush the buffer */
    transform(ctx, ctx->buf);
    _gcry_burn_stack(88 + 4 * sizeof(void *));
    ctx->count = 0;
    ctx->nblocks++;
  }

  if (!buf)
    return;

  if (ctx->count) {
    for (; len && ctx->count < 64; len--)
      ctx->buf[ctx->count++] = *buf++;

    SHA1_update(ctx, NULL, 0);

    if (!len)
      return;
  }

  while (len >= 64) {
    transform(ctx, buf);
    ctx->count = 0;
    ctx->nblocks++;
    len -= 64;
    buf += 64;
  }

  _gcry_burn_stack(88 + 4 * sizeof(void *));

  for (; len && ctx->count < 64; len--)
    ctx->buf[ctx->count++] = *buf++;
}

/* The routine final terminates the computation and returns the
 * digest.  The handle is prepared for a new cycle, but adding bytes
 * to the handle will the destroy the returned buffer.  Returns: 20
 * bytes representing the digest.
 */
void SHA1_final(SHA1_CTX *ctx, uint8_t *digest)
{
  uint32_t t, msb, lsb;
  uint8_t *p;

  SHA1_update(ctx, NULL, 0); /* flush */;

  t = ctx->nblocks;
  /* multiply by 64 to make a byte count */
  lsb = t << 6;
  msb = t >> 26;
  /* add the count */
  t = lsb;

  if ((lsb += ctx->count) < t)
    msb++;

  /* multiply by 8 to make a bit count */
  t = lsb;
  lsb <<= 3;
  msb <<= 3;
  msb |= t >> 29;

  if (ctx->count < 56) { /* enough room */
    ctx->buf[ctx->count++] = 0x80; /* pad */

    while (ctx->count < 56)
      ctx->buf[ctx->count++] = 0;  /* pad */
  } else { /* need one extra block */
    ctx->buf[ctx->count++] = 0x80; /* pad character */

    while (ctx->count < 64)
      ctx->buf[ctx->count++] = 0;

    SHA1_update(ctx, NULL, 0);  /* flush */;
    memset(ctx->buf, 0, 56 ); /* fill next block with zeroes */
  }

  /* append the 64 bit count */
  ctx->buf[56] = msb >> 24;
  ctx->buf[57] = msb >> 16;
  ctx->buf[58] = msb >>  8;
  ctx->buf[59] = msb;
  ctx->buf[60] = lsb >> 24;
  ctx->buf[61] = lsb >> 16;
  ctx->buf[62] = lsb >>  8;
  ctx->buf[63] = lsb;
  transform(ctx, ctx->buf );
  _gcry_burn_stack(88 + 4 * sizeof(void *));

  p = ctx->buf;
#ifdef WORDS_BIGENDIAN
#define X(a) do { *(uint32_t *)p = ctx->h##a ; p += 4; } while(0)
#else /* little endian */
#define X(a) do { *p++ = ctx->h##a >> 24; *p++ = ctx->h##a >> 16;  \
                  *p++ = ctx->h##a >> 8; *p++ = ctx->h##a; } while(0)
#endif
  X(0);
  X(1);
  X(2);
  X(3);
  X(4);
#undef X

  memcpy(digest, ctx->buf, SHA_DIGESTSIZE);
}

void HMAC_SHA1_init(HMAC_SHA1_CTX *ctx, const uint8_t *key, size_t keylen)
{
  SHA1_CTX *ictx;
  uint8_t tkey[SHA_DIGESTSIZE];
  uint8_t pad[SHA_BLOCKSIZE];
  int i;

  ictx = &ctx->ictx;

  memcpy(tkey, key, keylen);

  if (keylen > SHA_BLOCKSIZE) {
    SHA1_CTX tctx;

    SHA1_init(&tctx);
    SHA1_update(&tctx, key, keylen);
    SHA1_final(&tctx, tkey);
    keylen = SHA_DIGESTSIZE;
  }

  SHA1_init(ictx);

  memset(pad, 0x36, SHA_BLOCKSIZE);
  memset(ctx->pad, 0x5C, SHA_BLOCKSIZE);

  for (i = 0; i < keylen; i++) {
    pad[i] ^= tkey[i];
    ctx->pad[i] ^= tkey[i];
  }

  SHA1_update(ictx, pad, SHA_BLOCKSIZE);
}

void HMAC_SHA1_update(HMAC_SHA1_CTX *ctx, const uint8_t *buf, size_t len)
{
  SHA1_update(&ctx->ictx, buf, len);
}

void HMAC_SHA1_final(HMAC_SHA1_CTX *ctx, uint8_t *digest)
{
  SHA1_CTX octx;
  uint8_t idigest[20];

  SHA1_final(&ctx->ictx, idigest);

  SHA1_init(&octx);
  SHA1_update(&octx, ctx->pad, SHA_BLOCKSIZE);
  SHA1_update(&octx, idigest, SHA_DIGESTSIZE);
  SHA1_final(&octx, digest);
}

/* This function provides maximum of 20 random bytes.  Bytes are
 * calculated as follows: HMAC-SHA1(key=random(), msg=random()).
 */
int random_bytes(uint8_t *buffer, int num)
{
  long int key, msg;
  uint8_t bytes[20];
  HMAC_SHA1_CTX ctx;

  if (num < 1) return -1;

  key = random();
  msg = random();
  HMAC_SHA1_init(&ctx, (uint8_t *)&key, sizeof(key));
  HMAC_SHA1_update(&ctx, (uint8_t *)&msg, sizeof(msg));
  HMAC_SHA1_final(&ctx, bytes);
  memcpy(buffer, bytes, num > 20 ? 20 : num);

  return 0;
}

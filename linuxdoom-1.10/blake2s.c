// Public domain BLAKE2s (minimal) based on reference; trimmed for this project.
#include "blake2s.h"
#include <string.h>

static const uint32_t blake2s_IV[8] = {
  0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
  0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

static const uint8_t blake2s_sigma[10][16] = {
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 },
  {14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3 },
  {11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4 },
  { 7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8 },
  { 9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13 },
  { 2,12, 6,10, 4, 7,15,14, 1,11, 9, 5, 3,13, 8, 0 },
  {12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11 },
  {13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10 },
  { 6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5 },
  {10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0 }
};

static uint32_t rotr32(const uint32_t w, const unsigned c) {
  return ( w >> c ) | ( w << (32 - c) );
}

static void blake2s_set_lastnode(blake2s_state *S) { }
static void blake2s_set_lastblock(blake2s_state *S) { S->f[0] = (uint32_t)-1; }

static void blake2s_increment_counter(blake2s_state *S, const uint32_t inc)
{
  S->t[0] += inc;
  if (S->t[0] < inc) S->t[1]++;
}

static void blake2s_init0(blake2s_state *S)
{
  int i;
  memset(S, 0, sizeof(*S));
  for (i = 0; i < 8; i++) S->h[i] = blake2s_IV[i];
}

static void blake2s_compress(blake2s_state *S, const uint8_t block[64])
{
  uint32_t m[16];
  uint32_t v[16];
  int i;

  for (i = 0; i < 16; ++i)
    m[i] = ((uint32_t)block[i * 4 + 0] << 0) |
           ((uint32_t)block[i * 4 + 1] << 8) |
           ((uint32_t)block[i * 4 + 2] << 16) |
           ((uint32_t)block[i * 4 + 3] << 24);

  for (i = 0; i < 8; ++i) v[i] = S->h[i];
  v[ 8] = blake2s_IV[0];
  v[ 9] = blake2s_IV[1];
  v[10] = blake2s_IV[2];
  v[11] = blake2s_IV[3];
  v[12] = S->t[0] ^ blake2s_IV[4];
  v[13] = S->t[1] ^ blake2s_IV[5];
  v[14] = S->f[0] ^ blake2s_IV[6];
  v[15] = S->f[1] ^ blake2s_IV[7];

#define G(r,i,a,b,c,d) \
  do { \
    a = a + b + m[ blake2s_sigma[r][2*i+0] ]; \
    d = rotr32(d ^ a, 16); \
    c = c + d; \
    b = rotr32(b ^ c, 12); \
    a = a + b + m[ blake2s_sigma[r][2*i+1] ]; \
    d = rotr32(d ^ a, 8); \
    c = c + d; \
    b = rotr32(b ^ c, 7); \
  } while(0)

#define ROUND(r) \
  do { \
    G(r,0,v[ 0],v[ 4],v[ 8],v[12]); \
    G(r,1,v[ 1],v[ 5],v[ 9],v[13]); \
    G(r,2,v[ 2],v[ 6],v[10],v[14]); \
    G(r,3,v[ 3],v[ 7],v[11],v[15]); \
    G(r,4,v[ 0],v[ 5],v[10],v[15]); \
    G(r,5,v[ 1],v[ 6],v[11],v[12]); \
    G(r,6,v[ 2],v[ 7],v[ 8],v[13]); \
    G(r,7,v[ 3],v[ 4],v[ 9],v[14]); \
  } while(0)

  ROUND(0); ROUND(1); ROUND(2); ROUND(3); ROUND(4);
  ROUND(5); ROUND(6); ROUND(7); ROUND(8); ROUND(9);

#undef G
#undef ROUND

  for( i = 0; i < 8; ++i )
    S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
}

int blake2s_init_param(blake2s_state *S, size_t outlen, const void *key, size_t keylen)
{
  blake2s_init0(S);
  S->h[0] ^= 0x01010000 ^ (uint32_t)(keylen << 8) ^ (uint32_t)outlen;
  if (keylen > 0)
  {
    uint8_t block[64];
    memset(block, 0, sizeof(block));
    memcpy(block, key, keylen);
    blake2s_update(S, block, 64);
  }
  return 0;
}

int blake2s_init(blake2s_state *S, size_t outlen)
{
  return blake2s_init_param(S, outlen, NULL, 0);
}

int blake2s_init_key(blake2s_state *S, size_t outlen, const void *key, size_t keylen)
{
  return blake2s_init_param(S, outlen, key, keylen);
}

int blake2s_update(blake2s_state *S, const void *pin, size_t inlen)
{
  const uint8_t *in = (const uint8_t *)pin;
  if (inlen == 0) return 0;
  while (inlen > 0) {
    size_t left = S->buflen;
    size_t fill = 64 - left;
    if (inlen > fill) {
      memcpy(S->buf + left, in, fill); // fill buffer
      S->buflen += fill;
      blake2s_increment_counter(S, 64);
      blake2s_compress(S, S->buf); // compress
      S->buflen = 0;
      in += fill;
      inlen -= fill;
    } else { // inlen <= fill
      memcpy(S->buf + left, in, inlen);
      S->buflen += inlen; // hold
      return 0;
    }
  }
  return 0;
}

int blake2s_final(blake2s_state *S, void *out, size_t outlen)
{
  uint8_t buffer[32];
  int i;
  if (outlen == 0 || outlen > 32) return -1;
  blake2s_increment_counter(S, (uint32_t)S->buflen);
  blake2s_set_lastblock(S);
  memset(S->buf + S->buflen, 0, 64 - S->buflen); // padding
  blake2s_compress(S, S->buf);

  for (i = 0; i < 8; ++i) {
    buffer[i * 4 + 0] = (uint8_t)(S->h[i] >> 0);
    buffer[i * 4 + 1] = (uint8_t)(S->h[i] >> 8);
    buffer[i * 4 + 2] = (uint8_t)(S->h[i] >> 16);
    buffer[i * 4 + 3] = (uint8_t)(S->h[i] >> 24);
  }

  memcpy(out, buffer, outlen);
  return 0;
}

void blake2s_mac16(const uint8_t *key, size_t keylen, const uint8_t *data, size_t len, uint8_t out[16])
{
  blake2s_state S;
  blake2s_init_key(&S, 16, key, keylen);
  blake2s_update(&S, data, len);
  blake2s_final(&S, out, 16);
}

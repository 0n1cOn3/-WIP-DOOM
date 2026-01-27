#pragma once
// Minimal BLAKE2s interface (public domain/CC0 single-file impl)
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t h[8];
    uint32_t t[2];
    uint32_t f[2];
    uint8_t  buf[64];
    size_t   buflen;
} blake2s_state;

int blake2s_init(blake2s_state *S, size_t outlen);
int blake2s_init_key(blake2s_state *S, size_t outlen, const void *key, size_t keylen);
int blake2s_update(blake2s_state *S, const void *pin, size_t inlen);
int blake2s_final(blake2s_state *S, void *out, size_t outlen);

// Convenience: keyed MAC into a 16-byte output
void blake2s_mac16(const uint8_t *key, size_t keylen, const uint8_t *data, size_t len, uint8_t out[16]);

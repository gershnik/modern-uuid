/* sha3.h */
#ifndef RHASH_SHA3_H
#define RHASH_SHA3_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define sha3_224_hash_size  28
#define sha3_256_hash_size  32
#define sha3_384_hash_size  48
#define sha3_512_hash_size  64
#define sha3_max_permutation_size 25
#define sha3_max_rate_in_qwords 24

/**
 * SHA3 Algorithm context.
 */
typedef struct muuid_sha3_ctx
{
	/* 1600 bits algorithm hashing state */
	uint64_t hash[sha3_max_permutation_size];
	/* 1536-bit buffer for leftovers */
	uint64_t message[sha3_max_rate_in_qwords];
	/* count of bytes in the message[] buffer */
	unsigned rest;
	/* size of a message block processed at once */
	unsigned block_size;
} muuid_sha3_ctx;

/* methods for calculating the hash function */

void muuid_sha3_224_init(muuid_sha3_ctx* ctx);
void muuid_sha3_256_init(muuid_sha3_ctx* ctx);
void muuid_sha3_384_init(muuid_sha3_ctx* ctx);
void muuid_sha3_512_init(muuid_sha3_ctx* ctx);
void muuid_sha3_update(muuid_sha3_ctx* ctx, const unsigned char* msg, size_t size);
void muuid_sha3_final(muuid_sha3_ctx* ctx, unsigned char* result);

#ifdef USE_KECCAK
#define muuid_keccak_224_init muuid_sha3_224_init
#define muuid_keccak_256_init muuid_sha3_256_init
#define muuid_keccak_384_init muuid_sha3_384_init
#define muuid_keccak_512_init muuid_sha3_512_init
#define muuid_keccak_update muuid_sha3_update
void muuid_keccak_final(sha3_ctx* ctx, unsigned char* result);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* RHASH_SHA3_H */

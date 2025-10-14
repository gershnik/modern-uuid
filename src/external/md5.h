/*
 * No copyright is claimed. This code is in the public domain; do with it what
 * you wish.
 */
#ifndef UTIL_LINUX_MD5_H
#define UTIL_LINUX_MD5_H

#include <stdint.h>

#define MUUID_MD5LENGTH 16

struct MUUID_MD5Context {
    uint32_t buf[4];
    uint32_t bits[2];
    unsigned char in[64];
};

void muuid_MD5Init(struct MUUID_MD5Context *ctx);
void muuid_MD5Update(struct MUUID_MD5Context *ctx, unsigned char const *buf, unsigned len);
void muuid_MD5Final(unsigned char digest[MUUID_MD5LENGTH], struct MUUID_MD5Context *ctx);
void muuid_MD5Transform(uint32_t buf[4], uint32_t const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MUUID_MD5Context MUUID_MD5_CTX;

#endif /* !UTIL_LINUX_MD5_H */

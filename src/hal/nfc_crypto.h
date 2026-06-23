#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t oddparity8(uint8_t x);

void Crypto1Setup(uint8_t Key[6], uint8_t Uid[4], uint8_t CardNonce[4]);
void Crypto1Auth(uint8_t EncryptedReaderNonce[4]);
uint8_t Crypto1Byte(void);
void Crypto1ByteArray(uint8_t *Buffer, uint8_t Count);
uint8_t Crypto1Nibble(void);

#ifdef __cplusplus
}
#endif

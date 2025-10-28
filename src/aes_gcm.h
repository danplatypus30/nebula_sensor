#ifndef AES_GCM_H
#define AES_GCM_H

#include <stdint.h>
#include <stddef.h>

/*
 * AES-GCM parameter constants
 * ---------------------------
 * AES-128 uses a 16-byte key (128 bits)
 * GCM standard IV length = 12 bytes (96 bits)
 * Authentication tag length = 16 bytes (128 bits)
 */

#define AES_GCM_KEY_SIZE   16   // 128-bit key
#define AES_GCM_IV_SIZE    12   // 96-bit nonce / IV
#define AES_GCM_TAG_SIZE   16   // 128-bit authentication tag

/*
 * Encrypts plaintext[] of 'length' bytes with AES-GCM.
 * Resulting payload layout:
 *    [IV | Ciphertext | Tag]
 *
 * 'payload' must have space for AES_GCM_IV_SIZE + length + AES_GCM_TAG_SIZE bytes.
 */
void encrypt_character_array(const uint8_t *key,
                             const uint8_t *iv,
                             const uint8_t *plaintext,
                             uint8_t *payload,
                             size_t length);

#endif /* AES_GCM_H */


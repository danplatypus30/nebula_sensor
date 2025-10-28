#include "aes_gcm.h"
#include <psa/crypto.h>        // PSA API
#include <string.h>
#include <stdio.h>

// payload layout: [IV | Ciphertext | Tag]
// key:  AES_GCM_KEY_SIZE bytes (16)
// iv:   AES_GCM_IV_SIZE  bytes (12)
// tag:  AES_GCM_TAG_SIZE bytes (16) appended by PSA to output

// replaced nrf_crypto_* modules
// nrf_crypto_* is legacy and not part of the Zephyr/NCS BLE app

void encrypt_character_array(const uint8_t *key, const uint8_t *iv, const uint8_t *plaintext, uint8_t *payload, size_t length)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    // make the ciphertext buffer
    // PSA produces CT||TAG; we’ll prepend IV ourselves
    uint8_t ct_and_tag[/* pt */ 0 + /* tag */ 0 + 1]; // placeholder to satisfy old compilers
    // Use variable length array for exact ciphertext + tag size:
    uint8_t ct_and_tag_vla[length + AES_GCM_TAG_SIZE];
    uint8_t *ct_and_tag_ptr = ct_and_tag_vla;

    size_t out_len = 0;

    // PSA init
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("psa_crypto_init failed: %d\n", (int)status);
        return;
    }

    // Import a volatile AES-128 key (no persistent storage)
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, AES_GCM_KEY_SIZE * 8);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_GCM);

    status = psa_import_key(&attr, key, AES_GCM_KEY_SIZE, &key_id);
    if (status != PSA_SUCCESS) {
        printf("psa_import_key failed: %d\n", (int)status);
        return;
    }

    // AEAD encrypt (no AAD here; pass AAD ptr/len if you need associated data)
    status = psa_aead_encrypt(key_id, PSA_ALG_GCM,
                          iv, AES_GCM_IV_SIZE,   // nonce / IV (12 bytes recommended)
                          NULL, 0,               // AAD ptr/len (not added)
                          plaintext, length,     // input
                          ct_and_tag_ptr,         // ptr to ciphertext buffer
                          sizeof(ct_and_tag_vla), // size of ciphertext buffer
                          &out_len); // size of output in the buffer
    if (status != PSA_SUCCESS) {
        printf("psa_aead_encrypt failed: %d\n", (int)status);
        (void) psa_destroy_key(key_id);
        return;
    }

    // Expect ciphertext + tag
    if (out_len != (length + AES_GCM_TAG_SIZE)) {
        printf("psa_aead_encrypt unexpected out_len=%u\n", (unsigned)out_len);
        (void) psa_destroy_key(key_id);
        return;
    }

    // 3) Create the payload with the structure: IV || Ciphertext || Authentication Tag
    // Build payload = IV || CT || TAG
    memcpy(payload,                              iv,               AES_GCM_IV_SIZE); // iv
    memcpy(payload + AES_GCM_IV_SIZE,            ct_and_tag_ptr,   out_len);         // ciphertext and tag

    // free
    (void) psa_destroy_key(key_id);
}



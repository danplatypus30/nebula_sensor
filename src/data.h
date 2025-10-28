#pragma once
#include <stdint.h>
#include <stddef.h>

#define CHUNK_SIZE 200u

// Same meaning as the old project
typedef struct __packed {
    uint8_t num_chunks;   // total chunks to send
    uint8_t chunks_rx;    // acks received from central
    uint8_t ready;        // 0=idle, 1=sending, 2=done
} meta_t;

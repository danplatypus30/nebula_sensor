/*
 * Move kernel.h to the top. This transitively includes autoconf.h,
 * which is required for the NUS service functions to be declared correctly.
 */
#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>      // sys_csrand_get()
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h> // Include NUS header

#include "data.h"
#include "aes_gcm.h"
#include "sensor_logic.h"

// This is defined in main.c, declare it here to use it
extern struct bt_conn *current_conn;

LOG_MODULE_DECLARE(peripheral_uart, LOG_LEVEL_INF);

// ---- App state (replace sizes with your real max payload) ----
static struct {
    // Plaintext to protect (fill this with your real sensor bytes)
    uint8_t  plaintext[2048];
    size_t   plaintext_len;

    // Encrypted payload = IV || CT || TAG
    uint8_t  payload[2048 + AES_GCM_IV_SIZE + AES_GCM_TAG_SIZE];
    size_t   payload_len;

    // Transfer progress
    size_t   off;
    bool     running;

    // Metadata (like the old code)
    meta_t   meta;

    // Crypto IV (12 bytes)
    uint8_t  iv[AES_GCM_IV_SIZE];

    struct k_work_delayable tx_work;
} S;

// ---- Work handler to push chunks over NUS ----
static void tx_work_handler(struct k_work *work)
{
    if (!S.running || !current_conn) return;

    // If notifications are not enabled yet, bt_nus_send() returns -EINVAL.
    // We'll just retry a little later.
    size_t remaining = S.payload_len - S.off;
    size_t n = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;

    // Replace the custom function with the standard bt_nus_send
    int err = bt_nus_send(current_conn, &S.payload[S.off], n);
    if (err) {
        LOG_WRN("bt_nus_send err %d (retry)", err);
        k_work_reschedule(&S.tx_work, K_MSEC(200));
        return;
    }

    S.off += n;

    // Update & “publish” metadata locally (mirrors old behavior)
    S.meta.chunks_rx += 1;

    if (S.off < S.payload_len) {
        k_work_reschedule(&S.tx_work, K_MSEC(5)); // pacing
    } else {
        S.meta.ready = 2; // done
        S.running = false;
        LOG_INF("transfer complete (%u bytes)", (unsigned)S.payload_len);
    }
}

// Define the callbacks for the NUS service
static struct bt_nus_cb nus_callbacks = {
    .received = sensor_on_rx_cmd,
};

void sensor_init(void)
{
    int err;

    memset(&S, 0, sizeof(S));
    k_work_init_delayable(&S.tx_work, tx_work_handler);

    // replaces bt_nus_init()
    err = bt_nus_cb_register(&nus_callbacks, NULL);
    if (err) {
        LOG_ERR("Failed to initialize NUS service (err: %d)", err);
    }
}

// Fill S.plaintext with your real data
static void fill_plaintext_demo(void)
{
    static const char demo[] = "NEBULA demo payload — replace with real sensor data";
    memcpy(S.plaintext, demo, sizeof(demo));
    S.plaintext_len = sizeof(demo);
}

void sensor_prepare_payload(void)
{
    // 1) Fill plaintext
    fill_plaintext_demo();

    /* Comment out all encryption codes, send in plaintext
    
    // 2) Random IV (12 bytes). On Nordic DKs, sys_csrand_get() draws from HW entropy.
    sys_csrand_get(S.iv, sizeof(S.iv));

    // 3) Encrypt: output layout = IV || CT || TAG (your PSA-backed function)
    encrypt_character_array(
        //key (const uint8_t*)"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
        //iv   S.iv,
        //pt   S.plaintext,
        //out  S.payload,
        //len  S.plaintext_len
    );
    S.payload_len = AES_GCM_IV_SIZE + S.plaintext_len + AES_GCM_TAG_SIZE;
    */

    // Send plaintext directly without encryption
    memcpy(S.payload, S.plaintext, S.plaintext_len);
    S.payload_len = S.plaintext_len;

    // 4) Init metadata like the old code
    S.meta.num_chunks = (S.payload_len + CHUNK_SIZE - 1) / CHUNK_SIZE;
    S.meta.chunks_rx  = 0;
    S.meta.ready      = 1;   // “sending”
    S.off             = 0;

    // logs to show plaintext being sent
    LOG_INF("Payload to be sent: \"%.*s\"", (int)S.payload_len, S.payload);
}

void sensor_start_transfer(void)
{
    if (!current_conn) {
        LOG_WRN("no connection; cannot start transfer");
        return;
    }
    if (S.payload_len == 0) {
        sensor_prepare_payload();
    }
    S.running = true;
    k_work_reschedule(&S.tx_work, K_NO_WAIT);
}

// Very small command parser over NUS RX.
// You can keep the old 3-byte metadata flow by having the central send “ACK”
// after each chunk, or keep it simple: central sends “START”, we stream.
void sensor_on_rx_cmd(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
    // The 'conn' parameter is unused in this case, but required by the signature.
    // The (void)conn; cast prevents a compiler warning about an unused variable.
    (void)conn;

    if (len >= 5 && !memcmp(data, "START", 5)) {
        LOG_INF("START received from central");
        sensor_prepare_payload();
        LOG_INF("payload prepared starting transfer");
        sensor_start_transfer();
        return;
    }

    if (len >= 4 && !memcmp(data, "PREP", 4)) {
        LOG_INF("PREP received from central");
        sensor_prepare_payload();
        return;
    }

    // Optional: accept acknowledgments like the old metadata flow
    // Example: "ACK <n>" increments S.meta.chunks_rx to <n>
    if (len >= 3 && !memcmp(data, "ACK", 3)) {
        // parse number after ACK if you want strict syncing
        LOG_INF("ACK received from central");
        return;
    }

    LOG_INF("RX cmd ignored (len=%u)", (unsigned)len);
}

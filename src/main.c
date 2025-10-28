/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic UART Bridge Service (NUS) sample
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include <zephyr/bluetooth/services/nus.h>

#include <dk_buttons_and_leds.h>

#include <zephyr/settings/settings.h>

#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>

#include "data.h"
#include "sensor_logic.h"

#define LOG_MODULE_NAME peripheral_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/* Default Nordic UART Service (NUS) UUID in little-endian byte order */
#define BT_UUID_NUS_VAL \
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e

/* Define a custom 16-bit UUID for the Nebula service identifier */
#define BT_UUID_NEBULA_VAL 0x180A
#define BT_UUID_NEBULA     BT_UUID_DECLARE_16(BT_UUID_NEBULA_VAL)

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

#define CON_STATUS_LED DK_LED2

struct bt_conn *current_conn = NULL; // global, not static
static struct k_work adv_work;

// Advertise both the 128-bit NUS UUID and the 16-bit Nebula identifier UUID
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    /* Advertise the 128-bit NUS Service UUID. No extra parentheses needed. */
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
    /* Advertise the 16-bit custom Nebula UUID as an identifier */
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_NEBULA_VAL)),
};

// Put the name in the scan response packet for debugging
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void advertising_start(void);

static void adv_work_handler(struct k_work *work)
{
    // Use the modern, non-deprecated advertising parameters
    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .sid = 0,
        .secondary_max_skip = 0,
        .options = BT_LE_ADV_OPT_CONNECTABLE,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .peer = NULL,
    };

    int err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad),
                  sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
    } else {
        LOG_INF("Advertising successfully started");
    }
}

// called when bluetooth device connects successfully to nrf
static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (err) {
        LOG_ERR("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
        return;
    }

    bt_addr_le_to_str(
        bt_conn_get_dst(conn), // address of connecting device
        addr, sizeof(addr) 
    ); // convert binary address to readable string
    LOG_INF("Connected %s", addr); 

    current_conn = bt_conn_ref(conn);
    dk_set_led_on(CON_STATUS_LED);
}

// called when bluetooth device disconnects from nrf
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    // show address and reason
    LOG_INF("Disconnected: %s, reason 0x%02x %s", addr, reason, bt_hci_err_to_str(reason));

    if (current_conn) {
        bt_conn_unref(current_conn); // decrements global var
        current_conn = NULL;
        dk_set_led_off(CON_STATUS_LED); // turn off LED2
    }
}

// happens on disconnect
static void recycled_cb(void)
{
    LOG_INF("Connection object available from previous conn. Disconnect is complete!");
    advertising_start();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected        = connected,
    .disconnected     = disconnected,
    .recycled         = recycled_cb,
};

// ADD THIS FUNCTION DEFINITION
static void advertising_start(void)
{
    k_work_submit(&adv_work);
}

static void error(void)
{
    dk_set_leds_state(DK_ALL_LEDS_MSK, DK_NO_LEDS_MSK);

    while (true) {
        /* Spin for ever */
        k_sleep(K_MSEC(1000));
    }
}

static void configure_gpio(void)
{
    int err;

    err = dk_leds_init();
    if (err) {
        LOG_ERR("Cannot init LEDs (err: %d)", err);
    }
}

int main(void)
{
    int blink_status = 0;
    int err;

    configure_gpio(); // configure pins and LED
    sensor_init();

    err = bt_enable(NULL);
    if (err) {
        error();
    }

    LOG_INF("Bluetooth initialized");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    k_work_init(&adv_work, adv_work_handler);
    advertising_start();

    // send hello message every 5 seconds
    // k_work_init_delayable(&periodic_tx, periodic_tx_handler);
    // k_work_reschedule(&periodic_tx, K_SECONDS(5));		

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}

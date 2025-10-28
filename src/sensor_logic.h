#ifndef SENSOR_LOGIC_H
#define SENSOR_LOGIC_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/bluetooth/conn.h>

void sensor_init(void);
void sensor_prepare_payload(void);
void sensor_start_transfer(void);
void sensor_on_rx_cmd(struct bt_conn *conn, const uint8_t *data, uint16_t len);

#endif // SENSOR_LOGIC_H
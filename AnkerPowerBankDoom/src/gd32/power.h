/* Port telemetry shared by the pre-game pages. See power.c for where the
   numbers come from in the stock firmware. */

#ifndef POWER_H
#define POWER_H

#include <stdint.h>

struct port_reading {
    int ok;          /* the chip answered */
    uint8_t state;   /* stock calls this usb_A[0] */
    uint16_t mv;     /* millivolts, as the chip reports them */
    uint16_t ma;     /* milliamps */
    uint16_t dv;     /* tenths of a volt, the unit the BLE status uses */
    uint16_t da;     /* tenths of an amp */
    uint16_t dw;     /* tenths of a watt, dv * da / 10 */
    uint8_t reg_a;
    uint8_t reg_b;
};

void power_init(void);
/* Runs the stock port-controller boot sequence once. */
void power_ports_start(void);
/* Pin modes for the bit-banged buses; safe on a running system. */
void ports_pins_setup(void);
/* Live register picture of the port controller, 16 bytes. */
const uint8_t *power_ctl_regs(void);
/* ADC counts on the converter rail: about 870 when it runs. */
uint16_t power_rail(void);
uint16_t power_rail_boot(void);
int power_cold_start(void);
void power_poll(void);
/* Port pages on: only then does this touch the shared I2C bus. */
void power_set_active(int on);
const struct port_reading *power_usba(void);
/* 1 once the chip has been given up on for this boot. */
int power_usba_given_up(void);
/* Addresses that answered the one bus scan, 7-bit. */
const uint8_t *power_i2c_seen(unsigned *n);
/* Pack current in mA, positive into the pack. */
int power_pack_ma(void);

/* gauge.c owns the bit-banged bus these use. */
int gbus_read(uint8_t addr_w, uint8_t reg, uint8_t *dst, unsigned n);
int gbus_probe(uint8_t addr_w);
unsigned gbus_count(void);
void gbus_select(unsigned n);
int gbus_write(uint8_t addr_w, uint8_t reg, uint8_t val);

#endif

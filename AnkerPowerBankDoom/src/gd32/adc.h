/* ADC0 telemetry of the two USB-C ports. See adc.c for where the
   register values and the channel map come from. */

#ifndef ADC_H
#define ADC_H

#include <stdint.h>

struct adc_reading {
    int ok;
    uint16_t c1_v_raw, c1_i_raw;
    uint16_t c2_v_raw, c2_i_raw;
    uint16_t c1_zero, c2_zero;
    uint16_t c1_dv, c2_dv; /* tenths of a volt */
    uint16_t c1_da, c2_da; /* tenths of an amp */
    uint16_t c1_dw, c2_dw; /* tenths of a watt */
    uint16_t rank0, rank5;
};

void adc_init(void);
void adc_poll(void);
const struct adc_reading *adc_now(void);
uint16_t adc_sample_rank(unsigned rank);
void adc_mark(void);
extern uint16_t adc_boot_marks[24];

#endif


#ifndef _CABLE_DIGILENT_H_
#define _CABLE_DIGILENT_H_

#include <stdint.h>
#include "cable_common.h"

jtag_cable_t *cable_digilent_get_driver(void);
int  cable_digilent_init();
int  cable_digilent_out(uint8_t value);
int  cable_digilent_inout(uint8_t value, uint8_t *inval);
void cable_digilent_wait();
int  cable_digilent_opt(int c, char *str);
int cable_digilent_write_bit(uint8_t packet);
int cable_digilent_read_write_bit(uint8_t packet_out, uint8_t *bit_in);
int cable_digilent_read_stream(uint32_t *outstream, uint32_t *instream, int len_bits, int set_last_bit);
int cable_digilent_write_stream(uint32_t *stream, int len_bits, int set_last_bit);

void jtag_set(int tck, int trstn, int tdi, int tms);
int jtag_get();
void jtag_close();

#endif

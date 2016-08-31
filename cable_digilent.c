/* cable_sim.c - Simulation connection drivers for the Advanced JTAG Bridge
   Copyright (C) 2001 Marko Mlinar, markom@opencores.org
   Copyright (C) 2004 György Jeney, nog@sdf.lonestar.org


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */



#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <sys/mman.h>

#include "cable_digilent.h"
#include "errcodes.h"
#include "/usr/include/digilent/adept/dpcdecl.h" 
#include "/usr/include/digilent/adept/djtg.h"
#include "/usr/include/digilent/adept/dmgr.h"

static HIF hif = hifInvalid;
static char *device;
static BOOL overlap;

#define debug(...) if (getenv("ADV_DEBUG")) fprintf(stderr, __VA_ARGS__ )

jtag_cable_t digilent_cable_driver = {
    .name              = "digilent",
    .inout_func        = cable_digilent_inout,
    .out_func          = cable_digilent_out,
    .init_func         = cable_digilent_init,
    .opt_func          = cable_digilent_opt,
    .bit_out_func      = cable_digilent_write_bit,
    .bit_inout_func    = cable_digilent_read_write_bit,
    .stream_out_func   = cable_digilent_write_stream,
    .stream_inout_func = cable_digilent_read_stream,
    .flush_func        = NULL,
    .opts              = "d:",
    .help              = "-d device name as reported by 'dadutil enum' e.g. Nexys4DDR\n",
};

/*-----------------------------------------------[ DIGILENT specific functions ]---*/

/* Writes bitstream via bit-bang. Can be used by any driver which does not have a high-speed transfer function.
 * Transfers LSB to MSB of stream[0], then LSB to MSB of stream[1], etc.
 */
int cable_digilent_write_stream(uint32_t *stream, int len_bits, int set_last_bit) {
  int err = APP_ERR_NONE;

  if (set_last_bit)
    {
      int len = len_bits - 1;
      int bits_this_index = len&31;
      DjtgPutTdiBits(hif, 0, (BYTE*)stream, NULL, len, overlap);
      uint8_t outval = ((stream[len/32] >> bits_this_index) & 1) | TMS;
      DjtgPutTmsTdiBits(hif, &outval, NULL, 1, overlap);
    }
  else
    DjtgPutTdiBits(hif, 0, (BYTE*)stream, NULL, len_bits, overlap);
  return err;
}

/* Gets bitstream via bit-bang.  Can be used by any driver which does not have a high-speed transfer function.
 * Transfers LSB to MSB of stream[0], then LSB to MSB of stream[1], etc.
 */
int cable_digilent_read_stream(uint32_t *outstream, uint32_t *instream, int len_bits, int set_last_bit) {
  int err = APP_ERR_NONE;
  if (set_last_bit)
    {
      int len = len_bits - 1;
      int bits_this_index = len&31;
      instream[len/32] = 0;
      DjtgPutTdiBits(hif, 0, (BYTE*)outstream, (BYTE*)instream, len, overlap);
      uint8_t inval, outval = ((outstream[len/32] >> bits_this_index) & 1) | TMS;
      DjtgPutTmsTdiBits(hif, &outval, &inval, 1, overlap);
      instream[len/32] |= ((inval&1) << bits_this_index);  
    }
  else
    DjtgPutTdiBits(hif, 0, (BYTE*)outstream, (BYTE*)instream, len_bits, overlap);
  return err;
}

jtag_cable_t *cable_digilent_get_driver(void)
{
  return &digilent_cable_driver;
}

int cable_digilent_write_bit(uint8_t packet)
{
  return DjtgPutTmsTdiBits(hif, &packet, NULL, 1, overlap) ? APP_ERR_NONE : APP_ERR_COMM;
}

int cable_digilent_read_write_bit(uint8_t packet_out, uint8_t *bit_in)
{
  return DjtgPutTmsTdiBits(hif, &packet_out, bit_in, 1, overlap) ? APP_ERR_NONE : APP_ERR_COMM;
}

int cable_digilent_init()
{
  DWORD frqSet;
  overlap = getenv("OVERLAP") != 0;

        if (hif == hifInvalid) {
                // DGTG API Call: DjtgDisable
	  // DMGR API Call: DmgrOpen
	  if(!DmgrOpen(&hif, device)) {
	    printf("Error: Could not open device \"%s\". Check device name\n", device);
	    return APP_ERR_BAD_PARAM;
	  }

	  // DJTG API CALL: DjtgEnable
	  if(!DjtgEnable(hif)) {
	    printf("Error: DjtgEnable failed\n");
	    return APP_ERR_BAD_PARAM;
	  }

	  if (!DjtgSetSpeed(hif, 100000000, &frqSet))
	    printf("Error: DjtgSetSpeed failed\n");
	  else
	    printf("DjtgSetSpeed returned %dHz\n", frqSet);

	  debug("DIGILENT cable ready!\n");
	}
	
  return APP_ERR_NONE;
}

int cable_digilent_out(uint8_t value)
{
  int tck   = (value & 0x1) >> 0;
  int trstn = (value & 0x2) >> 1;
  int tdi   = (value & 0x4) >> 2;
  int tms   = (value & 0x8) >> 3;

  debug("Sent %d\n", value);
  jtag_set(tck, trstn, tdi, tms);

  return APP_ERR_NONE;
}

int cable_digilent_inout(uint8_t value, uint8_t *inval)
{
  uint8_t dat;

  dat = jtag_get();

  cable_digilent_out(value);

  *inval = dat;

  return APP_ERR_NONE;
}

int cable_digilent_opt(int c, char *str)
{
  switch(c) {
  case 'd':
    device = strdup(str);
    return cable_digilent_init();
    break;
  default:
    fprintf(stderr, "Unknown parameter '%c'\n", c);
    return APP_ERR_BAD_PARAM;
  }
  return APP_ERR_NONE;
}

void jtag_set(int tck, int trstn, int tdi, int tms) {
  static int oldrst = -1;
  if (trstn != oldrst)
    {
      oldrst = trstn;
      DjtgSetAuxReset(hif, trstn, 1);
    }

DjtgSetTmsTdiTck(hif, tms!=0, tdi!=0, tck!=0);
}

int jtag_get() {

  BYTE rgbTdo;
  // DJTG API Call: DjtgGetTdoBits
  if(!DjtgGetTdoBits(hif, fFalse, fFalse, &rgbTdo, 1, overlap)) {
    printf("Error: DjtgGetTdoBits failed\n");
    abort();
  }
  return rgbTdo != 0;
}

void jtag_close() {
       // Disable Djtg and close device handle
        if( hif != hifInvalid ) {
                // DGTG API Call: DjtgDisable
                DjtgDisable(hif);

                // DMGR API Call: DmgrClose
                DmgrClose(hif);
        }
}

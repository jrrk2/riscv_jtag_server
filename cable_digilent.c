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

static HIF hif;

#define debug(...) if (getenv("ADV_DEBUG10")) fprintf(stderr, __VA_ARGS__ )

/* Only used in the vpi */
jtag_cable_t digilent_cable_driver = {
    .name              = "digilent",
    .inout_func        = cable_digilent_inout,
    .out_func          = cable_digilent_out,
    .init_func         = cable_digilent_init,
    .opt_func          = cable_digilent_opt,
    .bit_out_func      = cable_digilent_write_bit,
    .bit_inout_func    = cable_digilent_read_write_bit,
    .stream_out_func   = cable_common_write_stream,
    .stream_inout_func = cable_common_read_stream,
    .flush_func        = NULL,
    .opts              = "",
    .help              = "",
};

/*-----------------------------------------------[ DIGILENT specific functions ]---*/
jtag_cable_t *cable_digilent_get_driver(void)
{
  return &digilent_cable_driver;
}

int cable_digilent_write_bit(uint8_t packet)
{
  uint8_t bit_in;
  return cable_digilent_read_write_bit(packet, &bit_in);
}

int cable_digilent_read_write_bit(uint8_t packet_out, uint8_t *bit_in)
{
  BYTE rgbSnd, rgbRcv;
  int err = APP_ERR_NONE;

        int tdi = ((packet_out & TDO) ? 0x01 : 0x00);
        int tms = ((packet_out & TMS) ? 1 : 0);

	rgbSnd = (tms << 1) | tdi;
	
	DjtgPutTmsTdiBits(hif, &rgbSnd, &rgbRcv, 1, fFalse);
	*bit_in = rgbRcv;
	return err;
}

int cable_digilent_init()
{
        int i;
        int cCodes = 0;
	BYTE rgbSetup[] = {0xaa, 0x22, 0x00};
        BYTE rgbTdo[4];
	INT32 idcode;
        INT32 rgIdcodes[16];
	int retval = APP_ERR_NONE;
        // DMGR API Call: DmgrOpen
        if(!DmgrOpen(&hif, "Nexys4DDR")) {
                printf("Error: Could not open device. Check device name\n");
                return(-1);
        }

        // DJTG API CALL: DjtgEnable
        if(!DjtgEnable(hif)) {
                printf("Error: DjtgEnable failed\n");
                return (-1);
        }

        /* Put JTAG scan chain in SHIFT-DR state. RgbSetup contains TMS/TDI bit-pairs. */
        // DJTG API Call: DgtgPutTmsTdiBits
        if(!DjtgPutTmsTdiBits(hif, rgbSetup, NULL, 9, fFalse)) {
                printf("DjtgPutTmsTdiBits failed\n");
                abort();
        }

        /* Get IDCODES from device until we receive a value of 0x00000000 */
        do {
                
                // DJTG API Call: DjtgGetTdoBits
                if(!DjtgGetTdoBits(hif, fFalse, fFalse, rgbTdo, 32, fFalse)) {
                        printf("Error: DjtgGetTdoBits failed\n");
                        abort();
                }

                // Convert array of bytes into 32-bit value
                idcode = (rgbTdo[3] << 24) | (rgbTdo[2] << 16) | (rgbTdo[1] << 8) | (rgbTdo[0]);

                // Place the IDCODEs into an array for LIFO storage
                rgIdcodes[cCodes] = idcode;

                cCodes++;

        } while( idcode != 0 );

        /* Show the IDCODEs in the order that they are connected on the device */
        printf("Ordered JTAG scan chain:\n");
        for(i=cCodes-2; i >= 0; i--) {
                printf("0x%08x\n", rgIdcodes[i]);
        }

	
  debug("DIGILENT cable ready!");

  return retval;
  /*
fail:
  jtag_close();

  return retval;
  */
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
  if(!DjtgGetTdoBits(hif, fFalse, fFalse, &rgbTdo, 1, fFalse)) {
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

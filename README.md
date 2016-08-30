# Advanced JTAG Bridge

This is a port of the advanced JTAG bridge from the [MinSoC
project](http://opencores.org/project,minsoc) to RISC-V. The port has been done
as part of the [PULP project](http://pulp.ethz.ch).

The bridge connects to a JTAG target and opens a local port for a remote gdb
connection. Supported targets include [RTL simulations via JTAG
DPI](https://github.com/ethz-iis/jtag_dpi), FPGA emulation and FTDI cables.

## Build

Run the following commands to the build the JTAG server:

    ./autogen.sh
    ./configure
    make

## Usage Example

The following command connects to JTAG tap 0 of a running RTL simulation and
listens for connections from GDB on port 1234. The VPI/DPI driver has been
started with port 4567.

    ./adv_jtag_bridge -x 0 -c 8 -l 0:4 -l 1:4 -g 1234 vpi -p 4567

## This version modified by jrrk2@cam.ac.uk for compatibility with digilent libraries. These 
need to be installed in the usual waY:

sudo dpkg -i /home/jrrk2/Downloads/digilent.adept.runtime_2.16.5-amd64.deb
sudo dpkg -i /home/jrrk2/Downloads/digilent.adept.utilities_2.2.1-amd64.deb
tar xzf /home/jrrk2/Downloads/digilent.adept.sdk_2.4.2.tar.gz
cd digilent.adept.sdk_2.4.2
sudo install.sh

These libraries, though slower than direct connection to the USB, provide interoperability with the way Vivado expects to work.

SoNIC for Terasic DE5-Net Port
=====
**/bin: config scripts**
- scripts to enable 5GT/s on PCIe Gen2, and disable virtual channel (must be executed before running sonic driver).

**/driver:**
- /working2: sonic device driver

**/firmware: sonic FPGA firmware**
- 1. you could download the firmware to FPGA flash using the flash_program_ub2.sh script (note: set correct path to quartus) This will survive poweroff.
- 2. you could program the PCIe_Fundamental.sof code to FPGA using Quartus Programmer. This will not survive poweroff.

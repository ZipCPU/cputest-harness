## This is the CPU Test-Harness project

The project is designed to take an (iCE40) binary image, named flash_image.bin
and placed in the main directory, and ...

1. Unpack it
2. Reformat it as a Verilog .v file
3. Run Verilator on that .v file
4. Add a main program to the file
5. Simulate the file as if it were run in actual hardware.

The project contains simulation files for both a [serial port](cpp/uartsim.cpp)
as well as a [Quad SPI flash](cpp/flashsim.cpp).  A single GPIO wire, `o_done`,
may also be set to end the simulation.  See the [.pcf](flash_image.pcf) for
example I/O connections to both the Serial port and the Quad SPI flash devices.

To use:

1. Arrange your design so that your I/O ports include those listed in the [pcf](flash_image.pcf) file.
2. Place your binary image in the main directory
3. Run make

## Dependencies

This project depends upon make, binutils, [g++](https://www.gcc.org), sed, [Verilator](https://www.veripool.org/wiki/verilator/), and [icestorm](http://www.clifford.at/icestorm) for proper functionality.

## License

This project is copyrighted by Gisselquist Technology, LLC.  It is licensed
under the LGPL.

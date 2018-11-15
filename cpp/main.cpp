////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	cpp/main.cpp
//
// Project:	CPU Test-Harness
//
// Purpose:	To demonstrate a useful Verilog file which could be used as a
//		toplevel program later, to demo the transmit UART as it might
//	be commanded from a WB bus, and having a FIFO.
//
//	If all goes well, the program will write out the words of the Gettysburg
//	address in interactive mode.  In non-interactive mode, the program will
//	read its own output and report on whether or not it worked well.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2018, Gisselquist Technology, LLC
//
// This file is part of the CPU Test-Harness project.
//
// The CPU Test-Harness project is free software (firmware): you can
// redistribute it and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// The CPU Test-Harness project is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
// General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  (It's in the $(ROOT)/doc directory.  Run make
// with no target there if the PDF file isn't present.)  If not, see
// <http://www.gnu.org/licenses/> for a copy.
//
// License:	LGPL, v3, as defined and found on www.gnu.org,
//		http://www.gnu.org/licenses/lgpl.html
//
//
////////////////////////////////////////////////////////////////////////////////
//
//
#include <verilatedos.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "Vflash_image.h"
#include "uartsim.h"
#include "flashsim.h"

#define	VBASE		Vflash_image
#define	BAUDRATE_HZ	115200
#define	CLOCKRATE_HZ	12000000
#define	CLOCKRATE_MHZ	(CLOCKRATE_HZ/1e6)
#define	DEF_BAUDCLOCKS	((int)(0.5 + CLOCKRATE_HZ / (double)BAUDRATE_HZ))
#define	DEF_NETPORT	9136	// Drawn from urandom


void	usage(void) {
	fprintf(stderr, "USAGE: main [-h] [options] [flash_file.bin]\n");
	fprintf(stderr, "\n"
"\n"
"\tSimulates a (compiled-in, Verilated) FPGA design having only a serial\n"
"\tport and a QSPI flash.\n"
"\n"
"\tThe serial port simulation will assume a serial data rate of\n"
"\t\"baud-clocks\" per baud.  This can be calculated by\n"
"\tclock_speed / baud rate.  The default value, %d, is appropriate for\n"
"\ta %.0f MHz clock running %.1f kBaud.\n"
"\n"
"\t-b\tUse the optional -b argument to change the number of clock ticks\n"
"\t\tper baud interval.\n"
"\n"
"\t-c <clockrate_hz>   Sets the assumed clock rate.  Useful for getting\n"
"\t	valid time measurements in VCD, and for setting the baud rate\n"
"\t	if baud clocks is not given.\n"
"\n"
"\t-d <filename>.vcd   Dumps internal wire transitions to the .vcd file\n"
"\t	for later viewing in GTKwave or other VCD viewer\n"
"\n"
"\t-f\tDebug the flash port\n"
"\n"
"\t-m <nclocks> Creates a maximum number of clock ticks before exiting.\n"
"\t	The default is to run until the o_done output is set.  With this\n"
"\t	option, the simulation will stop after <nclocks> have been\n"
"\t	completed.\n"
"\n"
"\t-n <port> Sets the TCP/IP port number for the simulated serial port\n"
"\t    I/O.\n"
"\n"
"\t    Serial port outputs will be sent both to the console as well as\n"
"\t    to TCP/IP port %d.  Inputs will be received on TCP/IP port %d\n"
"\t    only.  To send serial data to this design,\n"
"\n"
"\t	%% telnet localhost %d\n"
"\n"
"\t	and type in any data of interest.  A port number of zero will\n"
"\t	disable this capability.\n"
"\n"
"\t-p	Starts the flash in the deep power down mode\n"
"\n"
"\t-s <filename>	Creates a file of name <filename> and then dumps\n"
"\t	a copy of all serial port output to it.  By default, no serial\n"
"\t	port dump file will be created.\n"
"\n"
"\t[flash_file.bin] is the name of an (optional) binary flash image\n"
"\t    containing the information that would be found on the flash,\n"
"\n"
"\t-h\tDisplays this message\n"
"\t\n",
	DEF_BAUDCLOCKS, CLOCKRATE_MHZ, BAUDRATE_HZ/1e3,
	DEF_NETPORT, DEF_NETPORT, DEF_NETPORT);
};


#define	TBASSERT(TB,A) do { if (!(A)) { (TB).closetrace(); } assert(A); } while(0);

class TESTB {
public:
	Vflash_image	*m_core;
	VerilatedVcdC*	m_trace;
	uint64_t	m_time_ns;
	UARTSIM		*m_net, *m_console;
	FLASHSIM	*m_flash;
	bool		m_done;
	uint64_t	m_clock_period_ns;

	TESTB(int baudclocks, int netport, bool debug_flash=false)
				: m_time_ns(0l) {
		m_core = new Vflash_image;
		if (netport != 0) {
			m_net = new UARTSIM(netport);
			m_net->setup(baudclocks);
		} else
			m_net = NULL;
		m_console = new UARTSIM(0);
		m_console->setup(baudclocks);
		m_done = false;
		m_flash = new FLASHSIM(24, debug_flash);

		m_trace = NULL;
		Verilated::traceEverOn(true);
		m_core->i_clk = 0;
		eval(); // Get our initial values set properly.

		m_clock_period_ns = (uint64_t)(1.e9/CLOCKRATE_HZ);
	}

	virtual ~TESTB(void) {
		closetrace();
		delete m_core;
		delete m_console;
		delete m_net;
		m_core = NULL;
	}

	void	dump(FILE *fp) {
		// Record all serial-port output to a file
		m_console->dump(fp);
	}

	void	clockrate(int clockrate_hz) {
		m_clock_period_ns = (uint64_t)(1.e9 / clockrate_hz);
	}

	virtual	void	opentrace(const char *vcdname) {
		if (!m_trace) {
			m_trace = new VerilatedVcdC;
			m_core->trace(m_trace, 99);
			m_trace->open(vcdname);
		}
	}

	virtual	void	closetrace(void) {
		if (m_trace) {
			m_trace->close();
			delete m_trace;
			m_trace = NULL;
		}
	}

	virtual	void	eval(void) {
		m_core->eval();
	}

	virtual	void	tick(void) {
		m_time_ns += m_clock_period_ns;

		// Make sure we have our evaluations straight before the top
		// of the clock.  This is necessary since some of the 
		// connection modules may have made changes, for which some
		// logic depends.  This forces that logic to be recalculated
		// before the top of the clock.
		eval();
		if (m_trace) m_trace->dump((vluint64_t)(m_time_ns-2));
		m_core->i_clk = 1;
		eval();
		if (m_trace) m_trace->dump((vluint64_t)(m_time_ns));
		m_core->io_qspi_dat = (*m_flash)(m_core->o_qspi_csn,
				m_core->o_qspi_sck,
				m_core->io_qspi_dat);
		eval();
		if (m_trace) m_trace->dump((vluint64_t)(m_time_ns+2));
		m_core->i_clk = 0;
		eval();

		if (m_trace) {
			m_trace->dump((vluint64_t)(m_time_ns+m_clock_period_ns/2));
			m_trace->flush();
		}

		if (m_net) {
			m_core->i_uart_rx = (*m_net)(m_core->o_uart_tx);
			(*m_console)(m_core->o_uart_tx);
		} else
			m_core->i_uart_rx = (*m_console)(m_core->o_uart_tx);

		m_core->io_qspi_dat = (*m_flash)(m_core->o_qspi_csn,
				m_core->o_qspi_sck,
				m_core->io_qspi_dat);

		m_done = (m_done) || m_core->o_done;
	}

	unsigned long	time_ns(void) {
		return m_time_ns;
	}

	inline	bool	done(void) { return m_done; }
	inline	void	load(const char *fname) { m_flash->load(fname); }
	inline	bool	deep_sleep(bool v) { m_flash->deep_sleep(v); }
};


int	main(int argc, char **argv) {
	Verilated::commandArgs(argc, argv);
	int		netport = DEF_NETPORT;
	unsigned	baudclocks = 0, clockrate_hz = 0;
	const char *	flash_filename = NULL,
			*serialport_dump_filename = NULL,
			*vcd_filename = NULL;
	bool		verbose_flag = false, debug_flash = false,
			start_in_power_down = false;
	int		opt;
	unsigned	max_clocks = 0;

	while((opt = getopt(argc, argv, "hb:c:d:fs:n:p")) != -1) {
		switch(opt) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'b':
			baudclocks = atoi(optarg);
			break;
		case 'c':
			clockrate_hz = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			vcd_filename = strdup(optarg);
			break;
		case 'f':
			debug_flash = true;
			break;
		case 'm':
			max_clocks = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			netport = atoi(optarg);
			break;
		case 'p':
			start_in_power_down = true;
			break;
		case 's':
			serialport_dump_filename = strdup(optarg);
			break;
		case 'v':
			verbose_flag = true;
			break;
		default:
			fprintf(stderr, "ERR: invalid usage\n");
			usage();
			exit(EXIT_FAILURE);
		}
	}

	if (clockrate_hz == 0)
		clockrate_hz = CLOCKRATE_HZ; // The default
	if (baudclocks == 0)
		baudclocks = DEF_BAUDCLOCKS;

	TESTB		tb(baudclocks, netport, debug_flash);

	if ((argv[1])&&(strlen(argv[1])>4)
		&&(strcmp(&argv[1][strlen(argv[1])-4], ".bin")==0))
		flash_filename = argv[1];
	if ((flash_filename)&&(flash_filename[0])) {
		if (verbose_flag)
			printf("Loading flash image from %s\n", flash_filename);
		if (access(flash_filename, R_OK) != 0) {
			fprintf(stderr, "Cannot read flash image, %s\n",
				flash_filename);
			exit(EXIT_FAILURE);
		}
		tb.load(flash_filename);
	}

	tb.deep_sleep(start_in_power_down);

	if ((serialport_dump_filename)
		&&(strlen(serialport_dump_filename)>0)) {
		if (verbose_flag)
			printf("Forwarding serial-port output to %s\n",
				serialport_dump_filename);
		FILE	*fp = fopen(serialport_dump_filename, "w");
		if (!fp) {
			fprintf(stderr, "ERR: Could not open %s for writing\n",
				serialport_dump_filename);
			exit(EXIT_FAILURE);
		}
		tb.dump(fp);
	}

	if (vcd_filename != NULL) {
		tb.opentrace(vcd_filename);
		tb.clockrate(clockrate_hz);
	}

	tb.eval();
	if (max_clocks > 0) {
		for(unsigned clock_ticks=0;
			clock_ticks < max_clocks; clock_ticks++) {
			if (tb.done())
				break;
			tb.tick();
		}
	} else {
		while(!tb.done())
			tb.tick();
	}
}


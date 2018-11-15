################################################################################
##
## Filename: 	Makefile
##
## Project:	CPU Test-Harness
##
## Purpose:	
##
## Creator:	Dan Gisselquist, Ph.D.
##		Gisselquist Technology, LLC
##
################################################################################
##
## Copyright (C) 2018, Gisselquist Technology, LLC
##
## This file is part of the CPU Test-Harness project.
##
## The CPU Test-Harness project project is free software (firmware): you can
## redistribute it and/or modify it under the terms of the GNU Lesser General
## Public License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## The CPU Test-Harness project is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
## General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this program.  (It's in the $(ROOT)/doc directory.  Run make
## with no target there if the PDF file isn't present.)  If not, see
## <http://www.gnu.org/licenses/> for a copy.
##
## License:	LGPL, v3, as defined and found on www.gnu.org,
##		http://www.gnu.org/licenses/lgpl.html
##
##
################################################################################
##
##
.PHONY: all
all:
#
BASE    :=  flash_image
all: $(EXEFILE)
BINFILE :=  $(BASE).bin
PCFFILE := -p $(BASE).pcf
EXEFILE := $(BASE)_tb
#
#
#
.DELETE_ON_ERROR:
#
OBJDIR   := obj_dir
CPPD     := cpp
VERILATOR := verilator
VERILATOR_ROOT ?= $(shell bash -c '$(VERILATOR) -V|grep VERILATOR_ROOT | head -1 | sed -e "s/^.*=\s*//"')
VINCD    := $(VERILATOR_ROOT)/include
ASCFILE  := $(OBJDIR)/$(BASE).asc
VERILOG  := $(OBJDIR)/$(BASE).v
LIBFILE := $(OBJDIR)/V$(BASE)__ALL.a
YOSYSD   := `which yosys | sed -e "s/bin\/yosys/share\/yosys/"`
MAINCPP  := main
SIMFILES := uartsim flashsim
SIMSRCS  := $(addprefix $(CPPD)/,  $(addsuffix .cpp,$(MAINCPP) $(SIMFILES)))
SIMHDRS  := $(wildcard $(CPPD)/*.h)
SIMOBJS  := $(addprefix $(OBJDIR)/,$(addsuffix   .o,$(MAINCPP) $(SIMFILES)))
ICEVFLG  := -sc -n $(BASE)
VFLAGS   := -Wno-lint -Wno-fatal -Wno-style --top-module $(BASE) -cc
INCS := -I $(VINCD) -I $(CPPD) -I $(OBJDIR)/
CFLAGS := -g


test: $(EXEFILE)
	./$(EXEFILE) $(BINFILE)

$(ASCFILE): $(BINFILE)
	$(mk-objdir)
	iceunpack $(BINFILE)  $(ASCFILE)

$(VERILOG): $(ASCFILE) $(BASE).pcf
	icebox_vlog $(ICEVFLG) $(PCFFILE) $(ASCFILE) > $(VERILOG)
	sed -e 's/\\io_qspi/io_qspi/g' --in-place $(VERILOG)
	sed -e 's/^tran.*;//g' --in-place $(VERILOG)

$(OBJDIR)/V$(BASE).mk: $(VERILOG)
	echo $(YOSYSD)/ice40/cells_sim.v
	file $(YOSYSD)/ice40/cells_sim.v
	verilator $(VFLAGS) $(VERILOG) $(YOSYSD)/ice40/cells_sim.v

$(OBJDIR)/V$(BASE).h: $(OBJDIR)/V$(BASE).mk
$(OBJDIR)/main.o: $(OBJDIR)/V$(BASE).mk

$(OBJDIR)/V$(BASE)__ALL.a:
	make -C $(OBJDIR)/ -f V$(BASE).mk

$(OBJDIR)/verilated.o: $(VINCD)/verilated.cpp
	$(mk-objdir)
	g++ $(CFLAGS) -c -I $(VINCD) $< -o $(OBJDIR)/verilated.o

$(OBJDIR)/%.o: $(CPPD)/%.cpp
	$(mk-objdir)
	g++ $(CFLAGS) -c -I $(VINCD) -I $(CPPD) -I $(OBJDIR)/ $< -o $(OBJDIR)/$*.o

$(EXEFILE): $(OBJDIR)/verilated.o $(OBJDIR)/V$(BASE)__ALL.a
$(EXEFILE): $(SIMOBJS)
	g++ $(CFLAGS) -I $(VINCD) -I $(OBJDIR)/ -I $(CPPD) $(SIMOBJS) $(LIBFILE) $(OBJDIR)/verilated.o -o $(EXEFILE)

define	build-depends
	$(mk-objdir)
	@echo "Building dependency file"
	@$(CXX) $(CFLAGS) $(INCS) -MM $(SIMSRCS) > $(OBJDIR)/xdepends.txt
	@sed -e 's/^.*.o: /$(OBJDIR)\/&/' <$(OBJDIR)/xdepends.txt > $(OBJDIR)/depends.txt
	@rm $(OBJDIR)/xdepends.txt
endef

$(OBJDIR)/depends.txt: $(OBJDIR)/V$(BASE).mk
$(OBJDIR)/depends.txt: $(wildcard $(CPPD)/*.cpp) $(wildcard $(CPPD)/*.h)
	$(build-depends)

.PHONY: depends
depends: $(OBJDIR)/depends.txt

define	mk-objdir
	@bash -c "if [ ! -e $(OBJDIR) ]; then mkdir -p $(OBJDIR); fi"
endef

clean:
	rm -rf $(OBJDIR)

ifneq	($(MAKECMDGOALS),clean)
-include $(OBJDIR)/depends.txt
endif

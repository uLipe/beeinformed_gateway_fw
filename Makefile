#
# Simple make recipe to build beeinformed_gateway proect
# Author: The Beeinformed Team
#
CC=gcc
LD=gcc
GDB=gdb
CFLAGS=-g -c -O3 -Ibeeinfo_include
LDFLAGS=-g  
OUTFILE?=beeinformed_fw
#
# Add source directories:
#


#add main or other c file stuff
SRC += $(wildcard *.c)

#
# Define linker script files:
#
LIBS = -lbluetooth  -lpthread  -lgattlib

#
# .c to .o recursion magic:
#
OBJS  = $(SRC:.c=.o)

#
# Define the build chain:
#
.PHONY: all, clean

all: $(OUTFILE).out
	@echo "[BIN]: Generated the $(OUTFILE).out binary file!"

clean:
	@echo "[CLEAN]: Cleaning !"
	@rm -f  *.o
	@rm -f  *.out	
	@echo "[CLEAN]: Done !"


#
# Linking step:
#
$(OUTFILE).out: $(OBJS)
	@echo "[LD]: Linking files!"
	@$(LD) $(LDFLAGS)  $(OBJS) $(LIBS) -o $@
	@echo "[LD]: Cleaning intermediate files!"
	@rm -f  *.o
#
# Compiling step:
#
.c.o:
	@echo "[CC]: $< "
	@$(CC) $(CFLAGS) -o $@  $<
# Example Makefile for ROSE users
# This makefile is provided as an example of how to use ROSE when ROSE is
# installed (using "make install").  This makefile is tested as part of the
# "make distcheck" rule (run as part of tests before any SVN checkin).
# The test of this makefile can also be run by using the "make installcheck"
# rule (run as part of "make distcheck").

# ROSE install directory
ROSE_HOME =  $(HOME)/edg4x-edgsrc-rose-inst
# Location of include directory after "make install"
ROSE_INCLUDE_DIR = $(ROSE_HOME)/include
# Location of library directory after "make install"
ROSE_LIB_DIR =  $(ROSE_HOME)/lib
# Location of Boost include directory
BOOST_CPPFLAGS = -I$(HOME)/include

CC                    = gcc
CXX                   = g++
CPPFLAGS              = $(BOOST_CPPFLAGS) -I$(ROSE_INCLUDE_DIR) -I$(HOME)/glibc-inst/include 
#CXXCPPFLAGS           = @CXXCPPFLAGS@
CXXFLAGS              = -g -Wall 
LDFLAGS               = -L$(ROSE_LIB_DIR) -L$(HOME)/lib -L$(HOME)/glibc-inst/lib -static -pthread -Wl,--start-group -lpthread_nonshared -lboost_system -lboost_wave -lhpdf -lrose -lm -lboost_date_time -lboost_thread -lboost_filesystem -lgcrypt -lgpg-error -lboost_program_options -lboost_regex dlstubs.o -Wl,--end-group 

# Location of source code
ROSE_SOURCE_DIR = ./src

executableFiles = functionLocator printRoseAST pdt_roseparse preproc nodeFromHandle swap_test

default: pdt_roseparse

# Default make rule to use
all: $(executableFiles)
	@if [ x$${ROSE_IN_BUILD_TREE:+present} = xpresent ]; then echo "ROSE_IN_BUILD_TREE should not be set" >&2; exit 1; fi

clean:
	rm -f $(executableFiles) *.o

$(executableFiles): dlstubs.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(ROSE_SOURCE_DIR)/$@.C $(LDFLAGS) 

dlstubs.o: dlstubs.c
	$(CC) -c dlstubs.c

# Example Makefile for ROSE users
# This makefile is provided as an example of how to use ROSE when ROSE is
# installed (using "make install").  This makefile is tested as part of the
# "make distcheck" rule (run as part of tests before any SVN checkin).
# The test of this makefile can also be run by using the "make installcheck"
# rule (run as part of "make distcheck").

# ROSE install directory
ROSE_HOME =  $(HOME)/edg4x-edgsrc-rose-inst
BOOST_HOME ?= $(HOME)
# Location of include directory after "make install"
ROSE_INCLUDE_DIR = $(ROSE_HOME)/include
# Location of library directory after "make install"
ROSE_LIB_DIR =  $(ROSE_HOME)/lib
# Location of Boost include directory
BOOST_CPPFLAGS = -I$(BOOST_HOME)/include

CC                    = gcc
CXX                   = g++
CPPFLAGS              = $(BOOST_CPPFLAGS) -I$(ROSE_INCLUDE_DIR) 
# -I$(HOME)/glibc-inst/include 
#CXXCPPFLAGS           = @CXXCPPFLAGS@
CXXFLAGS              = -g -Wall 
LDFLAGS               = -L$(ROSE_LIB_DIR) -L$(BOOST_HOME)/lib -L$(BOOST_HOME)/glibc-inst/lib -static -pthread -Wl,--start-group -lpthread_nonshared -lboost_system -lboost_wave -lhpdf -lrose -lm -lboost_date_time -lboost_thread -lboost_filesystem -lgcrypt -lgpg-error -lboost_program_options -lboost_regex dlstubs.o -Wl,--end-group 

# Apple LDFLAGS

# NOTE: Before building, please move the .dylib files out of $(BOOST_HOME/lib directory on Apple so it uses .a files instead. 
LDFLAGS               = -L$(ROSE_LIB_DIR) -L$(BOOST_HOME)/lib -L$(BOOST_HOME)/glibc-inst/lib  -lpthread -lboost_system -lboost_wave -lhpdf -lrose -lm -lboost_date_time -lboost_thread -lboost_filesystem -lgcrypt -lgpg-error -lboost_program_options -lboost_regex dlstubs.o ./Apple/libstdc++.a  ./Apple/libgcc_eh.a


# Location of source code
ROSE_SOURCE_DIR = ./src

executableFiles = functionLocator printRoseAST edg44-pdt_roseparse preproc nodeFromHandle swap_test

default: edg44-pdt_roseparse

# Default make rule to use
all: $(executableFiles)
	@if [ x$${ROSE_IN_BUILD_TREE:+present} = xpresent ]; then echo "ROSE_IN_BUILD_TREE should not be set" >&2; exit 1; fi

clean:
	rm -f $(executableFiles) *.o
	rm -f $(BOOST_HOME)/lib/*.dylib

$(executableFiles): dlstubs.o
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(ROSE_SOURCE_DIR)/$@.C $(LDFLAGS) 
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(ROSE_SOURCE_DIR)/$@.C 
	/usr/llvm-gcc-4.2/bin/../libexec/gcc/i686-apple-darwin11/4.2.1/collect2 -dynamic -arch x86_64 -macosx_version_min 10.8.3 -weak_reference_mismatches non-weak -o foo -lcrt1.10.6.o -L/usr/llvm-gcc-4.2/bin/../lib/gcc/i686-apple-darwin11/4.2.1/x86_64 -L/Applications/Xcode.app/Contents/Developer/usr/llvm-gcc-4.2/lib/gcc/i686-apple-darwin11/4.2.1/x86_64 -L/usr/llvm-gcc-4.2/bin/../lib/gcc/i686-apple-darwin11/4.2.1 -L/usr/llvm-gcc-4.2/bin/../lib/gcc -L/Applications/Xcode.app/Contents/Developer/usr/llvm-gcc-4.2/lib/gcc/i686-apple-darwin11/4.2.1 -L/usr/llvm-gcc-4.2/bin/../lib/gcc/i686-apple-darwin11/4.2.1/../../.. -L/Applications/Xcode.app/Contents/Developer/usr/llvm-gcc-4.2/lib/gcc/i686-apple-darwin11/4.2.1/../../.. -o $@ $@.o $(LDFLAGS) 

dlstubs.o: dlstubs.c
	$(CC) -c dlstubs.c

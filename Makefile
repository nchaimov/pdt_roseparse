# Example Makefile for ROSE users
# This makefile is provided as an example of how to use ROSE when ROSE is
# installed (using "make install").  This makefile is tested as part of the
# "make distcheck" rule (run as part of tests before any SVN checkin).
# The test of this makefile can also be run by using the "make installcheck"
# rule (run as part of "make distcheck").


# Location of include directory after "make install"
ROSE_INCLUDE_DIR = $(HOME)/edg4x-edgsrc-rose-inst/include

# Location of Boost include directory
BOOST_CPPFLAGS = -I$(HOME)/include

# Location of Dwarf include and lib (if ROSE is configured to use Dwarf)
ROSE_DWARF_INCLUDES = 
ROSE_DWARF_LIBS_WITH_PATH = 

# Location of library directory after "make install"
ROSE_LIB_DIR =  $(HOME)/edg4x-edgsrc-rose-inst/lib

ROSE_HOME =  $(HOME)/edg4x-edgsrc-rose-inst

CC                    = gcc
CXX                   = g++
CPPFLAGS              = $(BOOST_CPPFLAGS) 
#CXXCPPFLAGS           = @CXXCPPFLAGS@
CXXFLAGS              = -g -Wall 
LDFLAGS               = 

#ROSE_LIBS = $(ROSE_LIB_DIR)/librose.la

# Location of source code
ROSE_SOURCE_DIR = ./src
 

executableFiles = functionLocator printRoseAST pdt_roseparse preproc nodeFromHandle


# Default make rule to use
all: $(executableFiles)
	@if [ x$${ROSE_IN_BUILD_TREE:+present} = xpresent ]; then echo "ROSE_IN_BUILD_TREE should not be set" >&2; exit 1; fi

clean:
	rm -f $(executableFiles)

# -L/usr/lib/x86_64-redhat-linux5E/lib64/ 
# and smaller than linking to static libraries).  Dynamic linking requires the 
# use of the "-L$(ROSE_LIB_DIR) -Wl,-rpath" syntax if the LD_LIBRARY_PATH is not
# modified to use ROSE_LIB_DIR.  We provide two example of this; one using only
# the "-lrose -ledg" libraries, and one using the many separate ROSE libraries.
$(executableFiles): 
#	g++ -m32 -I$(ROSE_INCLUDE_DIR) -o $@ $(ROSE_SOURCE_DIR)/$@.C -L$(ROSE_LIB_DIR) -Wl,-rpath $(ROSE_LIB_DIR) $(ROSE_LIBS)
	$(CXX) -I$(ROSE_INCLUDE_DIR) -I$(HOME)/glibc-inst/include $(CPPFLAGS) $(CXXFLAGS) -o $@ $(ROSE_SOURCE_DIR)/$@.C $(LIBS_WITH_RPATH) -L$(HOME)/edg4x-edgsrc-rose-inst/lib -L$(HOME)/lib -L$(ROSE_LIB_DIR) -L$(HOME)/glibc-inst/lib -static -pthread -Wl,--start-group -lpthread_nonshared -lboost_system -lboost_wave -lhpdf -lrose -lm -lboost_date_time -lboost_thread -lboost_filesystem -lgcrypt -lgpg-error -lboost_program_options -lboost_regex dlstubs.o -Wl,--end-group
#	/bin/sh ../libtool --mode=link $(CXX) $(CPPFLAGS) $(CXXFLAGS)  $(LDFLAGS) -I$(ROSE_INCLUDE_DIR) $(BOOST_CPPFLAGS) -o $@ $(ROSE_SOURCE_DIR)/$@.C $(ROSE_LIBS)
#	/bin/sh $(ROSE_HOME)/libtool --mode=link $(CXX) $(CPPFLAGS) $(CXXFLAGS)  $(LDFLAGS) -I$(ROSE_INCLUDE_DIR) $(BOOST_CPPFLAGS) $(ROSE_DWARF_INCLUDES) -o $@ $(ROSE_SOURCE_DIR)/$@.C $(ROSE_LIBS) $(ROSE_DWARF_LIBS_WITH_PATH)



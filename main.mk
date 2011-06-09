###############################################################################
# The following macros should be defined before this script is
# invoked:
#
# TOP              The toplevel directory of the source tree.  
#
# BCC              C Compiler and options for use in building executables that
#                  will run on the platform that is doing the build.
#
# OPTS             Extra compiler command-line options.
#
# EXE              The suffix to add to executable files.  ".exe" for windows
#                  and "" for Unix.
#
# TCC              C Compiler and options for use in building executables that 
#                  will run on the target platform.  This is usually the same
#                  as BCC, unless you are cross-compiling.
#
# AR               Tools used to build a static library.
# RANLIB
#
# TCL_FLAGS        Extra compiler options needed for programs that use the
#                  TCL library.
#
# LIBTCL           Linker options needed to link against the TCL library.
#
# Once the macros above are defined, the rest of this make script will
# build the product
################################################################################

# This is how we compile
#
TCCX =  $(TCC) $(OPTS) -I. -I$(TOP)/src -I$(TOP) 

# Object files for the SQLite library.
#
LIBOBJ+= parse.o

# All of the source code files.
#
SRC = \
  $(TOP)/src/parse.y 

# Generated source code files
#
SRC += \
  parse.c \
  parse.h

# Header files used by all library source files.
#
HDR = \
   parse.h

# This is the default Makefile target.
#
all:	parse.c

# Rules to build the LEMON compiler generator
#
lemon:	$(TOP)/tool/lemon.c $(TOP)/src/lempar.c
	$(BCC) -o lemon $(TOP)/tool/lemon.c
	cp $(TOP)/src/lempar.c .

# Rules to build individual *.o files from generated *.c files. This
# applies to:
#
#     parse.o
#     opcodes.o
#
%.o: %.c $(HDR)
	$(TCCX) -c $<

# Rules to build individual *.o files from files in the src directory.
#
%.o: $(TOP)/src/%.c $(HDR)
	$(TCCX) -c $<

# Rules to build parse.c and parse.h - the outputs of lemon.
#
parse.h:	parse.c

parse.c:	$(TOP)/src/parse.y lemon $(TOP)/addopcodes.awk
	cp $(TOP)/src/parse.y .
	rm -f parse.h
	./lemon $(OPTS) parse.y
	mv parse.h parse.h.temp
	$(NAWK) -f $(TOP)/addopcodes.awk parse.h.temp >parse.h

clean:	
	rm -f *.o
	rm -f lemon lempar.c parse.*

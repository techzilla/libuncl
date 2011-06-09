#!/usr/make
#
#### The toplevel directory of the source tree.
#
TOP = ../xjd1

#### C Compiler and options for use in building executables that
#    will run on the platform that is doing the build.
#
BCC = gcc -g

#### The suffix to add to executable files.  ".exe" for windows.
#    Nothing for unix.
#
#EXE = .exe
EXE =

#### C Compile and options for use in building executables that 
#    will run on the target platform.  This is usually the same
#    as BCC, unless you are cross-compiling.
#
TCC = gcc -Wall -g
#TCC = /opt/mingw/bin/i386-mingw32-gcc -Os

#### Tools used to build a static library.
#
AR = ar cr
#AR = /opt/mingw/bin/i386-mingw32-ar cr
RANLIB = ranlib
#RANLIB = /opt/mingw/bin/i386-mingw32-ranlib

### Auxiliary libraries needed to build the shell
#
AUXLIB =

#### Extra compiler options needed for programs that use the TCL library.
#
#TCL_FLAGS =
#TCL_FLAGS = -DSTATIC_BUILD=1
TCL_FLAGS = -I/home/drh/tcltk/8.5linux
#TCL_FLAGS = -I/home/drh/tcltk/8.5win -DSTATIC_BUILD=1
#TCL_FLAGS = -I/home/drh/tcltk/8.3hpux

#### Linker options needed to link against the TCL library.
#
#LIBTCL = -ltcl -lm -ldl
LIBTCL = /home/drh/tcltk/8.5linux/libtcl8.5g.a -lm -ldl
#LIBTCL = /home/drh/tcltk/8.5win/libtcl85s.a -lmsvcrt
#LIBTCL = /home/drh/tcltk/8.3hpux/libtcl8.3.a -ldld -lm -lc

#### Additional objects for library when TCL support is enabled.
#TCLOBJ =
TCLOBJ = tclxjd1.o

# You should not have to change anything below this line
###############################################################################
include $(TOP)/main.mk

# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES =  -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src/ -I$(top_srcdir)/src/utils -I$(SX_COMPLIB_PATH)/include \
	    -I$(MLNX_LIB_PATH)/include -I$(srcdir) -I$(OES_PATH)/OES/

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmlaginternalapi.la

libmlaginternalapi_la_SOURCES = mlag_internal_api.c 

libmlaginternalapi_la_LIBADD= -L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
			      -L$(MLNX_LIB_PATH)/lib/ -lctrllearn \
			      -L$(SX_COMPLIB_PATH)/lib -lsxrpc \
			      -ldl

# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src/ -I$(srcdir) -I$(SX_COMPLIB_PATH)/include -I$(OES_PATH)/OES -I$(top_srcdir)/src/utils\
		-I$(MLNX_LIB_PATH)/include -I$(OES_PATH)/OES/

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libservicelayer.la

libservicelayer_la_SOURCES = service_layer.c

libservicelayer_la_LIBADD= -L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
                    	 -L$(MLNX_LIB_PATH)/lib/ -leventdisp \
			 -ldl

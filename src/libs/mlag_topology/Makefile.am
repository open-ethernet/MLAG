# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src/ -I$(top_srcdir)/src/utils/ -I$(srcdir) -I$(SX_COMPLIB_PATH)/include \
		-I$(MLNX_LIB_PATH)/include -I$(FSM_LIB_PATH)/include -I$(OES_PATH)/OES/

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmlagtopo.la

libmlagtopo_la_SOURCES =  mlag_topology.c

libmlagtopo_la_LIBADD= -L../mlag_common/.libs/ -lmlagcommon \
			 -L../mlag_manager/.libs/ -lmlagmgr \
			 -L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
			 -L$(FSM_LIB_PATH)/lib/ -lFsmFramework \
			 -L$(MLNX_LIB_PATH)/lib/ -lcommu -leventdisp \
	                 -ldl

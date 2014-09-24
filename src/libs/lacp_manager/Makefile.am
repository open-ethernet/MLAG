# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src/ -I$(srcdir) -I$(SX_COMPLIB_PATH)/include \
		-I$(top_srcdir)/src/libs/mlag_common/ -I$(top_srcdir)/src/utils/ \
		-I$(MLNX_LIB_PATH)/include/mlnx_lib  -I$(FSM_LIB_PATH)/include/FSM_Tool \
		-I$(OES_PATH)/OES/ -I$(top_srcdir)/src/libs/service_layer

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmlaglacpmgr.la

libmlaglacpmgr_la_SOURCES = lacp_db.c lacp_manager.c 

libmlaglacpmgr_la_LIBADD= 	-L../mlag_common/.libs/ -lmlagcommon \
				-L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
				-L$(MLNX_LIB_PATH)/lib/ -leventdisp -lcommu \
				-ldl

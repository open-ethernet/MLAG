# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES =	-I$(top_srcdir) \
			-I$(top_srcdir)/include \
			-I$(top_srcdir)/src/ \
			-I$(srcdir) \
			-I$(SX_COMPLIB_PATH)/include \
			-I$(MLNX_LIB_PATH)/include \
			-I$(FSM_LIB_PATH)/include \
			-I$(OES_PATH)/OES/ \
			-I$(top_srcdir)/src/libs/mlag_common \
			-I$(top_srcdir)/src/libs/mlag_manager \
			-I$(top_srcdir)/src/libs/mlag_master_election \
			-I$(top_srcdir)/src/libs/service_layer \
			-I$(top_srcdir)/src/libs/health_manager \
			-I$(top_srcdir)/src/libs/mlag_master_election \
			-I$(top_srcdir)/src/libs/mlag_topology/ \
			-I$(top_srcdir)/src/libs/port_manager/ \
			-I$(MLNX_LIB_PATH)/include/mlnx_lib

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmlagtnl.la

libmlagtnl_la_SOURCES =  mlag_tunneling.c

libmlagtnl_la_LIBADD= -L../mlag_common/.libs/ -lmlagcommon \
			 -L../mlag_manager/.libs/ -lmlagmgr \
			 -L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
			 -L$(FSM_LIB_PATH)/lib/ -lFsmFramework \
			 -L$(MLNX_LIB_PATH)/lib/ -lcommu -leventdisp \
	                 -ldl

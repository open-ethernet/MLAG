# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src/ -I$(top_srcdir)/src/utils/ -I$(srcdir) -I$(SX_COMPLIB_PATH)/include \
		   -I$(top_srcdir)/src/libs/mlag_master_election -I$(top_srcdir)/src/libs/health_manager \
		   -I$(top_srcdir)/src/libs/mlag_l3_interface_manager -I$(FSM_LIB_PATH)/include/FSM_Tool/ \
		   -I$(top_srcdir)/src/libs/mlag_common -I$(MLNX_LIB_PATH)/include/mlnx_lib -I$(OES_PATH)/OES/


if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmlagmgr.la

libmlagmgr_la_SOURCES = mlag_peering_fsm.c mlag_manager_db.c mlag_dispatcher.c mlag_manager.c

libmlagmgr_la_LIBADD= 	-L../mlag_common/.libs/ -lmlagcommon \
			-L../port_manager/.libs -lmlagportmgr \
			-L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
			-L$(MLNX_LIB_PATH)/lib -leventdisp -lcommu \
			-L$(FSM_LIB_PATH)/lib/ -lFsmFramework \
			-ldl

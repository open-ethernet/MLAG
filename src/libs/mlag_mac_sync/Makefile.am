# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include \
           -I$(top_srcdir)/src/ -I$(top_srcdir)/src/utils -I$(top_srcdir)/src/libs/health_manager \
           -I$(srcdir) -I$(SX_COMPLIB_PATH)/include \
           -I$(FSM_LIB_PATH)/include/FSM_Tool \
           -I$(top_srcdir)/src/libs/mlag_common \
           -I$(top_srcdir)/src/libs/mlag_manager \
           -I$(top_srcdir)/src/libs/port_manager \
           -I$(top_srcdir)/src/libs/mlag_master_election \
           -I$(top_srcdir)/src/libs/mlag_topology \
           -I$(MLNX_LIB_PATH)/include/mlnx_lib  \
           -I$(OES_PATH)/OES/ 

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmlagmacsync.la

libmlagmacsync_la_SOURCES =  mlag_mac_sync_dispatcher.c mlag_mac_sync_manager.c mlag_mac_sync_peer_manager.c \
            mlag_mac_sync_master_logic.c mlag_mac_sync_flush_fsm.c mlag_mac_sync_router_mac_db.c

libmlagmacsync_la_LIBADD= 	-L../mlag_common/.libs/ -lmlagcommon \
			-L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
			-L$(MLNX_LIB_PATH)/lib -leventdisp -lcommu -lctrllearn\
			 -L$(FSM_LIB_PATH)/lib/ -lFsmFramework \
			 -ldl

		
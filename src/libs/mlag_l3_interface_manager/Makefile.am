# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src/ -I$(top_srcdir)/src/utils \
           -I$(top_srcdir)/src/libs/health_manager -I$(top_srcdir)/src/libs/mlag_master_election \
           -I$(top_srcdir)/src/libs/mlag_common -I$(top_srcdir)/src/libs/mlag_manager \
           -I$(top_srcdir)/src/libs/service_layer -I$(top_srcdir)/src/libs/mlag_topology \
           -I$(srcdir) -I$(SX_COMPLIB_PATH)/include -I$(MLNX_LIB_PATH)/include -I$(OES_PATH)/OES

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libl3interface.la

libl3interface_la_SOURCES = mlag_l3_interface_manager.c mlag_l3_interface_peer.c \
                            mlag_l3_interface_master_logic.c

libl3interface_la_LIBADD= -L../mlag_common/.libs/ -lmlagcommon \
			-L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
			-L$(MLNX_LIB_PATH)/lib -leventdisp -lcommu \
		 	-L../mlag_master_election/.libs/ -lmasterelection \
		    -L../mlag_manager/.libs/ -lmlagmgr \
		    -L$(SL_LIB_PATH_FULL_PREFIX) -l${SL_LIB_NAME_FULL_PREFIX} \
			-ldl

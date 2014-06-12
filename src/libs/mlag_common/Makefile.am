# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src/ -I$(srcdir) -I$(SX_COMPLIB_PATH)/include \
		   -I$(top_srcdir)/src/utils -I$(top_srcdir)/src/libs/mlag_manager \
           -I$(top_srcdir)/src/libs/mlag_master_election \
           -I$(MLNX_LIB_PATH)/include/mlnx_lib \
           -I$(OES_PATH)/OES/ \
           -I$(top_srcdir)/../mlnx_lib/libcommu

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmlagcommon.la

libmlagcommon_la_SOURCES = mlag_common.c mlag_comm_layer_wrapper.c

libmlagcommon_la_LIBADD= -L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
                    	 -L$(MLNX_LIB_PATH)/lib/ -leventdisp -lcommu \
			 -ldl

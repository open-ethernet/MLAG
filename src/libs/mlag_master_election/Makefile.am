# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include \
           -I$(top_srcdir)/src/ -I$(top_srcdir)/src/utils -I$(top_srcdir)/src/libs/health_manager \
           -I$(srcdir) -I$(SX_COMPLIB_PATH)/include -I$(FSM_LIB_PATH)/include/FSM_Tool/ \
           -I$(MLNX_LIB_PATH)/include -I$(top_srcdir)/src/libs/mlag_common -I$(OES_PATH)/OES/ \
           -I$(top_srcdir)/src/libs/mlag_manager 

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmasterelection.la

libmasterelection_la_SOURCES = mlag_master_election.c mlag_master_election_fsm.c

libmasterelection_la_LIBADD= -L../mlag_common/.libs/ -lmlagcommon \
            	-L../mlag_manager/.libs/ -lmlagmgr \
				-L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
				-L$(MLNX_LIB_PATH)/lib/ -leventdisp \
				-L$(FSM_LIB_PATH)/lib/ -lFsmFramework \
				-ldl
			

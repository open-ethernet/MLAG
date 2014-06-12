# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(OES_PATH)/OES -I$(top_srcdir)/src/ -I$(top_srcdir)/src/utils -I$(top_srcdir)/src/libs/mlag_internal_api -I$(SX_COMPLIB_PATH)/include -I$(srcdir)

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

lib_LTLIBRARIES = libmlagapi.la

libmlagapi_la_SOURCES = mlag_api.c

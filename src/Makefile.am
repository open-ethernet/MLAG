# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/src/libs/ -I$(top_srcdir)/include -I$(top_srcdir)/src/utils/ -I$(srcdir) \
           -I$(SX_COMPLIB_PATH)/include -I$(MLNX_LIB_PATH)/include -I$(top_srcdir)/src/libs/mlag_internal_api \
	   -I$(top_srcdir)/src/libs/mlag_common -I$(top_srcdir)/src/libs/port_manager \
	   -I$(top_srcdir)/src/libs/mlag_master_election -I$(OES_PATH)/OES/

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

bin_PROGRAMS = mlag

mlag_SOURCES = mlag_conf.c mlag_init.c mlag_main.c 

CFLAGS = @CFLAGS@ $(CFLAGS_MLAG_COMMON) $(DBGFLAGS) -pthread

mlag_LDADD = 	-Llibs/mlag_manager/.libs/ -lmlagmgr \
		-Llibs/port_manager/.libs/ -lmlagportmgr \
		-Llibs/health_manager/.libs/ -lmlaghealth \
		-Llibs/mlag_tunneling/.libs/ -lmlagtnl \
		-L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog \
		-L$(MLNX_LIB_PATH)/lib -lcommu -lctrllearn \
		-L$(CTRL_LEARN_HAL_LIB_PATH_FULL_PREFIX) -l${CTRL_LEARN_HAL_LIB_NAME_FULL_PREFIX}\
		-L$(OES_LIB_PATH_FULL_PREFIX) -l${OES_LIB_NAME_FULL_PREFIX}\
		-Llibs/mlag_master_election/.libs/ -lmasterelection \
		-Llibs/mlag_mac_sync/.libs/ -lmlagmacsync \
		-Llibs/mlag_l3_interface_manager/.libs/ -ll3interface \
		-Llibs/mlag_internal_api/.libs/ -lmlaginternalapi \
		-L$(SL_LIB_PATH_FULL_PREFIX) -l${SL_LIB_NAME_FULL_PREFIX} \
		-L$(NL_LIB_PATH_FULL_PREFIX) -l${NL_LIB_NAME_FULL_PREFIX} \
		-Llibs/mlag_topology/.libs/ -lmlagtopo \
		-Llibs/mlag_common/.libs/ -lmlagcommon \
		-Llibs/port_manager/.libs/ -lmlagportmgr \
		-Llibs/lacp_manager/.libs/ -lmlaglacpmgr \
		$(EXTRA_MLAG_LDADD) \
		-ldl

mlag_LDFLAGS = -rdynamic


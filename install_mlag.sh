#!/bin/bash
SCRIPT_PATH="${BASH_SOURCE[0]}";
if ([ -h "${SCRIPT_PATH}" ]) then
  while([ -h "${SCRIPT_PATH}" ]) do SCRIPT_PATH=`readlink "${SCRIPT_PATH}"`; done
fi
pushd . > /dev/null
cd `dirname ${SCRIPT_PATH}` > /dev/null
SCRIPT_PATH=`pwd`;
popd  > /dev/null

##Clone Repositories
git clone https://github.com/evgenyi1/FSM-tool $SCRIPT_PATH/../FSM-tool
git clone https://github.com/Mellanox/mlnx_lib $SCRIPT_PATH/../mlnx_lib
git clone https://github.com/open-ethernet/OES $SCRIPT_PATH/../OES

## Install mlnx_lib
cd $SCRIPT_PATH/../mlnx_lib
./autogen.sh
./configure --with-oes=$SCRIPT_PATH/../OES
cd sx_complib
./autogen.sh
./configure
make && make install
cd ..
make && make install

## Install FSM-tool
cd $SCRIPT_PATH/../FSM-tool
./autogen.sh
./configure
make && make install

## Install OES
cd $SCRIPT_PATH/../OES/OES
make && make install

## Install MLAG
cd $SCRIPT_PATH 
./autogen.sh
./configure --with-sxcomplib=/usr/local/ --with-mlnx-lib=/usr/local/ --with-fsmlib=/usr/local/ \
	--with-oes=$SCRIPT_PATH/../OES --with-oes-lib-path=/usr/local/lib/ --with-oes-lib-name=oesstub \
	--with-ctrl-learn-hal-lib-path=/usr/local/lib/ --with-ctrl-learn-hal-lib-name=ctrllearnhalstub \
	--with-sl-lib-path=$SCRIPT_PATH/src/libs/service_layer/.libs/ --with-sl-lib-name=servicelayer \
	--with-nl-lib-path=$SCRIPT_PATH/src/libs/notification_layer/.libs/ --with-nl-lib-name=mlagnotify
make && make install


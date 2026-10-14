#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

# # ############################# 
#1. Install Dependencies and packages

apt update
apt install -y libsqlite3-dev libcurl4-openssl-dev valgrind lcov clang libsystemd-dev libboost-all-dev libwebsocketpp-dev meson libcunit1 libcunit1-dev curl protobuf-compiler-grpc libgrpc-dev libgrpc++-dev libunwind-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
pip install jsonref

############################
# Build trevor-base64
if [ ! -d "trower-base64" ]; then
git clone https://github.com/xmidt-org/trower-base64.git
fi
cd trower-base64
meson setup --warnlevel 3 --werror build
ninja -C build
ninja -C build install
cd ..
###########################################
# Clone the required repositories

THUNDER_TOOLS_COMMIT_SHA="d5dd83c7c19c49c7f25c558c126500bd2d64f7a4"
THUNDER_COMMIT_SHA="2c0fcc5529e7da734be558ca6efa05d934dcce31"

git clone --branch R4_4-RDK https://github.com/rdkcentral/ThunderTools.git
cd ThunderTools
git checkout $THUNDER_TOOLS_COMMIT_SHA
cd ..

git clone --branch R4_4-RDK https://github.com/rdkcentral/Thunder.git
cd Thunder
git checkout $THUNDER_COMMIT_SHA
cd ..

git clone --branch develop https://github.com/rdkcentral/entservices-apis.git

git clone --branch feature/RDKEMW-25013 https://github.com/rdkcentral/entservices-testframework.git

############################
# Build Thunder-Tools
echo "======================================================================================"
echo "buliding thunderTools"

cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \

cmake --build build/ThunderTools --target install


############################
# Build Thunder
echo "======================================================================================"
echo "buliding thunder"

cmake -G Ninja -S Thunder -B build/Thunder \
    -DMESSAGING=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DBUILD_TYPE=Debug \
    -DBINDING=127.0.0.1 \
    -DPORT=55555 \
    -DEXCEPTIONS_ENABLE=ON \

cmake --build build/Thunder --target install


############################
# Build entservices-apis
echo "======================================================================================"
echo "buliding entservices-apis"
cd entservices-apis
rm -rf jsonrpc/DTV.json
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-1007.patch
cd ..

cmake -G Ninja -S entservices-apis  -B build/entservices-apis \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \

cmake --build build/entservices-apis --target install



############################
# generating extrnal headers
cd $GITHUB_WORKSPACE
cd entservices-testframework/Tests
echo " Empty mocks creation to avoid compilation errors"
echo "======================================================================================"
mkdir -p headers
mkdir -p headers/audiocapturemgr
mkdir -p headers/rdk/iarmbus
mkdir -p headers/rdk/iarmmgrs-hal
mkdir -p headers/rdk/halif/
mkdir -p headers/rdk/halif/deepsleep-manager
mkdir -p headers/ccec/drivers
mkdir -p headers/network
mkdir -p headers/proc
mkdir -p headers/libusb
mkdir -p headers/Dobby
mkdir -p headers/Dobby/Public/Dobby
mkdir -p headers/Dobby/IpcService
echo "dir created successfully"
echo "======================================================================================"

echo "======================================================================================"
echo "empty headers creation"
cd headers
echo "current working dir: "${PWD}
touch audiocapturemgr/audiocapturemgr_iarm.h
touch ccec/drivers/CecIARMBusMgr.h
touch rdk/iarmbus/libIARM.h
touch rdk/iarmbus/libIBus.h
touch rdk/iarmbus/libIBusDaemon.h
touch rdk/halif/deepsleep-manager/deepSleepMgr.h
touch rdk/iarmmgrs-hal/mfrMgr.h
touch rdk/iarmmgrs-hal/pwrMgr.h
touch rdk/iarmmgrs-hal/sysMgr.h
touch network/wifiSrvMgrIarmIf.h
touch network/netsrvmgrIarm.h
touch libudev.h
touch libusb/libusb.h
touch rfcapi.h
touch rbus.h
touch telemetry_busmessage_sender.h
touch maintenanceMGR.h
touch pkg.h
touch edid-parser.hpp
touch secure_wrapper.h
touch wpa_ctrl.h
touch proc/readproc.h
touch btmgr.h
touch rdk_logger_milestone.h
touch audioOutputPortType.hpp
touch audioOutputPortConfig.hpp
touch tr181api.h
touch Dobby/DobbyProtocol.h
touch Dobby/DobbyProxy.h
touch Dobby/Public/Dobby/IDobbyProxy.h
touch Dobby/IpcService/IpcFactory.h
echo "files created successfully"
echo "======================================================================================"

cd ../../
cp -r /usr/include/gstreamer-1.0/gst /usr/include/glib-2.0/* /usr/lib/x86_64-linux-gnu/glib-2.0/include/* /usr/local/include/trower-base64/base64.h .

if [ ! -e "$GITHUB_WORKSPACE/../entservices-infra" ]; then
    ln -s "$GITHUB_WORKSPACE" "$GITHUB_WORKSPACE/../entservices-infra" || true
fi
if [ ! -e "$GITHUB_WORKSPACE/../entservices-testframework" ]; then
    ln -s "$GITHUB_WORKSPACE/entservices-testframework" "$GITHUB_WORKSPACE/../entservices-testframework" || true
fi
if [ ! -e "$GITHUB_WORKSPACE/../Thunder" ]; then
    ln -s "$GITHUB_WORKSPACE/Thunder" "$GITHUB_WORKSPACE/../Thunder" || true
fi

ls -la ${GITHUB_WORKSPACE}

#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
############################
# Build entservices-infra
echo "buliding entservices-infra"

cd ${GITHUB_WORKSPACE}
cmake -G Ninja -S "$GITHUB_WORKSPACE" -B build/entservices-infra \
-DUSE_THUNDER_R4=ON \
-DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
-DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
-DCMAKE_VERBOSE_MAKEFILE=ON \
-DCMAKE_DISABLE_FIND_PACKAGE_IARMBus=ON \
-DCMAKE_DISABLE_FIND_PACKAGE_RFC=ON \
-DCMAKE_DISABLE_FIND_PACKAGE_DS=ON \
-DCOMCAST_CONFIG=OFF \
-DRDK_SERVICES_COVERITY=ON \
-DRDK_SERVICES_L1_TEST=ON \
-DDS_FOUND=ON \
-DPLUGIN_USBDEVICE=ON \
-DPLUGIN_USB_MASS_STORAGE=ON \
-DPLUGIN_OCICONTAINER=ON \
-DPLUGIN_TELEMETRY=ON \
-DCMAKE_CXX_FLAGS="-DEXCEPTIONS_ENABLE=ON \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/audiocapturemgr \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/rdk/ds \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/rdk/iarmbus \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/rdk/iarmmgrs-hal \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/ccec/drivers \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/network \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/libusb \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/Dobby \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/Dobby/Public/Dobby \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/Dobby/IpcService \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/thunder \
-I /usr/include/libdrm \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/devicesettings.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/Iarm.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/Rfc.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/RBus.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/Telemetry.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/Udev.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/pkg.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/maintenanceMGR.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/secure_wrappermock.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/libusb/libusb.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/Dobby.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/Dobby/DobbyProtocol.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/Dobby/DobbyProxy.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/Dobby/Public/Dobby/IDobbyProxy.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/Dobby/IpcService/IpcFactory.h \
-Wall -Werror -Wno-error=format \
-Wl,-wrap,system -Wl,-wrap,popen -Wl,-wrap,syslog \
-DENABLE_TELEMETRY_LOGGING -DUSE_IARMBUS \
-DENABLE_SYSTEM_GET_STORE_DEMO_LINK -DENABLE_DEEP_SLEEP \
-DENABLE_SET_WAKEUP_SRC_CONFIG -DENABLE_THERMAL_PROTECTION \
-DUSE_DRM_SCREENCAPTURE -DHAS_API_SYSTEM -DHAS_API_POWERSTATE \
-DHAS_RBUS -DDISABLE_SECURITY_TOKEN -DENABLE_DEVICE_MANUFACTURER_INFO -DUSE_THUNDER_R4=ON -DTHUNDER_VERSION=4 -DTHUNDER_VERSION_MAJOR=4 -DTHUNDER_VERSION_MINOR=4" \


cmake --build build/entservices-infra --target install
echo "======================================================================================"
exit 0

mkdir -p build
cd build
rm -rf device_cmake
rm -rf device_bin
mkdir -p device_cmake
cmake -Bdevice_cmake -DPLAYDATE_BUILD_FOR_DEVICE=ON -DCMAKE_TOOLCHAIN_FILE=${PLAYDATE_SDK_PATH}/C_API/buildsupport/arm.cmake ..
cmake --build device_cmake
mkdir -p device_bin
mv ../metal_crank.pdx device_bin/metal_crank.pdx

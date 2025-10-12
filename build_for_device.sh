mkdir -p build
cd build
mkdir -p device
cd device
rm -rf *
cmake -DPLAYDATE_BUILD_FOR_DEVICE=ON -DCMAKE_TOOLCHAIN_FILE=${PLAYDATE_SDK_PATH}/C_API/buildsupport/arm.cmake ../..
cmake --build .
mkdir -p bin
mv ../../metal_crank.pdx bin/metal_crank.pdx

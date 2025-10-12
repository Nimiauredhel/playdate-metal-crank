mkdir -p build
cd build
rm -rf simulator_cmake
rm -rf simulator_bin
mkdir -p simulator_cmake
mkdir -p simulator_bin
cmake -Bsimulator_cmake ..
cmake --build simulator_cmake
mv ../metal_crank.pdx simulator_bin/metal_crank.pdx

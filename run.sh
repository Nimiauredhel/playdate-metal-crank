mkdir -p build
cd build
mkdir -p bin
cmake ..
cmake --build .
mv ../metal_crank.pdx bin/metal_crank.pdx
PlaydateSimulator bin/metal_crank.pdx

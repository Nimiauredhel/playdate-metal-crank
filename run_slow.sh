rm -rf build
mkdir build
cd build
cmake ..
cmake --build .
mkdir -p bin
mv ../metal_crank.pdx bin/metal_crank.pdx
PlaydateSimulator bin/metal_crank.pdx &
cpulimit -p $! -l 30

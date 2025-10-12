mkdir -p build
cd build
mkdir -p simulator
cd simulator
rm -rf *
cmake ../..
cmake --build .
mkdir -p bin
mv ../../metal_crank.pdx bin/metal_crank.pdx
PlaydateSimulator bin/metal_crank.pdx &
cpulimit -p $! -l 30

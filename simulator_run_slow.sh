bash ./simulator_build.sh
PlaydateSimulator build/simulator_bin/metal_crank.pdx &
cpulimit -p $! -l 30

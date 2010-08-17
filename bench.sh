#!/bin/bash

echo "Benchmark" > log.bench
for NPROC in 4; do 

DIRS[0]="papps/splash2/codes/kernels/fft"
EXES[0]="./FFT -m22 -p$NPROC"
DIRS[1]="papps/splash2/codes/kernels/lu/contiguous_blocks"
EXES[1]="./LU -n1280 -p$NPROC"
DIRS[2]="papps/splash2/codes/kernels/radix"
EXES[2]="./RADIX -n5000000 -p$NPROC"
DIRS[3]="papps/splash2/codes/apps/barnes"
EXES[3]="./BARNES < input.p$NPROC"
DIRS[4]="papps/splash2/codes/apps/radiosity"
EXES[4]="./RADIOSITY -room -batch -p $NPROC" 
DIRS[5]="papps/splash2/codes/apps/raytrace"
EXES[5]="./RAYTRACE -m128 -p$NPROC inputs/car.env"
DIRS[6]="papps/splash2/codes/apps/ocean/contiguous_partitions"
EXES[6]="./OCEAN -n514 -p$NPROC" 
DIRS[7]="papps/splash2/codes/apps/water-nsquared"
EXES[7]="./WATER-NSQUARED < input.p$NPROC"

echo 
echo "---------------------"
echo "$NPROC CORES" 
echo "---------------------"
for i in 0 1 2 4 5 6 7; do 
    echo "$i ${DIRS[i]} ${EXES[i]}"
    echo "Clean"
    (cd "${DIRS[i]}"; make clean ) >& /dev/null 
    echo "Compile"
    (cd "${DIRS[i]}"; make ) >& log.build
    echo "Run"
    (cd "${DIRS[i]}"; time nice sh -c "${EXES[i]}") 2>> log.bench 
    grep real log.bench | awk '{ print $2 }' | sed "s/0m//g" | sed "s/s//"
done # i 
done # NPROC

#!/bin/bash
# EMAIL="heechul.yun@gmail.com"
NPROC=`cat src/config.h | grep CPU | awk '{ print $3 }'`
NPROC=2

# this prevent counting lazy binding of ld.so 
LD_BIND_NOW=on  

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

fail()
{
	echo "FAILED: $*"
	cat log.check | \
		mail -s "FAIL:dpthread-check-`date`" $EMAIL
	exit  # real problem 
}

echo "Verification" > log.check
for i in 0 1 2 5; do 
    echo ${DIRS[i]} ${EXES[i]} >> log.check 
    (cd "${DIRS[i]}"; make clean ) 
    (cd "${DIRS[i]}"; make ) >& log.build
    (./verify.sh ${DIRS[i]} "${EXES[i]}" $NPROC ) 2>> log.check || fail "${DIRS[i]}"
done 

if [ ! -z "$EMAIL" ]; then 
	echo "success" | mail -s "PASS:dpthread-check-`date`" $EMAIL
else
	echo success 
fi 

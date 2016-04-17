NCPU=`cat /proc/cpuinfo | grep processor | awk '{a++} END {print a}'`
echo "#define MAX_CPU $NCPU" > config.h 

grep "Intel(R) Core(TM) i7" /proc/cpuinfo && \
    echo "#define USE_INTEL_NEHALEM 1" >> config.h 
grep "W3530" /proc/cpuinfo && \
    echo "#define USE_INTEL_NEHALEM 1" >> config.h 
grep "Intel(R) Core(TM)2" /proc/cpuinfo && \
    echo "#define USE_INTEL_CORE2 1" >> config.h
exit 0 

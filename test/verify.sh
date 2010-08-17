#!/bin/bash 
CMD="$*"
DPTHREAD_DIR=~/Papers/cs523/dpthread
# NPROC=`cat $DPTHREAD_DIR/src/config.h | grep CPU | awk '{ print $3 }'`
NPROC=3
#MATCH_MODE="INST_COUNT" # must match exact instruction count 
MATCH_MODE="SYNC_ORDER" # only sync order should be matched 

#CMD='./BARNES < input.p$NPROC'
export DPTHREAD_DEBUG=1
ITER=100
i=0

error()
{
    echo $*
    exit 1
}

check_vm_randomization()
{
    va_rand=`cat /proc/sys/kernel/randomize_va_space`
    if [ ! "$va_rand" = "0" ]; then 
	echo "/proc/sys/kernel/randomize_va_space(val=$va_rand) shoud be turned off" 
	sudo sysctl -w kernel.randomize_va_space=0
    fi 
    echo "OK - randomization turned off." 
}

# need to check vm 
check_vm_randomization

# logging 
while true; do 
    DPTHREAD_LOG_FILE="log$i" $CMD || error "Failed to exec" 

    # process the log files 
    j=0
    while true; do 
	if [ "$MATCH_MODE" = "INST_COUNT" ]; then 
	    grep -v "Thread" log$i.p$j | grep -v "STAT" | sed "s/\[RT:[0-9]*\]//g" | sed "s/\]([0-9]*,[-]*[0-9]*)/\]/g"> log$i.p$j.sync
	elif [ "$MATCH_MODE" = "SYNC_ORDER" ]; then 
	    grep -v "Thread" log$i.p$j | grep -v "STAT" | sed "s/\[RT:[0-9]*\]//g" | sed "s/\]([0-9]*,[-]*[0-9]*)/\]/g" | sed "s/\[LT:[-]*[0-9]*\]//g" > log$i.p$j.sync
	fi 
	j=`expr $j + 1`
	[ "$j" = "$NPROC" ] && break
    done 
    echo "ITERATION $i" 
    # verify with previous 
    if [ ! -z "$previ" ]; then 
	j=0
	while true; do 
	    echo ">> compare log$previ.p$j vs log$i.p$j" 
	    diff log$previ.p$j.sync log$i.p$j.sync > /dev/null || error "FAIL at $i th iteration. at proc $j" 
	    j=`expr $j + 1`
	    [ "$j" = "$NPROC" ] && break
	done 
    fi 
    echo " -> OK" 
    echo 

    previ=$i 
    i=`expr $i + 1`
    [ "$i" = "$ITER" ] && break
done 
# cleanup 
rm log*.sync 

echo "PASS" 

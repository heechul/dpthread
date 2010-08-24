#!/bin/bash 
FILE="llvm.tar.bz2"
EMAIL="heechul.yun@gmail.com"

LD_BIND_NOW=on

# FILE="1mega"
rm $FILE
rm log*.p*
NPROC=2
CMD="./aget -n $NPROC http://heechul.cs.uiuc.edu/$FILE"
echo $CMD
NPROC=`expr $NPROC + 2` # 2 more threads are running

MATCH_MODE="INST_COUNT" # must match exact instruction count 
# MATCH_MODE="SYNC_ORDER" # only sync order should be matched 

#CMD='./BARNES < input.p$NPROC'
export DPTHREAD_DEBUG=1
ITER=10
i=0

error()
{
    echo $* | mail -s "FAIL:aget-verify-`date`" $EMAIL
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
    cat /proc/interrupts > log$i.interrupts_begin
    DPTHREAD_LOG_FILE="log$i" $CMD || error "Failed to exec" 
    cat /proc/interrupts > log$i.interrupts_end

    # process the log files 
    j=0
    rm $FILE
    while true; do 
	if [ "$MATCH_MODE" = "INST_COUNT" ]; then 
	    grep -v "Thread" log$i.p$j | grep -v "STAT" | grep -v "EXIT" | sed "s/\[RT:[0-9]*\]//g" | sed "s/\]([0-9]*,[-]*[0-9]*)/\]/g"> log$i.p$j.sync
	elif [ "$MATCH_MODE" = "SYNC_ORDER" ]; then 
	    grep -v "Thread" log$i.p$j | grep -v "STAT" | grep -v "EXIT"|  sed "s/\[RT:[0-9]*\]//g" | sed "s/\]([0-9]*,[-]*[0-9]*)/\]/g" | sed "s/\[LT:[-]*[0-9]*\]//g" > log$i.p$j.sync
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
echo "success" | mail -s "PASS:aget-check-`date`" $EMAIL


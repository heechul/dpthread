#!/bin/sh 
CMD="$*"
#CMD="./locktest -n 2 -i 100000" 
NPROC=2
export KENDO_DEBUG=1 
ITER=1000
i=0

error()
{
    echo $*
    exit 1
}

# logging 
while true; do 
    KENDO_LOG_FILE="log$i" $CMD || error "Failed to exec" 

    # process the log files 
    j=0
    while true; do 
	grep -v "Thread" log$i.p$j > log$i.p$j.sync
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

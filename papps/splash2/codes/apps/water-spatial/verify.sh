#!/bin/sh 
CMD="$*"
#CMD="./locktest -n 2 -i 100000" 
NPROC=2
export KENDO_DEBUG=1 
ITER=100
i=0

error()
{
    echo $*
    exit 1
}

# logging 
while true; do 
    KENDO_LOG_FILE="log$i" $CMD

    # process the log files 
    j=0
    while true; do 
	j=`expr $j + 1`
	grep -v "Thread" log$i.p$j > log$i.p$j.sync
	[ "$j" = "$NPROC" ] && break
    done 

    # verify with previous 
    if [ ! -z "$previ" ]; then 
	j=0
	while true; do 
	    j=`expr $j + 1`
		echo ">> compare log$previ.p$j vs log$i.p$j" 
	    diff log$previ.p$j.sync log$i.p$j.sync || error "FAIL" 
	    [ "$j" = "$NPROC" ] && break
	done 
    fi 

    previ=$i 
    i=`expr $i + 1`
    [ "$i" = "$ITER" ] && break
done 
# cleanup 
rm log*.sync 

echo "PASS" 

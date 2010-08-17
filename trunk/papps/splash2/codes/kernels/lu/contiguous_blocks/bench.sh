NCPU=`cat /proc/cpuinfo | grep processor | awk '{a++} END {print a}'`
CMD="./LU -n1280"
# CMD="./LU -n4096"
i=1
while true; do  
	echo $i

 	time sh -c "$CMD -p$i 2> /tmp/out"

	if [ "$i" -eq "$NCPU" ]; then 
		echo "done"
		break
	fi 
	i=$(($i * 2))
done 

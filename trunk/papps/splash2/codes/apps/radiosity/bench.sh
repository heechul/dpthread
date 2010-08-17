NCPU=`cat /proc/cpuinfo | grep processor | awk '{a++} END {print a}'`
CMD="./RADIOSITY -room -batch"
i=1
while true; do  
	echo $i

 	time sh -c "$CMD -p $i $ENV 2> /tmp/out"

	if [ "$i" -eq "$NCPU" ]; then 
		echo "done"
		break
	fi 
	i=$(($i * 2))
done 

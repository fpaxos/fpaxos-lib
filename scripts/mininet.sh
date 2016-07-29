
# clean up
rm replica*.log

# deploy
for ((i=1; i<$1+1; i++))
do
	h(expr $1+1) ./sample/replica $i ../paxos.conf >> replica_$i.log &
done

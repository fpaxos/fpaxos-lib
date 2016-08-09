
# deploy
for ((i=0; i<$1; i++))
do
	./sample/replica $i ../paxos.conf >> \dev\null &
done

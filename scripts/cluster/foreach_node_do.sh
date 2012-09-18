command="ps aux | grep marco|grep -v grep"

for (( i = 1; i < 10; i++ )); do 
    echo "Trying node0$i"
    ssh -t -o "ConnectTimeout=5" "node0$i" "$command";
done

for (( i = 10; i < 16; i++ )); do 
    echo "Trying node$i"
    ssh -t -o "ConnectTimeout=5" "node$i" "$command"; 
done

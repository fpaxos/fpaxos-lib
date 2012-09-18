# Default logging dir
LOGDIR="$HOME/paxos_logs"
BASEDIR="$HOME/libpaxos/tests"

function set_log_dir () {
	LOGDIR=$1
	rm -rf $LOGDIR	
	mkdir -p $LOGDIR
}

function set_basedir () {
	BASEDIR=$1
}

function init_check () {
	local DIR=`pwd`
	if [[ `basename $DIR` != "cluster" ]]; then
		echo "Please run from scripts/cluster/"
		exit;
	fi

	if [[ `hostname -s` != "node01" ]]; then
		echo "Please run from node01"
		exit;
	fi	

}

# function launch_detach_acceptor () {
#   local a_id=$1
#   local host=$2
#   local logfile="$LOGDIR/acceptor_$a_id"
# 
#   echo "Starting acceptor $a_id on host $host"
#   echo "(logs to: $logfile)"
#   ssh -t $host "$BASEDIR/acceptor_main -i $a_id &> $logfile &" &  
# }

function launch_background_acceptor () {
	local a_id=$1
	local host=$2
	local logfile="$LOGDIR/acceptor_$a_id"

	echo "Starting acceptor $a_id on host $host"
	echo "(logs to: $logfile)"
	ssh $host "$BASEDIR/example_acceptor $a_id &> $logfile" &	
}

function launch_tp_monitor () {
	local host=$1
	local logfile="$LOGDIR/monitor.txt"

	echo "Starting monitor on host $host"
	echo "(logs to: $logfile)"
	ssh $host "$BASEDIR/tp_monitor &> $logfile" &	    
}

function show_tp_log () {
    echo "*** Tail of tp_monitor log"
    echo "*** From: $LOGDIR/monitor.txt"
    tail -n 20 $LOGDIR/monitor.txt
    echo
}

function launch_background_oracle () {
	local host=$1
	local logfile="$LOGDIR/oracle.txt"

	echo "Starting oracle on host $host"
	echo "(logs to: $logfile)"
	ssh $host "$BASEDIR/example_oracle &> $logfile" &	
}

function launch_background_proposer () {
	local p_id=$1
	local host=$2
	local logfile="$LOGDIR/proposer_$p_id"

	echo "Starting proposer $p_id on host $host"
	echo "(logs to: $logfile)"
	ssh $host "$BASEDIR/example_proposer $p_id &> $logfile" &
}

function show_proposer_log () {
    echo "*** Tail of proposer $1 log"
    echo "*** From: $LOGDIR/proposer_$1"
    tail -n 20 $LOGDIR/proposer_$1
    echo
}


CLIENT_COUNT=0;
function launch_background_client () {
	local bench_args=$1
    # local logfile=$2
	local host=$2
	let 'CLIENT_COUNT += 1'
	local logfile="$LOGDIR/client_$CLIENT_COUNT"

	echo "Starting client $CLIENT_COUNT on host $host"
	echo "(logs to: $logfile)"
	ssh $host "$BASEDIR/benchmark_client $bench_args &> $logfile" &
}

function lauch_background_abmagic () {
	local host=$1
	local logfile="$LOGDIR/abmagic.txt"

	echo "Starting abmagic on host $host"
	echo "(logs to: $logfile)"
	ssh $host "$BASEDIR/abmagic &> $logfile" &
}

function launch_follow () {
	local cmd=$1
	local host=$2
	
	echo "Executing: \"$cmd\" on host $host"
	echo "from $BASEDIR"
	ssh -t $host "cd $BASEDIR && $cmd"
}

# function launch_background () {
#   local cmd=$1
#   local host=$2
#   
#   echo "Executing: \"$cmd\" on host $host"
#   echo "from $BASEDIR"
#   ssh $host "cd $BASEDIR && $cmd" &
# }

function remote_kill () {
	local prog=$1
	local host=$2
		
	echo "Killing $prog on host $host"
	ssh $host "killall -INT $prog"
}

function remote_kill_all () {
    local procnames="$1"
    local first=2;
    local last=16;

    for (( i = $first; i <= $last; i++ )); do
        nodenum="$i"
        if [[ $i -lt 10 ]]; then
            nodenum="0$i"
        fi
        echo "Killing on marco@node$nodenum"
        ssh -o "ConnectTimeout=10" "marco@node$nodenum" "killall -INT $procnames"
    done
}
#!/usr/bin/env bash

VG=""
BUILD="build"
CONFIG="paxos.conf"
OPT="--verbose"

tmux_test ()  {
	tmux new-session -d -s paxos
	tmux new-window -t paxos

	for (( i = 0; i < 3; i++ )); do
		tmux split
		tmux select-layout even-vertical
	done

	for (( i = 0; i < 3; i++ )); do
		tmux send-keys -t $i "$VG ./$BUILD/sample/replica $i $CONFIG $OPT" C-m
	done

	tmux send-keys -t 3 "./$BUILD/sample/client $CONFIG" C-m
	tmux selectp -t 3

	tmux attach-session -t paxos
	tmux kill-session -t paxos
}

usage () {
	echo "$0 [--help] [--build-dir dir] [--config-file] [--valgrind]
	[--silence-replica]"
	exit 1
}

while [[ $# > 0 ]]; do
	key="$1"
	case $key in
		-b|--build-dir)
		DIR=$2
		shift
		;;
		-c|--config)
		CONFIG=$2
		shift
		;;
		-h|--help)
		usage
		;;
		-s|--silence-replica)
		OPT=""
		;;
		-v|--valgrind)
		VG="valgrind "
		;;
		*)
		usage
		;;
	esac
	shift
done

tmux_test

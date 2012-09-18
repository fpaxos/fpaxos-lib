#!/bin/bash

# Edit this accordingly
PROJ_DIR="/Users/bridge/Desktop/mthesis/trunk/libpaxos2/tests" 


if [[ ! -e $PROJ_DIR ]]; then
    echo "You must edit the PROJ_DIR variable in this script!"
    exit 1;
fi

KEEP_XTERM_OPEN="echo \"press enter to close\"; read";

rm -rf /tmp/acceptor_*;
echo "Starting the acceptors"
xterm -geometry 80x24+10+10 -e "cd $PROJ_DIR; ./example_acceptor 0; $KEEP_XTERM_OPEN" &
xterm -geometry 80x24+400+10 -e "cd $PROJ_DIR; ./example_acceptor 1; $KEEP_XTERM_OPEN" &
xterm -geometry 80x24+800+10 -e "cd $PROJ_DIR; ./example_acceptor 2; $KEEP_XTERM_OPEN" &

xterm -geometry 80x24+800+400 -e "cd $PROJ_DIR; ./example_oracle; $KEEP_XTERM_OPEN" &

sleep 3;

xterm -geometry 80x24+10+300 -e "cd $PROJ_DIR; ./example_proposer 0; $KEEP_XTERM_OPEN" &
xterm -geometry 80x24+200+300 -e "cd $PROJ_DIR; ./example_proposer 1; $KEEP_XTERM_OPEN" &
xterm -geometry 80x24+400+300 -e "cd $PROJ_DIR; ./example_proposer 2; $KEEP_XTERM_OPEN" &
sleep 3;

xterm -geometry 80x8+10+600 -e "cd $PROJ_DIR; ./benchmark_client; $KEEP_XTERM_OPEN" &

echo "Press enter to send the kill signal"
read
killall -INT example_acceptor example_proposer example_learner benchmark_client example_oracle
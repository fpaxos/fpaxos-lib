#!/bin/bash

# Edit this accordingly
PROJ_DIR="/Users/bridge/Desktop/mthesis/trunk/libpaxos2/tests" 


if [[ ! -e $PROJ_DIR ]]; then
    echo "You must edit the PROJ_DIR variable in this script!"
    exit 1;
fi

KEEP_XTERM_OPEN="echo \"press enter to close\"; read";

rm -rf /tmp/acceptor_*;
echo "Starting ABMagic"
xterm -geometry 80x24+10+10 -e "cd $PROJ_DIR; ./abmagic; $KEEP_XTERM_OPEN" &

xterm -geometry 80x24+600+300 -e "cd $PROJ_DIR; ./tp_monitor; $KEEP_XTERM_OPEN" &
sleep 5;

xterm -geometry 80x8+10+600 -e "cd $PROJ_DIR; ./benchmark_client; $KEEP_XTERM_OPEN" &

echo "Press enter to send the kill signal"
read
killall -INT abmagic tp_monitor benchmark_client

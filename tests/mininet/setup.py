#!/usr/bin/python

from __future__ import print_function

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.node import CPULimitedHost
from mininet.link import TCLink
from mininet.util import dumpNodeConnections
from mininet.log import setLogLevel

from sys import argv
from time import sleep

class SingleSwitchTopo(Topo):
    "Single switch connected to n hosts."
    def __init__(self, n=2, lossy=False, **opts):
        Topo.__init__(self, **opts)
        switch = self.addSwitch('s1')
        for h in range(n):
            # Each host gets 50%/n of system CPU
            host = self.addHost('h%s' % (h + 1),
                                cpu=.9 / n)
            # 10 Mbps, 5ms delay, no packet loss
            self.addLink(host, switch, bw=10, delay='10ms', loss=0, use_htb=True)

def writeConfigFile(r,q1,q2,g1,g2):
    filename = "config_r"+str(r)+"_q"+str(q1)
    f = open(filename, 'w')
    # add replicas
    for id in range(0,r):
        f.write('replica '+str(id)+' 10.0.0.'+str(id+1)+' 880'+str(id)+'\n')
    # add rest of config
    f.write('verbosity debug\n')
    f.write('quorum-1 '+str(q1)+'\n')
    f.write('quorum-2 '+str(q2)+'\n')
    f.write('group-1 '+str(g1)+'\n')
    f.write('group-2 '+str(g2)+'\n')
    f.write('lmdb-env-path /tmp/acceptor\n')
    return filename

def libFPaxosTest(r,q1,q2,thrifty):
    "Create network and run simple performance test"
    topo = SingleSwitchTopo( n=r+1 )
    net = Mininet( topo=topo,
                   host=CPULimitedHost, link=TCLink,
                   autoStaticArp=True )
    net.start()
    print( "Dumping host connections" )
    dumpNodeConnections(net.hosts)
    print( "Running libFPaxos" )
    path = '../../build/sample/'
    if thrifty:
        config_file = writeConfigFile(r,q1,q2,r,q2)
    else:
        config_file = writeConfigFile(r,q1,q2,r,r)
    # start replicas
    for id in range(1,r+1):
        host = net.get('h'+ str(id))
        print( "Starting libFPaxos on replica "+str(id))
        stdout = host.cmd(path + 'replica '+str(id-1)+' '+config_file+' &>> '+str(id-1)+'.log &')
        print(stdout)
    # start client
    c = net.get('h'+str(r+1))
    print( "Starting libFPaxos client")
    stdout = c.cmd(path + 'client '+config_file+' -p 0 -o 10 &>> client.log &')
    print(stdout)
    pid = int( c.cmd('echo $!') )
    sleep(120)
    print("killing process "+str(pid))
    stdout = c.cmd('kill '+str(pid))
    sleep(1)
    print(stdout)
    net.stop()

def libPaxos(r):
    libFPaxosTest(r,(r/2)+1,(r/2)+1,False)

def libFPaxos(r,q1):
    libFPaxosTest(r,q1,(r-q1)+1,True)

if __name__ == '__main__':
    setLogLevel( 'info' )
    libPaxos(5)

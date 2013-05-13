# LibPaxos

This is a fork of [libpaxos](http://libpaxos.sourceforge.net/), an implementation of the Paxos protocol. This version of libpaxos is different from the original one in that it implements a network agnostic paxos (see ```libpaxos/lib/```). That is, libpaxos does not depend on any particular networking library and can be easily adapted to work with any networking library. 

As an example, we include a networked implementation of paxos (see ```libpaxos/evpaxos/```) that implements paxos on top libpaxos and [libevent](http://www.libevent.org).
 
This code has been extensively tested and benchmarked, it comes as-it-is, it may contain bugs and should not be used where true reliability/fault tolerance is required.

## Building

Before compiling libpaxos, you need to have compiled both [Berkeley DB](http://www.oracle.com/technetwork/products/berkeleydb/overview/index.html) and [libevent](http://www.libevent.org).
	
	git clone git@bitbucket.org:sciascid/libpaxos.git
	mkdir libpaxos/build
	cd libpaxos/build
	cmake -DBDB_ROOT=/path/to/libdb -DLIBEVENT_ROOT=/path/to/libevent ..
	make

## Running the examples

	cd libpaxos/build
	./sample/acceptor 0 ../config.cfg > /dev/null &
	./sample/acceptor 1 ../config.cfg > /dev/null &
	./sample/proposer 0 ../config.cfg > /dev/null &
	./sample/learner ../config.cfg > learner.txt &
	./sample/client ../config.cfg 1
		
##  Unit tests

First enable unit tests

	cmake -DBUILD_UNIT=ON ..
	make

And then execute the tests using
	
	make test

Or, if you want a more detailed output run

	./unit/gtest_main
	
## License

Libpaxos is distributed under GPL version 3.

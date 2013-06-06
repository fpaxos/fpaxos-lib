# LibPaxos

This is a fork of [libpaxos](http://libpaxos.sourceforge.net/), an implementation of the Paxos protocol. This version of libpaxos is different from the original one in that it implements a network agnostic paxos (see ```libpaxos/lib/```). That is, libpaxos does not depend on any particular networking library and can be easily adapted to work with any networking library. 

As an example, we include a networked implementation of paxos (see ```libpaxos/evpaxos/```) that implements paxos on top of libpaxos and [libevent](http://www.libevent.org).

## Building

These are the basic steps required to compile libpaxos

	git clone git@bitbucket.org:sciascid/libpaxos.git
	mkdir libpaxos/build
	cd libpaxos/build
	cmake ..
	make

Libpaxos depends on Berkeley [Berkeley DB](http://www.oracle.com/technetwork/products/berkeleydb/overview/index.html) and [libevent](http://www.libevent.org). Berkeley DB can be disabled in favor of an in-memory storage (see build options below).

### Useful build options

You pass options to cmake as follows: ```cmake -DOPTION=VALUE```

- ```BDB_ROOT=PATH```  - point it to your installation of Berkeley DB
- ```LIBEVENT_ROOT=PATH``` -  point it to your installation of Libevent
- ```USE_MEM_STORE=ON``` - enables in-memory storage, instead of Berkeley DB
- ```BUILD_UNIT=ON``` -  enables unit tests

## Running the examples

	cd libpaxos/build
	./sample/acceptor 0 ../config.cfg > /dev/null &
	./sample/acceptor 1 ../config.cfg > /dev/null &
	./sample/proposer 0 ../config.cfg > /dev/null &
	./sample/learner ../config.cfg > learner.txt &
	./sample/client 127.0.0.1:5550 1
	
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

# LibPaxos

This is LibPaxos3! A complete rewrite of [LibPaxos2](http://libpaxos.sourceforge.net/). LibPaxos3 has been improved in the following aspects:

- Doesn't make use of multicast
- Has a cleaner design
- Has a better build system based on CMake
- Comes with unit tests

LibPaxos3 is divided in two libraries: libpaxos and libevpaxos. 

Libpaxos (see ```libpaxos/lib```) implements the core of the Paxos consensus protocol, and is not cluttered with network specific code. That is, libpaxos does not depend on any particular networking library.

Libevpaxos (see ```libpaxos/evpaxos```) is the actual Paxos implementation on top of libpaxos and [libevent](http://www.libevent.org).

## Building

These are the basic steps required to get and compile libpaxos

	git clone git@bitbucket.org:sciascid/libpaxos.git
	mkdir libpaxos/build
	cd libpaxos/build
	cmake ..
	make

LibPaxos3 depends on Berkeley [Berkeley DB](http://www.oracle.com/technetwork/products/berkeleydb/overview/index.html) and [libevent](http://www.libevent.org). Berkeley DB can be disabled in favor of an in-memory storage (see build options below).

LibPaxos3 has been tested on Linux and OS X.

### Useful build options

You pass options to cmake as follows: ```cmake -DOPTION=VALUE```

- ```BDB_ROOT=PATH```  - point it to your installation of Berkeley DB
- ```LIBEVENT_ROOT=PATH``` -  point it to your installation of Libevent
- ```USE_MEM_STORE=ON``` - enables in-memory storage, instead of Berkeley DB
- ```BUILD_UNIT=ON``` -  enables unit tests - see section below

## Running the examples

	cd libpaxos/build
	./sample/acceptor 0 ../paxos.conf > /dev/null &
	./sample/acceptor 1 ../paxos.conf > /dev/null &
	./sample/proposer 0 ../paxos.conf > /dev/null &
	./sample/learner ../paxos.conf > learner.txt &
	./sample/client 127.0.0.1:5550 1

## Configuration

See ```paxos.conf``` for a sample configuration file.

##  Unit tests

Unit tests depend on the [Google Test](http://code.google.com/p/googletest/) library. First enable unit tests run

	cmake -DBUILD_UNIT=ON ..
	make

Execute the tests using ```make test```, or, run ```./unit/gtest``` for detailed output.

## Feedback

[LibPaxos project page](http://libpaxos.sourceforge.net)

[LibPaxos3 repository](https://bitbucket.org/sciascid/libpaxos)

[Mailing list](https://lists.sourceforge.net/lists/listinfo/libpaxos-general)

## License

Libpaxos is distributed under GPL version 3.

LibPaxos3 has been developed at the [University of Lugano](http://inf.usi.ch), by [Daniele Sciascia](http://atelier.inf.usi.ch/~sciascid).

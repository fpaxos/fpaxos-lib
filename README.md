# LibPaxos

This is LibPaxos3! A complete rewrite of [LibPaxos2][1].
LibPaxos3 has been improved in the following aspects:

- Doesn't make use of multicast
- Has a cleaner design
- Has a better build system based on CMake
- Comes with unit tests

LibPaxos3 is divided in two libraries: libpaxos and libevpaxos. 

Libpaxos (see ```libpaxos/paxos```) implements the core of the Paxos consensus 
protocol, and is not cluttered with network specific code. That is, libpaxos 
does not depend on any particular networking library.

Libevpaxos (see ```libpaxos/evpaxos```) is the actual networked Paxos 
implementation. This library is built on top of the libpaxos and [libevent][2].

## Building

These are the basic steps required to get and compile LibPaxos3

	git clone https://bitbucket.org/sciascid/libpaxos.git
	mkdir libpaxos/build
	cd libpaxos/build
	cmake ..
	make

LibPaxos3 depends on [BerkeleyDB][3] and [libevent][2]. Berkeley DB can be 
disabled in favor of an in-memory storage (see build options below).

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

Unit tests depend on the [Google Test][4] library. First enable unit tests run

	cmake -DBUILD_UNIT=ON ..
	make

Execute the tests using ```make test```, or, run ```./unit/gtest``` for 
detailed output.

## Feedback

[LibPaxos project page][1]

[LibPaxos3 repository][5]

[Mailing list][6]

## License

LibPaxos3 is distributed under the terms of the 3-clause BSD license.
LibPaxos3 has been developed at the [University of Lugano][7],
by [Daniele Sciascia][8].

[1]: http://libpaxos.sourceforge.net
[2]: http://www.libevent.org
[3]: http://www.oracle.com/technetwork/products/berkeleydb
[4]: http://code.google.com/p/googletest/
[5]: https://bitbucket.org/sciascid/libpaxos
[6]: https://lists.sourceforge.net/lists/listinfo/libpaxos-general
[7]: http://inf.usi.ch
[8]: http://atelier.inf.usi.ch/~sciascid

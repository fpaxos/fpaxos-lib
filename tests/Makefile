SRCS 		= example_learner.c example_acceptor.c example_proposer.c benchmark_client.c example_oracle.c abmagic.c tp_monitor.c tp_sampler.c

PROGRAMS	= $(subst .c,,$(SRCS))

AUX_FILES = *.txt

LDFLAGS		= ../libpaxos.a $(LEV_DIR)/.libs/libevent.a $(BDB_DIR)/libdb.a -lpthread
ifeq ($(strip $(SNAME)),Linux)
LDFLAGS		= $(LDFLAGS) -lrt
endif

include ../Makefile.conf
include ../Makefile.inc
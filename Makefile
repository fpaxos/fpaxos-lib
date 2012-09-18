include Makefile.conf

LIBPAXOS	= libpaxos.a
MODULES 	= lib tests
AR			= ar
ARFLAGS		= rc
QUIET		= @

all: bdb_check libevent_check $(LIBPAXOS) $(MODULES)
	@echo "Build completed"

.PHONY: $(MODULES) bdb_check libevent_check

$(MODULES):	
	$(QUIET) $(MAKE) QUIET=$(QUIET) --directory=$@ || exit 1;
	@echo "Done in $@/"

$(LIBPAXOS): lib 
	$(QUIET) $(AR) $(ARFLAGS) $@ $(addsuffix /*.o, $^)
	$(QUIET) ranlib $@
	@echo "Libpaxos.a done."

bdb_check:
	$(QUIET) if [ ! -e $(BDB_DIR) ]; then \
	echo "Error: Berkeley DB not found in $(BDB_DIR)"; \
	echo "You must build BDB and update the BDB_DIR variable in Makefile.conf before proceeding"; \
	exit 1; \
	fi

libevent_check:
	$(QUIET) if [ ! -e $(LEV_DIR) ]; then \
	echo "Error: LibEvent not found in $(LEV_DIR)"; \
	echo "You must build LibEvent and update the LEV_DIR variable in Makefile.conf before proceeding"; \
	exit 1; \
	fi

clean:
	$(QUIET) rm -f $(LIBPAXOS)
	$(QUIET) for m in $(MODULES); do \
		$(MAKE) QUIET=$(QUIET) clean --directory=$$m; \
	done
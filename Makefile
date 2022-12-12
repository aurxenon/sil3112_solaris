TOP_LEVEL_DIR = $(shell /bin/pwd)

include Makefile.defs

SUBDIRS = dktp

.PHONY: ${SUBDIRS}

all: ${SUBDIRS}
	${MAKE} -C $^ all

clean: ${SUBDIRS}
	${MAKE} -C $^ clean
	rm -rf ${MODS_DIR}
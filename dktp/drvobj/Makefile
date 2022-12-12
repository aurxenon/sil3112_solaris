TOP_LEVEL_DIR = $(shell /bin/pwd)/../..

include ${TOP_LEVEL_DIR}/Makefile.defs

CFLAGS += -I${TOP_LEVEL_DIR}/include

SOURCES_STRATEGY = strategy.o
MODULE_STRATEGY  = ${MODS_DIR}/sol11strategy.bfdld

all: ${MODS_DIR} ${MODULE_STRATEGY}

clean:
	rm -rf ${SOURCES_STRATEGY} ${MODULE_STRATEGY}

%.o: %.c
	${CC} ${CFLAGS} -c $^ -o $@

${MODULE_STRATEGY}: ${SOURCES_STRATEGY}
	${LD} -r $^ -o $@
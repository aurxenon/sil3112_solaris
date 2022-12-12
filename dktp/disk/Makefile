TOP_LEVEL_DIR = $(shell /bin/pwd)/../..

include ${TOP_LEVEL_DIR}/Makefile.defs

CFLAGS += -I${TOP_LEVEL_DIR}/include

SOURCES_CMDK = cmdk.o
MODULE_CMDK  = ${MODS_DIR}/sol11cmdk.bfdld
CONFIG_CMDK  = sol11cmdk.conf

all: ${MODS_DIR} ${MODULE_CMDK}

clean:
	rm -rf ${SOURCES_CMDK} ${MODULE_CMDK} ${MODS_DIR}/${CONFIG_CMDK}

%.o: %.c
	${CC} ${CFLAGS} -c $^ -o $@

${MODULE_CMDK}: ${SOURCES_CMDK}
	${LD} -r $^ -o $@
	cp ${CONFIG_CMDK} ${MODS_DIR}
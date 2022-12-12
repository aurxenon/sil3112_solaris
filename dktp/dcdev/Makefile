TOP_LEVEL_DIR = $(shell /bin/pwd)/../..

include ${TOP_LEVEL_DIR}/Makefile.defs

CFLAGS += -I${TOP_LEVEL_DIR}/include

SOURCES_DADK = dadk.o
SOURCES_GDA  = gda.o
MODULE_DADK  = ${MODS_DIR}/sol11dadk.bfdld
MODULE_GDA   = ${MODS_DIR}/sol11gda.bfdld

all: ${MODS_DIR} ${MODULE_DADK} ${MODULE_GDA}

clean:
	rm -rf ${SOURCES_DADK} ${SOURCES_GDA} ${MODULE_DADK} ${MODULE_GDA}

%.o: %.c
	${CC} ${CFLAGS} -c $^ -o $@

${MODULE_DADK}: ${SOURCES_DADK}
	${LD} -r $^ -o $@

${MODULE_GDA}: ${SOURCES_GDA}
	${LD} -r $^ -o $@
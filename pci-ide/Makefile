TOP_LEVEL_DIR = $(shell /bin/pwd)/..

include ${TOP_LEVEL_DIR}/Makefile.defs

CFLAGS += -I${TOP_LEVEL_DIR}/include

SOURCES_PCIIDE = pci-ide.o
MODULE_PCIIDE  = ${MODS_DIR}/sol11pci-ide.bfdld
CONFIG_PCIIDE  = sol11pci-ide.conf

all: ${MODS_DIR} ${MODULE_PCIIDE}

clean:
	rm -rf ${SOURCES_PCIIDE} ${MODULE_PCIIDE} ${MODS_DIR}/${CONFIG_PCIIDE}

%.o: %.c
	${CC} ${CFLAGS} -c $^ -o $@

${MODULE_PCIIDE}: ${SOURCES_PCIIDE}
	${LD} -r $^ -o $@
	cp ${CONFIG_PCIIDE} ${MODS_DIR}
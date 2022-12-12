TOP_LEVEL_DIR = $(shell /bin/pwd)/../../..

include ${TOP_LEVEL_DIR}/Makefile.defs

GHD_DIR = ${TOP_LEVEL_DIR}/dktp/hba/ghd

CFLAGS += -I${TOP_LEVEL_DIR}/include -I${GHD_DIR}/.. -DATA_DEBUG -DGHD_DEBUG -DDEBUG

SOURCES_ATA = ata_blacklist.o \
              ata_common.o \
		      ata_debug.o \
		      ata_disk.o \
		      ata_dma.o \
		      atapi_fsm.o \
		      atapi.o \
		      sil3xxx.o \
		      ${GHD_DIR}/ghd_debug.o \
              ${GHD_DIR}/ghd_dma.o \
		      ${GHD_DIR}/ghd_gcmd.o \
		      ${GHD_DIR}/ghd_queue.o \
		      ${GHD_DIR}/ghd_scsa.o \
		      ${GHD_DIR}/ghd_scsi.o \
		      ${GHD_DIR}/ghd_timer.o \
		      ${GHD_DIR}/ghd_waitq.o \
		      ${GHD_DIR}/ghd.o
MODULE_ATA  = ${MODS_DIR}/sol11ata.bfdld
CONFIG_ATA  = sol11ata.conf

all: ${MODS_DIR} ${MODULE_ATA}

clean:
	rm -rf ${SOURCES_ATA} ${MODULE_ATA} ${MODS_DIR}/${CONFIG_ATA}

%.o: %.c
	${CC} ${CFLAGS} -c $^ -o $@

${MODULE_ATA}: ${SOURCES_ATA}
	${LD} -r $^ -o $@
	cp ${CONFIG_ATA} ${MODS_DIR}
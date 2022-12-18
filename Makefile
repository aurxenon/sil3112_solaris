TOP_LEVEL_DIR = $(shell /bin/pwd)

include Makefile.defs

SUBDIRS = dktp \
          pci-ide

.PHONY: ${SUBDIRS}

all: $(SUBDIRS)

clean: $(SUBDIRS)
	rm -rf ${MODS_DIR}

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
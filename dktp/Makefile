SUBDIRS = controller/ata \
		  dcdev \
		  disk \
		  drvobj

.PHONY: ${SUBDIRS}

all: $(SUBDIRS)

clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
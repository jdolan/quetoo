
MINGW_TARGET = mingw32
MINGW_ARCH = i686

DATA = ../../quake2world-data
TARGET = target
QUAKE2WORLD = $(TARGET)/Quake2World
BINARIES = $(QUAKE2WORLD)/bin
LIBRARIES = $(QUAKE2WORLD)/lib
SHARE = $(QUAKE2WORLD)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-win32/$(MINGW_ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
ZIP = $(TARGET)/Quake2World-BETA-$(MINGW_ARCH).zip
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: bundle

pre-install:
	install -d $(TARGET)
	cp -r $(MINGW_ARCH) $(QUAKE2WORLD)
	
install: pre-install
	pushd ..
	make DESTDIR=$(realpath $(QUAKE2WORLD)) install
	popd

install-data: pre-install
	cp -r $(DATA)/target/* $(SHARE)

bundle: install
	./dllbundler.sh -x $(BINARIES)/quake2world.exe -d $(BINARIES)
	
release:	
	rsync -avzP $(QUAKE2WORLD)/*.* $(BINARIES) $(LIBRARIES) $(RSYNC_TARGET)

image:
	zip -9 -r $(ZIP) $(QUAKE2WORLD)

release-image:
	rsync -avzP $(ZIP) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
	
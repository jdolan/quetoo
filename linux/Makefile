# Makefile for Quetoo Linux distributable packages.
# Produces: AppImage, .deb, and .rpm
# Requires: linuxdeploy, fpm, imagemagick

ARCH      = x86_64
BUILD     = $(ARCH)-pc-linux
TARGET    = target
STAGING   = $(TARGET)/staging
APPDIR    = $(TARGET)/Quetoo.AppDir
PKGDIR    = $(TARGET)/pkg
VERSION  := $(shell cd .. && git rev-list --count HEAD 2>/dev/null || echo 0)

QUETOO_S3_BUCKET      = s3://quetoo
QUETOO_DATA_S3_BUCKET = s3://quetoo-data

BINARIES = \
	$(STAGING)/bin/quemap \
	$(STAGING)/bin/quetoo \
	$(STAGING)/bin/quetoo-dedicated \
	$(STAGING)/bin/quetoo-master

MODULES = \
	$(STAGING)/lib/default/cgame.so \
	$(STAGING)/lib/default/game.so

APPIMAGE = $(TARGET)/quetoo-$(BUILD).AppImage
DEB      = $(TARGET)/quetoo_$(VERSION)_amd64.deb
RPM      = $(TARGET)/quetoo-$(VERSION)-1.x86_64.rpm

LINUXDEPLOY     = $(TARGET)/linuxdeploy-$(ARCH).AppImage
APPIMAGETOOL    = $(TARGET)/appimagetool-$(ARCH).AppImage

all: appimage deb rpm

configure:
	cd .. && autoreconf -i && ./configure \
		CFLAGS='-g -O2' \
		LDFLAGS='-Wl,-rpath=XORIGIN/../lib' \
		--prefix=/

compile:
	$(MAKE) -C .. all

$(STAGING): compile
	install -d $(STAGING)
	cp -r quetoo/* $(STAGING)
	find $(STAGING) -name .turd -delete
	$(MAKE) -C .. DESTDIR=$(abspath $(STAGING)) install

$(LINUXDEPLOY):
	curl -fsSL -o $@ \
		https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$(ARCH).AppImage
	chmod +x $@

$(APPIMAGETOOL):
	curl -fsSL -o $@ \
		https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$(ARCH).AppImage
	chmod +x $@

# ---------------------------------------------------------------------------
# AppDir: populate with our layout under usr/, then let linuxdeploy bundle
# all shared library dependencies and rewrite RPATHs.

$(APPDIR): $(STAGING) $(LINUXDEPLOY)
	rm -rf $(APPDIR)
	install -d $(APPDIR)/usr

	cp -a $(STAGING)/bin   $(APPDIR)/usr/
	cp -a $(STAGING)/lib   $(APPDIR)/usr/
	cp -a $(STAGING)/share $(APPDIR)/usr/

	convert ../src/main/quetoo.ico[0] $(APPDIR)/quetoo.png

	ARCH=$(ARCH) $(LINUXDEPLOY) --appimage-extract-and-run \
		--appdir $(APPDIR) \
		$(foreach b,$(notdir $(BINARIES)),--executable $(APPDIR)/usr/bin/$(b)) \
		--desktop-file quetoo/Quetoo.desktop \
		--icon-file $(APPDIR)/quetoo.png

	# Install our custom AppRun (overrides linuxdeploy's default symlink).
	cp AppRun $(APPDIR)/AppRun
	chmod +x $(APPDIR)/AppRun

# ---------------------------------------------------------------------------
# AppImage

appimage: $(APPDIR) $(APPIMAGETOOL)
	ARCH=$(ARCH) $(APPIMAGETOOL) --appimage-extract-and-run \
		$(APPDIR) $(APPIMAGE)

# ---------------------------------------------------------------------------
# .deb / .rpm — reuse the linuxdeploy-bundled AppDir content so we don't
# need a separate library-bundling pass.

$(TARGET)/.pkg-stamp: $(APPDIR)
	rm -rf $(PKGDIR)

	# Install game under private prefix /usr/lib/quetoo so that the engine's
	# runtime path detection (strip at /bin/) resolves to /usr/lib/quetoo/lib.
	install -d $(PKGDIR)/usr/lib/quetoo
	cp -a $(APPDIR)/usr/bin $(PKGDIR)/usr/lib/quetoo/
	cp -a $(APPDIR)/usr/lib $(PKGDIR)/usr/lib/quetoo/

	# Desktop integration
	install -Dm644 quetoo/Quetoo.desktop \
		$(PKGDIR)/usr/share/applications/quetoo.desktop
	install -d $(PKGDIR)/usr/share/icons/hicolor/256x256/apps
	convert ../src/main/quetoo.ico[0] \
		$(PKGDIR)/usr/share/icons/hicolor/256x256/apps/quetoo.png

	# Thin wrapper scripts in /usr/bin that exec the real binaries
	for bin in quetoo quetoo-dedicated quemap quetoo-master; do \
		printf '#!/bin/sh\nexec /usr/lib/quetoo/bin/%s "$$@"\n' $$bin \
			| install -Dm755 /dev/stdin $(PKGDIR)/usr/bin/$$bin; \
	done

	touch $@

# ---------------------------------------------------------------------------
# .deb

deb: $(TARGET)/.pkg-stamp
	fpm -s dir -t deb \
		-n quetoo -v $(VERSION) \
		--architecture amd64 \
		--description "Free, open-source arena first-person shooter" \
		--url "https://quetoo.org" \
		--license "GPL-2.0" \
		--maintainer "jdolan <jdolan@jdolan.net>" \
		--depends "libc6 (>= 2.31)" \
		--depends "libstdc++6" \
		--depends "libgl1 | libgles2" \
		-p $(DEB) \
		-C $(PKGDIR) .

# ---------------------------------------------------------------------------
# .rpm

rpm: $(TARGET)/.pkg-stamp
	fpm -s dir -t rpm \
		-n quetoo -v $(VERSION) \
		--architecture x86_64 \
		--description "Free, open-source arena first-person shooter" \
		--url "https://quetoo.org" \
		--license "GPL-2.0" \
		--maintainer "jdolan <jdolan@jdolan.net>" \
		--depends "glibc >= 2.31" \
		--depends "libstdc++" \
		--depends "mesa-libGL" \
		-p $(RPM) \
		-C $(PKGDIR) .

# ---------------------------------------------------------------------------
# S3 publishing

release: all
	aws s3 sync --delete $(STAGING) $(QUETOO_S3_BUCKET)/$(BUILD)
	git rev-list --count HEAD > $(TARGET)/version
	aws s3 cp $(TARGET)/version $(QUETOO_S3_BUCKET)/versions/$(BUILD)
	aws s3 cp $(APPIMAGE) $(QUETOO_S3_BUCKET)/snapshots/
	aws s3 cp $(DEB)      $(QUETOO_S3_BUCKET)/snapshots/
	aws s3 cp $(RPM)      $(QUETOO_S3_BUCKET)/snapshots/

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(STAGING)/share

clean:
	rm -rf $(TARGET)

.PHONY: all configure compile appimage deb rpm release install-data clean

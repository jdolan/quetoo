# Makefile for Quetoo Linux distributable packages.
# Produces: AppImage, .deb, and .rpm
# Requires: linuxdeploy, fpm, imagemagick

ARCH     := $(shell uname -m)
BUILD    := $(ARCH)-pc-linux
TARGET    = target
STAGING   = $(TARGET)/staging
APPDIR    = $(TARGET)/Quetoo.AppDir
PKGDIR    = $(TARGET)/pkg
VERSION  := $(shell cd .. && git rev-list --count HEAD 2>/dev/null || echo 0)

# Debian uses amd64/arm64; rpm uses the uname names directly.
DEB_ARCH := $(if $(filter x86_64,$(ARCH)),amd64,$(if $(filter aarch64,$(ARCH)),arm64,$(ARCH)))

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

ARCHIVE  = $(TARGET)/quetoo-$(BUILD).tar.gz
DEB      = $(TARGET)/quetoo_$(VERSION)_$(DEB_ARCH).deb
RPM      = $(TARGET)/quetoo-$(VERSION)-1.$(ARCH).rpm

LINUXDEPLOY     = $(TARGET)/linuxdeploy-$(ARCH).AppImage

all: archive deb rpm

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

	# Install the icon at its XDG path so Sys_RegisterDesktopEntry can reference it.
	install -Dm644 $(APPDIR)/quetoo.png \
		$(APPDIR)/usr/share/icons/hicolor/256x256/apps/quetoo.png

# ---------------------------------------------------------------------------
# Archive

archive: $(APPDIR)
	tar czf $(ARCHIVE) -C $(APPDIR) --transform 's|^usr|quetoo|' usr/

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

	# Symlinks in /usr/bin pointing at the real binaries under /usr/lib/quetoo/bin/.
	# The kernel resolves /proc/self/exe to the real path, so the engine correctly
	# derives lib_dir and data_dir from /usr/lib/quetoo regardless of how it was invoked.
	# The RPATH ($ORIGIN/../lib) baked in by linuxdeploy also resolves from the real
	# binary location, so no LD_LIBRARY_PATH wrapper is needed.
	for bin in quetoo quetoo-dedicated quemap quetoo-master quetoo-attach quetoo-update; do \
		install -d $(PKGDIR)/usr/bin; \
		ln -sf /usr/lib/quetoo/bin/$$bin $(PKGDIR)/usr/bin/$$bin; \
	done

	# Server management: systemd units, start wrapper, default config, update script.
	install -Dm644 quetoo/usr/lib/systemd/system/quetoo-dedicated@.service \
		$(PKGDIR)/usr/lib/systemd/system/quetoo-dedicated@.service
	install -Dm644 quetoo/usr/lib/systemd/system/quetoo-master.service \
		$(PKGDIR)/usr/lib/systemd/system/quetoo-master.service
	install -Dm644 quetoo/etc/quetoo-dedicated/default.cfg       $(PKGDIR)/etc/quetoo-dedicated/default.cfg
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-dedicated-start $(PKGDIR)/usr/lib/quetoo/bin/quetoo-dedicated-start
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-update       $(PKGDIR)/usr/lib/quetoo/bin/quetoo-update
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-attach       $(PKGDIR)/usr/lib/quetoo/bin/quetoo-attach
	install -Dm440 quetoo/etc/sudoers.d/quetoo                   $(PKGDIR)/etc/sudoers.d/quetoo

	touch $@

# ---------------------------------------------------------------------------
# .deb

deb: $(TARGET)/.pkg-stamp
	fpm -s dir -t deb \
		-n quetoo -v $(VERSION) \
		--architecture $(DEB_ARCH) \
		--description "Free, open-source arena first-person shooter" \
		--url "https://quetoo.org" \
		--license "GPL-2.0" \
		--maintainer "jdolan <jdolan@jdolan.net>" \
		--depends "libc6 (>= 2.31)" \
		--depends "libstdc++6" \
		--depends "libgl1 | libgles2" \
		--depends "screen" \
		--deb-recommends "quetoo-data" \
		--config-files /etc/quetoo-dedicated/default.cfg \
		--config-files /etc/sudoers.d/quetoo \
		--after-install postinstall.sh \
		--before-remove preremove.sh \
		-p $(DEB) \
		-C $(PKGDIR) .

# ---------------------------------------------------------------------------
# .rpm

rpm: $(TARGET)/.pkg-stamp
	fpm -s dir -t rpm \
		-n quetoo -v $(VERSION) \
		--architecture $(ARCH) \
		--description "Free, open-source arena first-person shooter" \
		--url "https://quetoo.org" \
		--license "GPL-2.0" \
		--maintainer "jdolan <jdolan@jdolan.net>" \
		--depends "glibc >= 2.31" \
		--depends "libstdc++" \
		--depends "mesa-libGL" \
		--depends "screen" \
		--rpm-tag "Recommends: quetoo-data" \
		--config-files /etc/quetoo-dedicated/default.cfg \
		--config-files /etc/sudoers.d/quetoo \
		--after-install postinstall.sh \
		--before-remove preremove.sh \
		-p $(RPM) \
		-C $(PKGDIR) .

# ---------------------------------------------------------------------------
# S3 publishing

release: all
	aws s3 sync --delete $(STAGING) $(QUETOO_S3_BUCKET)/$(BUILD)
	git rev-list --count HEAD > $(TARGET)/version
	aws s3 cp $(TARGET)/version $(QUETOO_S3_BUCKET)/versions/$(BUILD)
	aws s3 cp $(ARCHIVE) $(QUETOO_S3_BUCKET)/snapshots/
	aws s3 cp $(DEB)     $(QUETOO_S3_BUCKET)/snapshots/
	aws s3 cp $(RPM)     $(QUETOO_S3_BUCKET)/snapshots/

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(STAGING)/share

clean:
	rm -rf $(TARGET)

.PHONY: all configure compile archive deb rpm release install-data clean

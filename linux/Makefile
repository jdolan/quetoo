# Makefile for Quetoo Linux distributable packages.
# Produces: AppImage-style archive, and quetoo-client / quetoo-server .deb + .rpm
# Requires: linuxdeploy, fpm, imagemagick

ARCH     := $(shell uname -m)
BUILD    := $(ARCH)-pc-linux
TARGET    = target
STAGING   = $(TARGET)/staging
APPDIR          = $(TARGET)/Quetoo.AppDir
APPDIR_CLIENT   = $(TARGET)/Quetoo-Client.AppDir
APPDIR_SERVER   = $(TARGET)/Quetoo-Server.AppDir
PKGDIR_CLIENT   = $(TARGET)/pkg-client
PKGDIR_SERVER   = $(TARGET)/pkg-server
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

# quetoo-client and quetoo-server are independent, fully self-contained
# packages that both include quetoo-dedicated (a player may want to run a
# local dedicated instance without pulling in quetoo-server's systemd/sudoers
# footprint). They are declared Conflicts of one another below since they'd
# otherwise both try to install the same files under /usr/lib/quetoo.
CLIENT_BINARIES = quemap quetoo quetoo-dedicated
SERVER_BINARIES = quetoo-dedicated quetoo-master

ARCHIVE     = $(TARGET)/quetoo-$(BUILD).tar.gz
DEB_CLIENT  = $(TARGET)/quetoo-client_$(VERSION)_$(DEB_ARCH).deb
DEB_SERVER  = $(TARGET)/quetoo-server_$(VERSION)_$(DEB_ARCH).deb
DEB_SHIM    = $(TARGET)/quetoo-shim_$(VERSION)_$(DEB_ARCH).deb
RPM_CLIENT  = $(TARGET)/quetoo-client-$(VERSION)-1.$(ARCH).rpm
RPM_SERVER  = $(TARGET)/quetoo-server-$(VERSION)-1.$(ARCH).rpm
RPM_SHIM    = $(TARGET)/quetoo-shim-$(VERSION)-1.$(ARCH).rpm
PKGDIR_SHIM = $(TARGET)/pkg-shim

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
	install -d $(STAGING)/bin $(STAGING)/lib $(STAGING)/share
	$(MAKE) -C .. DESTDIR=$(abspath $(STAGING)) install

$(LINUXDEPLOY):
	curl --fail --silent --show-error --location --retry 5 --retry-all-errors \
		-o $@ \
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

	ARCH=$(ARCH) LINUXDEPLOY_DISABLE_STRIP=1 $(LINUXDEPLOY) --appimage-extract-and-run \
		--appdir $(APPDIR) \
		$(foreach b,$(notdir $(BINARIES)),--executable $(APPDIR)/usr/bin/$(b)) \
		--desktop-file quetoo/Quetoo.desktop \
		--icon-file $(APPDIR)/quetoo.png

	# Install the icon at its XDG path so Sys_RegisterDesktopEntry can reference it.
	install -Dm644 $(APPDIR)/quetoo.png \
		$(APPDIR)/usr/share/icons/hicolor/256x256/apps/quetoo.png

$(APPDIR_CLIENT): $(STAGING) $(LINUXDEPLOY)
	rm -rf $(APPDIR_CLIENT)
	install -d $(APPDIR_CLIENT)/usr/bin $(APPDIR_CLIENT)/usr/lib $(APPDIR_CLIENT)/usr/share

	for bin in $(CLIENT_BINARIES); do cp -a $(STAGING)/bin/$$bin $(APPDIR_CLIENT)/usr/bin/; done
	install -d $(APPDIR_CLIENT)/usr/lib/quetoo
	for mod in $$(ls $(STAGING)/lib/quetoo); do cp -a $(STAGING)/lib/quetoo/$$mod $(APPDIR_CLIENT)/usr/lib/quetoo/; done

	convert ../src/main/quetoo.ico[0] $(APPDIR_CLIENT)/quetoo.png

	ARCH=$(ARCH) LINUXDEPLOY_DISABLE_STRIP=1 $(LINUXDEPLOY) --appimage-extract-and-run \
		--appdir $(APPDIR_CLIENT) \
		$(foreach b,$(CLIENT_BINARIES),--executable $(APPDIR_CLIENT)/usr/bin/$(b)) \
		--desktop-file quetoo/Quetoo.desktop \
		--icon-file $(APPDIR_CLIENT)/quetoo.png

	install -Dm644 $(APPDIR_CLIENT)/quetoo.png \
		$(APPDIR_CLIENT)/usr/share/icons/hicolor/256x256/apps/quetoo.png

$(APPDIR_SERVER): $(STAGING) $(LINUXDEPLOY)
	rm -rf $(APPDIR_SERVER)
	install -d $(APPDIR_SERVER)/usr/bin $(APPDIR_SERVER)/usr/lib $(APPDIR_SERVER)/usr/share

	for bin in $(SERVER_BINARIES); do cp -a $(STAGING)/bin/$$bin $(APPDIR_SERVER)/usr/bin/; done
	for mod in $$(ls $(STAGING)/lib/quetoo); do \
		install -d $(APPDIR_SERVER)/usr/lib/quetoo/$$mod; \
		cp -a $(STAGING)/lib/quetoo/$$mod/game.so $(APPDIR_SERVER)/usr/lib/quetoo/$$mod/; \
	done

	ARCH=$(ARCH) LINUXDEPLOY_DISABLE_STRIP=1 $(LINUXDEPLOY) --appimage-extract-and-run \
		--appdir $(APPDIR_SERVER) \
		$(foreach b,$(SERVER_BINARIES),--executable $(APPDIR_SERVER)/usr/bin/$(b))

# ---------------------------------------------------------------------------
# Archive

archive: $(APPDIR)
	tar czf $(ARCHIVE) -C $(APPDIR) --transform 's|^usr|quetoo|' usr/

# ---------------------------------------------------------------------------
# .deb / .rpm — reuse the linuxdeploy-bundled AppDir content so we don't
# need a separate library-bundling pass.

$(TARGET)/.pkg-client-stamp: $(APPDIR_CLIENT)
	rm -rf $(PKGDIR_CLIENT)

	# Install game under private prefix /usr/lib/quetoo so that the engine's
	# runtime path detection (strip at /bin/) resolves to /usr/lib/quetoo/lib.
	install -d $(PKGDIR_CLIENT)/usr/lib/quetoo
	cp -a $(APPDIR_CLIENT)/usr/bin $(PKGDIR_CLIENT)/usr/lib/quetoo/
	cp -a $(APPDIR_CLIENT)/usr/lib $(PKGDIR_CLIENT)/usr/lib/quetoo/
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-update $(PKGDIR_CLIENT)/usr/lib/quetoo/bin/quetoo-update
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-status $(PKGDIR_CLIENT)/usr/lib/quetoo/bin/quetoo-status

	# Desktop integration
	install -Dm644 quetoo/Quetoo.desktop \
		$(PKGDIR_CLIENT)/usr/share/applications/quetoo.desktop
	install -Dm644 quetoo/org.quetoo.Quetoo.metainfo.xml \
		$(PKGDIR_CLIENT)/usr/share/metainfo/org.quetoo.Quetoo.metainfo.xml
	install -d $(PKGDIR_CLIENT)/usr/share/icons/hicolor/256x256/apps
	convert ../src/main/quetoo.ico[0] \
		$(PKGDIR_CLIENT)/usr/share/icons/hicolor/256x256/apps/quetoo.png

	# Symlinks in /usr/bin pointing at the real binaries under /usr/lib/quetoo/bin/.
	# The kernel resolves /proc/self/exe to the real path, so the engine correctly
	# derives lib_dir and data_dir from /usr/lib/quetoo regardless of how it was invoked.
	# The RPATH ($ORIGIN/../lib) baked in by linuxdeploy also resolves from the real
	# binary location, so no LD_LIBRARY_PATH wrapper is needed.
	install -d $(PKGDIR_CLIENT)/usr/bin
	for bin in quetoo quemap quetoo-dedicated quetoo-update quetoo-status; do \
		ln -sf /usr/lib/quetoo/bin/$$bin $(PKGDIR_CLIENT)/usr/bin/$$bin; \
	done

	touch $@

$(TARGET)/.pkg-server-stamp: $(APPDIR_SERVER)
	rm -rf $(PKGDIR_SERVER)

	install -d $(PKGDIR_SERVER)/usr/lib/quetoo
	cp -a $(APPDIR_SERVER)/usr/bin $(PKGDIR_SERVER)/usr/lib/quetoo/
	cp -a $(APPDIR_SERVER)/usr/lib $(PKGDIR_SERVER)/usr/lib/quetoo/

	# Server management: systemd units, start wrapper, default config, update script.
	install -Dm644 quetoo/usr/lib/systemd/system/quetoo-dedicated@.service \
		$(PKGDIR_SERVER)/usr/lib/systemd/system/quetoo-dedicated@.service
	install -Dm644 quetoo/usr/lib/systemd/system/quetoo-master.service \
		$(PKGDIR_SERVER)/usr/lib/systemd/system/quetoo-master.service
	install -Dm644 quetoo/etc/quetoo-dedicated/default.cfg          $(PKGDIR_SERVER)/etc/quetoo-dedicated/default.cfg
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-dedicated-start $(PKGDIR_SERVER)/usr/lib/quetoo/bin/quetoo-dedicated-start
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-update          $(PKGDIR_SERVER)/usr/lib/quetoo/bin/quetoo-update
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-status          $(PKGDIR_SERVER)/usr/lib/quetoo/bin/quetoo-status
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-attach          $(PKGDIR_SERVER)/usr/lib/quetoo/bin/quetoo-attach
	install -Dm440 quetoo/etc/sudoers.d/quetoo                      $(PKGDIR_SERVER)/etc/sudoers.d/quetoo

	install -d $(PKGDIR_SERVER)/usr/bin
	for bin in quetoo-dedicated quetoo-master quetoo-attach quetoo-update quetoo-status; do \
		ln -sf /usr/lib/quetoo/bin/$$bin $(PKGDIR_SERVER)/usr/bin/$$bin; \
	done

	touch $@

# quetoo-shim is a transitional package, published only while migrating
# existing single-package `quetoo` installs to quetoo-client/quetoo-server
# (see #952/#953). It carries the full old-style monolithic file layout so
# that upgrading in place from `quetoo` doesn't drop any files dpkg/rpm no
# longer considers part of the package; only its quetoo-update learns to
# self-migrate. Drop this target once servers have had a release cycle to
# pick it up.
$(TARGET)/.pkg-shim-stamp: $(APPDIR)
	rm -rf $(PKGDIR_SHIM)

	install -d $(PKGDIR_SHIM)/usr/lib/quetoo
	cp -a $(APPDIR)/usr/bin $(PKGDIR_SHIM)/usr/lib/quetoo/
	cp -a $(APPDIR)/usr/lib $(PKGDIR_SHIM)/usr/lib/quetoo/

	install -Dm644 quetoo/Quetoo.desktop \
		$(PKGDIR_SHIM)/usr/share/applications/quetoo.desktop
	install -d $(PKGDIR_SHIM)/usr/share/icons/hicolor/256x256/apps
	convert ../src/main/quetoo.ico[0] \
		$(PKGDIR_SHIM)/usr/share/icons/hicolor/256x256/apps/quetoo.png

	install -d $(PKGDIR_SHIM)/usr/bin
	for bin in quetoo quetoo-dedicated quemap quetoo-master quetoo-attach quetoo-update quetoo-status; do \
		ln -sf /usr/lib/quetoo/bin/$$bin $(PKGDIR_SHIM)/usr/bin/$$bin; \
	done

	install -Dm644 quetoo/usr/lib/systemd/system/quetoo-dedicated@.service \
		$(PKGDIR_SHIM)/usr/lib/systemd/system/quetoo-dedicated@.service
	install -Dm644 quetoo/usr/lib/systemd/system/quetoo-master.service \
		$(PKGDIR_SHIM)/usr/lib/systemd/system/quetoo-master.service
	install -Dm644 quetoo/etc/quetoo-dedicated/default.cfg          $(PKGDIR_SHIM)/etc/quetoo-dedicated/default.cfg
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-dedicated-start $(PKGDIR_SHIM)/usr/lib/quetoo/bin/quetoo-dedicated-start
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-update          $(PKGDIR_SHIM)/usr/lib/quetoo/bin/quetoo-update
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-status          $(PKGDIR_SHIM)/usr/lib/quetoo/bin/quetoo-status
	install -Dm755 quetoo/usr/lib/quetoo/bin/quetoo-attach          $(PKGDIR_SHIM)/usr/lib/quetoo/bin/quetoo-attach
	install -Dm440 quetoo/etc/sudoers.d/quetoo                      $(PKGDIR_SHIM)/etc/sudoers.d/quetoo

	touch $@

# ---------------------------------------------------------------------------
# .deb

deb: deb-client deb-server deb-shim

deb-client: $(TARGET)/.pkg-client-stamp
	fpm -s dir -t deb \
		-n quetoo-client -v $(VERSION) \
		--architecture $(DEB_ARCH) \
		--description "Free, open-source arena first-person shooter" \
		--url "https://quetoo.org" \
		--license "GPL-2.0" \
		--maintainer "jdolan <jdolan@jdolan.net>" \
		--depends "libc6 (>= 2.31)" \
		--depends "libstdc++6" \
		--depends "libgl1 | libgles2" \
		--depends "libfreetype6" \
		--depends "libharfbuzz0b" \
		--depends "bash" \
		--depends "curl" \
		--depends "python3" \
		--deb-recommends "quetoo-data" \
		--replaces "quetoo" \
		--conflicts "quetoo" \
		--conflicts "quetoo-server" \
		-p $(DEB_CLIENT) \
		-C $(PKGDIR_CLIENT) .

deb-server: $(TARGET)/.pkg-server-stamp
	fpm -s dir -t deb \
		-n quetoo-server -v $(VERSION) \
		--architecture $(DEB_ARCH) \
		--description "Free, open-source arena first-person shooter (dedicated server)" \
		--url "https://quetoo.org" \
		--license "GPL-2.0" \
		--maintainer "jdolan <jdolan@jdolan.net>" \
		--depends "libc6 (>= 2.31)" \
		--depends "libstdc++6" \
		--depends "screen" \
		--depends "bash" \
		--depends "curl" \
		--depends "python3" \
		--deb-recommends "quetoo-data" \
		--config-files /etc/quetoo-dedicated/default.cfg \
		--config-files /etc/sudoers.d/quetoo \
		--after-install quetoo/postinstall.sh \
		--before-remove quetoo/preremove.sh \
		--replaces "quetoo" \
		--conflicts "quetoo" \
		--conflicts "quetoo-client" \
		-p $(DEB_SERVER) \
		-C $(PKGDIR_SERVER) .

deb-shim: $(TARGET)/.pkg-shim-stamp
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
		--depends "bash" \
		--depends "curl" \
		--depends "python3" \
		--deb-recommends "quetoo-data" \
		--config-files /etc/quetoo-dedicated/default.cfg \
		--config-files /etc/sudoers.d/quetoo \
		--after-install quetoo/postinstall.sh \
		--before-remove quetoo/preremove.sh \
		-p $(DEB_SHIM) \
		-C $(PKGDIR_SHIM) .

# ---------------------------------------------------------------------------
# .rpm

rpm: rpm-client rpm-server rpm-shim

rpm-client: $(TARGET)/.pkg-client-stamp
	fpm -s dir -t rpm \
		-n quetoo-client -v $(VERSION) \
		--architecture $(ARCH) \
		--description "Free, open-source arena first-person shooter" \
		--url "https://quetoo.org" \
		--license "GPL-2.0" \
		--maintainer "jdolan <jdolan@jdolan.net>" \
		--depends "glibc >= 2.31" \
		--depends "libstdc++" \
		--depends "mesa-libGL" \
		--depends "freetype" \
		--depends "harfbuzz" \
		--depends "bash" \
		--depends "curl" \
		--depends "python3" \
		--rpm-tag "Recommends: quetoo-data" \
		--replaces "quetoo" \
		--conflicts "quetoo" \
		--conflicts "quetoo-server" \
		-p $(RPM_CLIENT) \
		-C $(PKGDIR_CLIENT) .

rpm-server: $(TARGET)/.pkg-server-stamp
	fpm -s dir -t rpm \
		-n quetoo-server -v $(VERSION) \
		--architecture $(ARCH) \
		--description "Free, open-source arena first-person shooter (dedicated server)" \
		--url "https://quetoo.org" \
		--license "GPL-2.0" \
		--maintainer "jdolan <jdolan@jdolan.net>" \
		--depends "glibc >= 2.31" \
		--depends "libstdc++" \
		--depends "screen" \
		--depends "bash" \
		--depends "curl" \
		--depends "python3" \
		--rpm-tag "Recommends: quetoo-data" \
		--config-files /etc/quetoo-dedicated/default.cfg \
		--config-files /etc/sudoers.d/quetoo \
		--after-install quetoo/postinstall.sh \
		--before-remove quetoo/preremove.sh \
		--replaces "quetoo" \
		--conflicts "quetoo" \
		--conflicts "quetoo-client" \
		-p $(RPM_SERVER) \
		-C $(PKGDIR_SERVER) .

rpm-shim: $(TARGET)/.pkg-shim-stamp
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
		--depends "bash" \
		--depends "curl" \
		--depends "python3" \
		--rpm-tag "Recommends: quetoo-data" \
		--config-files /etc/quetoo-dedicated/default.cfg \
		--config-files /etc/sudoers.d/quetoo \
		--after-install quetoo/postinstall.sh \
		--before-remove quetoo/preremove.sh \
		-p $(RPM_SHIM) \
		-C $(PKGDIR_SHIM) .

# ---------------------------------------------------------------------------
# S3 publishing

release: all
	aws s3 sync --delete $(STAGING) $(QUETOO_S3_BUCKET)/$(BUILD)
	git rev-list --count HEAD > $(TARGET)/version
	aws s3 cp $(TARGET)/version $(QUETOO_S3_BUCKET)/versions/$(BUILD)
	aws s3 cp $(ARCHIVE)    $(QUETOO_S3_BUCKET)/snapshots/
	aws s3 cp $(DEB_CLIENT) $(QUETOO_S3_BUCKET)/snapshots/
	aws s3 cp $(DEB_SERVER) $(QUETOO_S3_BUCKET)/snapshots/
	aws s3 cp $(RPM_CLIENT) $(QUETOO_S3_BUCKET)/snapshots/
	aws s3 cp $(RPM_SERVER) $(QUETOO_S3_BUCKET)/snapshots/

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(STAGING)/share

clean:
	rm -rf $(TARGET)

.PHONY: all configure compile archive deb deb-client deb-server deb-shim rpm rpm-client rpm-server rpm-shim release install-data clean

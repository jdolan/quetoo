# Makefile for Quetoo.app bundle assembly, code signing, and notarization.
# Requires dylibbundler: https://github.com/auriamg/macdylibbundler
#
# Typical local workflow:
#   make                    # build, install, bundle libs, move assets
#   make sign               # sign with Developer ID (cert must be in Keychain)
#   make dmg                # create distributable DMG
#   make notarize           # notarize and staple DMG (needs APPLE_* env vars)
#
# For the bundle (with game data) DMG:
#   make bundle-data QUETOO_DATA_DIR=../../quetoo-data
#   make bundle-sign
#   make bundle-notarize

CODESIGN_IDENTITY ?= "Developer ID Application: Jay Dolan (C5PXKE9CU5)"
QUETOO_DATA_DIR   ?= ../../quetoo-data

ARCH      = arm64
BUILD     = $(ARCH)-apple-darwin
TARGET    = target
INSTALL   = $(TARGET)/Quetoo.app
BINDIR    = $(INSTALL)/Contents/MacOS
LIBDIR    = $(BINDIR)/lib
MODULEDIR = $(LIBDIR)/quetoo
DATADIR   = $(INSTALL)/Contents/Resources

SNAPSHOT        = $(TARGET)/Quetoo-$(BUILD).dmg
BUNDLE_SNAPSHOT = $(TARGET)/Quetoo-bundle-$(BUILD).dmg
VOLUME_NAME     = "Quetoo"
DMG_ROOT        = $(TARGET)/dmg-root

all: configure compile pre-install install install-lib move-resources

configure:
	cd .. ; autoreconf -i; ./configure CFLAGS='-g -O2' --prefix=/

compile:
	make -C .. -j$(shell sysctl -n hw.logicalcpu) all

pre-install:
	install -d $(INSTALL)
	cp -r Quetoo.app/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install:
	ln -f -s Contents/MacOS $(INSTALL)/bin
	ln -f -s Contents/MacOS/lib $(INSTALL)/lib
	ln -f -s Contents/Resources $(INSTALL)/share
	make -C .. DESTDIR=$(realpath $(INSTALL)) install
	rm -f $(INSTALL)/bin $(INSTALL)/lib $(INSTALL)/share

install-lib:
	dylibbundler -b \
		-x $(BINDIR)/quemap \
		-x $(BINDIR)/quetoo \
		-x $(BINDIR)/quetoo-dedicated \
		-x $(BINDIR)/quetoo-master \
		$$(find $(MODULEDIR) -name '*.so' | sed 's/^/-x /') \
		-d $(LIBDIR) -of -p @executable_path/lib

# Move non-Mach-O engine assets (UI layouts, shaders, images) out of
# lib/quetoo/ and into Contents/Resources/, preserving the relative path, so
# that lib/quetoo/default/shaders lands in Resources/default/shaders and
# lib/quetoo/default/ui in Resources/default/ui. The engine searches both on
# macOS.
# This leaves only .so plugins under lib/quetoo/, which codesign
# --verify --deep handles cleanly.
move-resources:
	@mkdir -p $(DATADIR)
	@if [ -d "$(MODULEDIR)" ]; then \
		find "$(MODULEDIR)" -mindepth 1 -type f ! -name "*.so" ! -name "*.dylib" \
			| while IFS= read -r src; do \
				rel="$${src#$(MODULEDIR)/}"; \
				dest="$(DATADIR)/$$rel"; \
				mkdir -p "$$(dirname "$$dest")"; \
				mv "$$src" "$$dest"; \
			done; \
		find "$(MODULEDIR)" -mindepth 1 -type d -empty -delete 2>/dev/null || true; \
	fi

# Sign all Mach-O code in the bundle, then seal the bundle.
# lib/quetoo/default/ contains only .so plugins after move-resources, so
# codesign --verify --deep finds no mixed-content issues.
sign:
	find "$(INSTALL)" -type f \( -name "*.dylib" -o -name "*.so" \) \
		-exec codesign --force --timestamp --options runtime \
			--sign $(CODESIGN_IDENTITY) {} \;
	for exe in quemap quetoo quetoo-dedicated quetoo-master; do \
		bin="$(BINDIR)/$$exe"; \
		if [ -f "$$bin" ]; then \
			codesign --force --timestamp --options runtime \
				--sign $(CODESIGN_IDENTITY) "$$bin"; \
		fi; \
	done
	codesign --force --timestamp --options runtime \
		--sign $(CODESIGN_IDENTITY) $(INSTALL)
	codesign --verify --deep $(INSTALL)

dmg-layout:
	rm -rf "$(DMG_ROOT)"
	mkdir -p "$(DMG_ROOT)"
	cp -R "$(INSTALL)" "$(DMG_ROOT)/"
	ln -s /Applications "$(DMG_ROOT)/Applications"

dmg: dmg-layout
	hdiutil create -volname $(VOLUME_NAME) -srcfolder "$(DMG_ROOT)" \
		-ov -format UDZO $(SNAPSHOT)

# Notarize and staple the binary DMG.
# Requires env vars: APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD
notarize: dmg
	xcrun notarytool submit $(SNAPSHOT) \
		--apple-id "$$APPLE_ID" \
		--team-id "$$APPLE_TEAM_ID" \
		--password "$$APPLE_APP_PASSWORD" \
		--wait
	xcrun stapler staple $(SNAPSHOT)

# Copy quetoo-data into Contents/Resources/ to produce a self-contained bundle.
bundle-data:
	mkdir -p $(DATADIR)/default
	cp -r $(QUETOO_DATA_DIR)/target/default/. $(DATADIR)/default/

# Re-seal the bundle after adding game data.
bundle-sign:
	codesign --force --timestamp --options runtime \
		--sign $(CODESIGN_IDENTITY) $(INSTALL)
	codesign --verify --deep $(INSTALL)

bundle-dmg: dmg-layout
	hdiutil create -volname $(VOLUME_NAME) -srcfolder "$(DMG_ROOT)" \
		-ov -format UDZO $(BUNDLE_SNAPSHOT)

# Notarize and staple the bundle DMG.
# Requires env vars: APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD
bundle-notarize: bundle-dmg
	xcrun notarytool submit $(BUNDLE_SNAPSHOT) \
		--apple-id "$$APPLE_ID" \
		--team-id "$$APPLE_TEAM_ID" \
		--password "$$APPLE_APP_PASSWORD" \
		--wait
	xcrun stapler staple $(BUNDLE_SNAPSHOT)

clean:
	rm -rf $(TARGET)/*

.PHONY: all configure compile pre-install install install-lib move-resources \
        sign dmg-layout dmg notarize bundle-data bundle-sign \
        bundle-dmg bundle-notarize clean

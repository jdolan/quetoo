# Copyright 1999-2008 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit games subversion

DESCRIPTION="Quake2World is a Free, standalone first person shooter video game.
Our goal is to bring the fun and excitement of straightforward old-school
death-match gaming to a more contemporary platform, and perhaps to a new
generation of gamers."
HOMEPAGE="http://quake2world.net/"
SRC_URI=""

ESVN_REPO_URI="svn://jdolan.dyndns.org/quake2world/trunk/"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~x86 ~amd64"
IUSE=""

DEPEND="${DEPEND}
	>=media-libs/libsdl-1.2
	media-libs/sdl-image
	net-misc/curl
"
RDEPEND="${DEPEND}"

src_compile() {
	MAKEOPTS="${MAKEOPTS} -j1"
	autoreconf -if 
	econf || die "configure failed"
	emake || die "make failed"
}

src_install() {
	make DESTDIR="${D}" install || die "install failed"
	insinto "/usr/share/applications"
	doins "desktop/quake2world.desktop"
	insinto "/usr/share/pixmaps"
	doins "desktop/quake2world.png"
}

pkg_postinst() {
	games_pkg_postinst
	einfo "This install is just the engine, you still need"
	einfo "data. Please install quake2world-data."
}


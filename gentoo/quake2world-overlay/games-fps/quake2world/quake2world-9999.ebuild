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
IUSE="nodata"

DEPEND="${DEPEND}
	>=media-libs/libsdl-1.2
	media-libs/sdl-image
	net-misc/curl
	net-misc/rsync
"
RDEPEND="${DEPEND}"

src_compile() {
	MAKEOPTS="${MAKEOPTS} -j1"
	autoreconf -if 
	egamesconf --without-svgalib || die "configure failed"
	emake || die "make failed"
}

src_install() {
	make DESTDIR="${D}" install || die "install failed"
	insinto "/usr/share/applications"
	doins "desktop/quake2world.desktop"
	insinto "/usr/share/pixmaps"
	doins "desktop/quake2world.png"
	insinto "/usr/game/bin"
	dogamesbin "${FILESDIR}/quake2world-update-data"
	prepgamesdirs
}

pkg_postinst() {
	games_pkg_postinst
	if use nodata ; then
		einfo "Only the game engine was  installed.  You still need to"
		einfo "install data. Please run quake2world-update-data as root."
	else
		einfo "Installing data, this could take a while..."
		/usr/games/bin/quake2world-update-data
	fi
}


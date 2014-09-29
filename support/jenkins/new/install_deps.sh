yum -y install openssh-clients rsync check check-devel clang-analyzer libtool

yum -y install SDL2-devel SDL2_image-devel \
SDL2_mixer-devel curl-devel physfs-devel glib2-devel \
libjpeg-turbo-devel zlib-devel ncurses-devel libxml2-devel

yum -y install SDL2-devel.i686 SDL2_image-devel.i686 \
SDL2_mixer-devel.i686 curl-devel.i686 physfs-devel.i686 glib2-devel.i686 \
libjpeg-turbo-devel.i686 zlib-devel.i686 ncurses-devel.i686 libxml2-devel.i686

yum -y install mingw64-SDL2 mingw64-SDL2_image mingw64-SDL2_mixer \
mingw64-curl mingw64-physfs mingw64-glib2 mingw64-libjpeg-turbo \
mingw64-zlib mingw64-pkg-config mingw64-libxml2 \
https://raw.github.com/maci0/rpmbuild/master/RPMS/noarch/mingw64-SDL2_image-2.0.0-3.fc20.noarch.rpm \
https://raw.github.com/maci0/rpmbuild/master/RPMS/noarch/mingw64-SDL2_mixer-2.0.0-3.fc20.noarch.rpm
#mingw64-pdcurses broken for now

yum -y install mingw32-SDL2 mingw32-SDL2_image mingw32-SDL2_mixer \
mingw32-curl mingw32-physfs mingw32-glib2 mingw32-libjpeg-turbo \
mingw32-zlib mingw32-pkg-config mingw32-libxml2 \
https://raw.github.com/maci0/rpmbuild/master/RPMS/noarch/mingw32-SDL2_image-2.0.0-3.fc20.noarch.rpm \
https://raw.github.com/maci0/rpmbuild/master/RPMS/noarch/mingw32-SDL2_mixer-2.0.0-3.fc20.noarch.rpm
#mingw32-pdcurses broken for now

yum -y upgrade


Name:           quake2world
Version:        0.0.1
Release:        1%{?dist}
Summary:        A Free FPS game.

Group:          Amusements/Games
License:        GPL
URL:            http://quake2world.net
Source0:        http://quake2world.net/files/quake2world-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  curl-devel libsdl-devel

%description
Quake2World is an unsupported, unofficial, multiplayer-only iteration of id Software's Quake II. 
It aims to blend the very best aspects of the entire Quake series to deliver an enjoyable 
FPS experience in a free to download, stand-alone game.

%prep
%setup -q -n quake2world-%{version}

%build
%configure --with-sdl --with-curl --with-zlib --with-opengl
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

desktop-file-install \
        --vendor fedora \
        --dir $RPM_BUILD_ROOT/%{_datadir}/applications/ \
        --delete-original \
        --remove-category=Application \
        --add-category=Shooter \
        $RPM_BUILD_ROOT%{_datadir}/applications/quake2world.desktop

mkdir -p $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/32x32/apps
mv $RPM_BUILD_ROOT%{_datadir}/pixmaps/quake2world.png \
  $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/32x32/apps
rmdir $RPM_BUILD_ROOT%{_datadir}/pixmaps

%clean
rm -rf $RPM_BUILD_ROOT

%post
touch --no-create %{_datadir}/icons/hicolor || :
%{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor || :

%postun
touch --no-create %{_datadir}/icons/hicolor || :
%{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor || :

%files
%defattr(-,root,root,-)
%{_bindir}/quake2world
%{_datadir}/quake2world
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/32x32/apps/*.png
%doc AUTHORS COPYING NEWS README docs/*.txt

%changelog
* Tue Jul 17 2007 maci <maci@satgnu.net> 0.0.0.1-1
- Initial release


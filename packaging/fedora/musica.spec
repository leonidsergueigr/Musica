Name:           musica
Version:        0.1.0
Release:        1%{?dist}
Summary:        Terminal music player with album art and a live spectrum

License:        MIT
URL:            https://github.com/leonidsergueigr/Musica
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  zlib-devel
# TODO(owner): confirm that the target Fedora release ships FTXUI 7 and TagLib 2.
BuildRequires:  ftxui-devel >= 7
BuildRequires:  taglib-devel >= 2.0

%description
musica is a terminal music player. It shows a folder browser and a play queue
side by side, with album art read from tags or a nearby cover file, and a
multi-line real-time spectrum analyser. It supports M3U playlists and English
and French interfaces.

%prep
%autosetup -n Musica-%{version}

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DMUSICA_SYSTEM_DEPS=ON \
    -DMUSICA_BUILD_TESTS=ON
%cmake_build

%check
%ctest

%install
%cmake_install

%files
%{_bindir}/musica
%{_mandir}/man1/musica.1*
%license %{_docdir}/musica/LICENSE
%license %{_docdir}/musica/THIRD_PARTY_LICENSES.md
%doc %{_docdir}/musica/README.md
%doc %{_docdir}/musica/README.fr.md
%doc %{_docdir}/musica/CHANGELOG.md

%changelog
* Tue Nov 17 2026 Léonid Gr <leonidgrdev@gmail.com> - 0.1.0-1
- Initial package

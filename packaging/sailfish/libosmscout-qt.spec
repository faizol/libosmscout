
Name: libosmscout-qt

Summary: libosmscout qt libraries
Version: 0.0.git.20160915
Release: 1
Group: Qt/Qt
License: LGPL
Source0:    %{name}-%{version}.tar.bz2

Requires: protobuf

BuildRequires: cmake
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: protobuf-devel
BuildRequires: libxml2-devel

%description
libosmscout qt libraries

%package devel
Summary: libosmscout qt development header files
Group: Development/Libraries
Requires: %{name} = %{version}

%description devel
libosmscout qt libraries - development files


%build
%cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DOSMSCOUT_BUILD_MAP_OPENGL=OFF -DOSMSCOUT_BUILD_IMPORT=ON -DOSMSCOUT_BUILD_MAP_AGG=OFF -DOSMSCOUT_BUILD_MAP_CAIRO=OFF -DOSMSCOUT_BUILD_MAP_SVG=OFF -DOSMSCOUT_BUILD_MAP_IOSX=OFF -DOSMSCOUT_BUILD_TESTS=OFF -DOSMSCOUT_BUILD_DEMOS=OFF -DOSMSCOUT_BUILD_BINDING_JAVA=OFF -DOSMSCOUT_BUILD_BINDING_CSHARP=OFF -DOSMSCOUT_BUILD_DOC_API=OFF -DOSMSCOUT_BUILD_CLIENT_QT=OFF -DOSMSCOUT_BUILD_TOOL_OSMSCOUT2=OFF -DOSMSCOUT_BUILD_TOOL_STYLEEDITOR=OFF -DGPERFTOOLS_USAGE=OFF -DOSMSCOUT_BUILD_TOOL_IMPORT=OFF -DOSMSCOUT_BUILD_TOOL_DUMPDATA=OFF .
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%check
ctest -V %{?_smp_mflags}

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%{_libdir}/libosmscout.so
%{_libdir}/libosmscout_import.so
%{_libdir}/libosmscout_map.so
%{_libdir}/libosmscout_map_qt.so

%files devel
%defattr(-, root, root, 0755)
%{_includedir}/osmscout

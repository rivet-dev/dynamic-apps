#
# gtksourceview.spec
#

%define api_version	1.0
%define lib_major 0
%define lib_name	%mklibname %{name}- %{api_version} %{lib_major}

Summary:	Source code viewing library
Name:		gtksourceview
Version: 	1.7.2
Release:	%mkrel 1
License:	GPL
Group:		Editors
URL:		http://people.ecsc.co.uk/~matt/downloads/rpms/gtksourceview/
Source0:	https://download.gnome.org/sources/%{name}/%{name}-%{version}.tar.xz
BuildRoot:	%{_tmppath}/%{name}-%{version}
BuildRequires:	libgtk+2-devel >= 2.3.0
BuildRequires:  libgnome-vfs2-devel >= 2.2.0
BuildRequires:  libgnomeprintui-devel >= 2.7.0
BuildRequires:  perl-XML-Parser
Conflicts:	gtksourceview-sharp <= 0.5-3mdk

%description
GtkSourceview is a library that adds syntax highlighting,
line numbers, and other programming-editor features.
GtkSourceView specializes these features for a code editor.

%package -n %{lib_name}
Summary:	Source code viewing library
Group:		Editors
Requires:	%{name} >= %{version}-%{release}
Provides:	lib%{name} = %{version}-%{release}
Provides:	libgtksourceview0 = %{version}-%{release}
Obsoletes:	libgtksourceview0
Provides:   libgtksourceview1.0 = %{version}-%{release}
Obsoletes:  libgtksourceview1.0

%description -n %{lib_name}
GtkSourceview is a library that adds syntax highlighting,
line numbers, and other programming-editor features.
GtkSourceView specializes these features for a code editor.

%package -n %{lib_name}-devel
Summary:        Libraries and include files for GtkSourceView
Group:          Development/GNOME and GTK+
Requires:       %{lib_name} = %{version}
Provides:	%{name}-devel = %{version}-%{release}
Provides:	lib%{name}-devel = %{version}-%{release}
Provides:	lib%{name}-%{api_version}-devel = %{version}-%{release}
Provides:	libgtksourceview0-devel = %{version}-%{release}
Obsoletes:	libgtksourceview0-devel
Provides:   libgtksourceview1.0-devel = %{version}-%{release}
Obsoletes:   libgtksourceview1.0-devel

%description -n %{lib_name}-devel
GtkSourceView development files


%prep
%setup -q

%build

%configure2_5x

%make

%install
rm -rf %{buildroot}

%makeinstall_std

%{find_lang} %{name}-%{api_version}

%post -n %{lib_name} -p /sbin/ldconfig

%postun -n %{lib_name} -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files -f %{name}-%{api_version}.lang
%defattr(-,root,root)
%doc AUTHORS ChangeLog NEWS README TODO
%{_datadir}/gtksourceview-%{api_version}

%files -n %{lib_name}
%defattr(-,root,root)
%{_libdir}/*.so.*

%files -n %{lib_name}-devel
%defattr(-,root,root)
%doc %{_datadir}/gtk-doc/html/gtksourceview
%{_libdir}/*.so
%attr(644,root,root) %{_libdir}/*.la
%{_includedir}/*
%{_libdir}/pkgconfig/*

%changelog
* Tue Aug 08 2006 GÃ¶tz Waschk <waschk@mandriva.org> 1.7.2-1mdv2007.0
- New release 1.7.2

* Tue Jul 25 2006 GÃ¶tz Waschk <waschk@mandriva.org> 1.7.1-1mdk
- New release 1.7.1

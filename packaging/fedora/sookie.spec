# Sookie for Fedora, built from a source tarball of this repository
# (git archive --prefix=sookie/ -o sookie.tar.gz HEAD). Sookie installs next
# to Fedora's mutter: it shares mutter-common's GSettings schemas (they are
# identical) and leaves out the few tools mutter also ships.
%global debug_package %{nil}
%{!?commit: %global commit unknown}

Name:           sookie
Version:        51.1
Release:        0.git%{commit}%{?dist}
Summary:        Window manager and compositor library for the Cali desktop
License:        GPL-2.0-or-later
URL:            https://github.com/CaliDE-Foundation/Sookie
Source0:        sookie.tar.gz

BuildRequires:  meson gcc gcc-c++ pkgconfig(glib-2.0) pkgconfig(gtk4) pkgconfig(wayland-server)
Requires:       mutter-common >= 51.1

%description
Sookie is a fork of GNOME's Mutter used by the Cali desktop. It installs
libsookie-52 and the sookie binary alongside Mutter, so GNOME keeps working.

%prep
%autosetup -n sookie

%build
%meson -Dtests=disabled -Dmutter_tests=false -Dclutter_tests=false \
       -Dcogl_tests=false -Dinstalled_tests=false -Dprofiler=false
%meson_build

%install
%meson_install
# Shipped by mutter / mutter-common already.
rm -f %{buildroot}%{_bindir}/gdctl %{buildroot}%{_bindir}/gnome-service-client
rm -f %{buildroot}%{_datadir}/bash-completion/completions/gdctl
rm -f %{buildroot}%{_datadir}/glib-2.0/schemas/org.gnome.mutter*.gschema.xml
rm -f %{buildroot}%{_datadir}/GConf/gsettings/mutter-schemas.convert
cd %{buildroot}
find . \( -type f -o -type l \) | sed 's#^\.##' > %{_builddir}/sookie-files.list

%files -f %{_builddir}/sookie-files.list
%dir %{_libdir}/sookie-52
%dir %{_includedir}/sookie-52

%changelog
* Mon Nov 23 2026 CaliDE Foundation - 51.1-0
- First package

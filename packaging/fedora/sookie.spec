# Sookie for Fedora, built from a source tarball of this repository
# with its meson subprojects downloaded (meson subprojects download; tar
# of the tree as sookie/). Sookie installs next
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

# Everything else: `dnf builddep mutter` (Sookie needs what Mutter needs).
BuildRequires:  meson gcc
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
rm -f %{buildroot}%{_mandir}/man1/gdctl.1* %{buildroot}%{_mandir}/man1/gnome-service-client.1*
rm -f %{buildroot}%{_datadir}/glib-2.0/schemas/org.gnome.mutter*.gschema.xml
rm -f %{buildroot}%{_datadir}/GConf/gsettings/mutter-schemas.convert
cd %{buildroot}
# Man pages are listed below: rpm compresses them after this runs.
find . \( -type f -o -type l \) | sed 's#^\.##' | grep -v '^/usr/share/man/' > %{_builddir}/sookie-files.list

%files -f %{_builddir}/sookie-files.list
%dir %{_libdir}/sookie-52
%dir %{_includedir}/sookie-52
%{_mandir}/man1/sookie.1*

%changelog
* Mon Nov 23 2026 CaliDE Foundation - 51.1-0
- First package

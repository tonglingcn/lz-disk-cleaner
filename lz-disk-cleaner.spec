#
# spec file for package lz-disk-cleaner
#
# Copyright (c) 2026 克亮 <315707022@qq.com>
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

Name:           lz-disk-cleaner
Version:        1.2.0
Release:        1
Summary:        Disk cleaning and optimization tool for Linux

License:        GPL-3.0-or-later
URL:            https://github.com/tonglingcn/lz-disk-cleaner
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig(gl)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(vulkan)
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(Qt6Core) >= 6.4
BuildRequires:  pkgconfig(Qt6Gui) >= 6.4
BuildRequires:  pkgconfig(Qt6Widgets) >= 6.4
BuildRequires:  pkgconfig(Qt6Network) >= 6.4
BuildRequires:  pkgconfig(Qt6Svg) >= 6.4
BuildRequires:  pkgconfig(Qt6SvgWidgets) >= 6.4

Requires:       polkit
Requires:       psmisc
Requires:       lsof
Recommends:     google-noto-color-emoji-fonts
Recommends:     lm_sensors

%description
LZ Disk Cleaner is a comprehensive disk cleaning and system optimization
tool designed for Linux distributions.

Main Features:
- Disk usage analysis with multi-level directory scanning
- Smart and custom cleanup modes
- APT source management (add, edit, enable/disable, delete)
- Startup application management
- File shredder with secure deletion
- System slimming (large files, duplicates finder)
- Hardware resource monitoring (CPU, Memory, Network, GPU)
- Linglong application management (Deepin)
- Immutable system snapshot support

%prep
%setup -q -n %{name}-%{version}

%build
mkdir -p build
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?_smp_mflags}

%install
cd build
%make_install

# Install desktop file
install -Dm644 debian/%{name}.desktop %{buildroot}%{_datadir}/applications/%{name}.desktop

# Install polkit policy
install -Dm644 src/helper/com.deepin.pkexec.disk-cleaner.policy \
    %{buildroot}%{_datadir}/polkit-1/actions/com.deepin.pkexec.disk-cleaner.policy

%files
%doc README.md
%license debian/copyright
%{_bindir}/%{name}
%{_bindir}/%{name}-helper
%{_datadir}/applications/%{name}.desktop
%{_datadir}/polkit-1/actions/com.deepin.pkexec.disk-cleaner.policy

%changelog
* Sat Apr 12 2026 克亮 <315707022@qq.com> - 1.2.0-1
- Add built-in whitelist protection in helper for log cleanup
- Fix /persistent partition scanning (du timeout and threshold)
- Fix unknown size display in scan results
- Sync version to 1.2.0

* Wed Mar 19 2026 克亮 <315707022@qq.com> - 1.1.1-1
- New features: hardware monitoring, APT source management, startup apps
- Add distro-adaptive title support
- Add dark theme support
- Various bug fixes and improvements

* Mon Jan 01 2026 克亮 <315707022@qq.com> - 1.0.0-1
- Initial release

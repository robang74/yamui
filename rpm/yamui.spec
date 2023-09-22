Name:       yamui
Summary:    Minimal UI tool for displaying simple graphical indicators
Version:    1.0.6.6
Release:    2
Url:        https://github.com/robang74/yamui.git
Group:      System/Boot
License:    ASL 2.0
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  pkgconfig(libpng)
BuildRequires:  pkgconfig(libdrm)

%description
%{summary}.

%global debug_package %{nil}

%prep
%setup -q -n %{name}-%{version}

%build
make -j$(nproc)

%install 

%make_install

%files
%defattr(-,root,root,-)
%{_bindir}/mstime
%{_bindir}/ustime
%{_bindir}/nstime
%{_bindir}/%{name}
%{_bindir}/%{name}-powerkey
%{_bindir}/%{name}-screensaverd

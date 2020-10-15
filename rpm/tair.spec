Name:           tair
Version:        3.2.4
Release:        1
Summary:        Taobao key/value storage system
Group: Application
Packager: taobao <opensource@taobao.com>
Vendor: TaoBao
Prefix:%{_prefix}
%define _install_path local/tair_bin

License:        GPL
URL:            http://yum.corp.alimama.com            
Source0:        %{NAME}-%{VERSION}.tar.gz

BuildRequires: automake >= 1.7.0
BuildRequires: libtool >= 1.5.0
BuildRequires: openssl-devel >= 0.9


Requires: openssl >= 0.9
Requires: curl

%description
Tair is a high performance, distribution key/value storage system.

%package devel
Summary: tair c++ client library
Group: Development/Libraries
Requires: t-csrd-tbnet-devel = 1.0.8
Requires: t_libeasy = 1.0.17
Requires: t-diamond >= 1.0.3

%description devel
The %name-devel package contains  libraries and header
files for developing applications that use the %name package.

%prep
%autosetup

%build
export TBLIB_ROOT=/opt/csr/common
chmod u+x bootstrap.sh
./bootstrap.sh
mkdir -p %{_prefix}/%{_install_path}
./configure --prefix=%{_prefix}/%{_install_path} --with-svn=%{svn} --with-release=yes --with-compress=no
make %{?_smp_mflags}

%install
#rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%post
echo %{_prefix}/%{_install_path}/lib > /etc/ld.so.conf.d/tair-%{VERSION}.conf
echo /opt/csr/common/lib >> /etc/ld.so.conf.d/tair-%{VERSION}.conf
echo /usr/local/lib >> /etc/ld.so.conf.d/tair-%{VERSION}.conf
echo /usr/local/lib64 >> /etc/ld.so.conf.d/tair-%{VERSION}.conf
/sbin/ldconfig

%post devel
echo %{_prefix}/%{_install_path}/lib > /etc/ld.so.conf.d/tair-%{VERSION}.conf
echo /opt/csr/common/lib >> /etc/ld.so.conf.d/tair-%{VERSION}.conf
echo /usr/local/lib >> /etc/ld.so.conf.d/tair-%{VERSION}.conf
echo /usr/local/lib64 >> /etc/ld.so.conf.d/tair-%{VERSION}.conf
/sbin/ldconfig

%postun
rm  -f /etc/ld.so.conf.d/tair-%{VERSION}.conf

%files
%defattr(0755, root, root)
%{_prefix}/%{_install_path}/sbin
%{_prefix}/%{_install_path}/lib
%config(noreplace) %{_prefix}/%{_install_path}/etc/*
%attr(0755, root, root) %{_prefix}/%{_install_path}/set_shm.sh
%attr(0755, root, root) %{_prefix}/%{_install_path}/tair.sh
%attr(0755, root, root) %{_prefix}/%{_install_path}/do_dump.sh
%attr(0755, root, root) %{_prefix}/%{_install_path}/do_upload.sh

%files devel
%{_prefix}/%{_install_path}/include
%{_prefix}/%{_install_path}/lib/libtairclientapi.*
%{_prefix}/%{_install_path}/lib/libsample_plugin.*
%{_prefix}/%{_install_path}/lib/libtairclientapi_impl.*
%{_prefix}/%{_install_path}/lib/libmdb.*
%{_prefix}/%{_install_path}/lib/libmdb_c.*



%changelog
* Mon Sep 14 2020 long_xingkai <longxingkai@talkweb.com.cn>
-ompatible with openeuler operating system

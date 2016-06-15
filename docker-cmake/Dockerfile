FROM centos:6.7
MAINTAINER "Mark Hudson" <mhudson@rubiconproject.com>
ENV container docker

# commmands inside the Dockerfile run as root (no sudo needed)
RUN yum install -y 'wget' which yum-utils mlocate cmake
# CentOS 6 comes with gcc-4.4
# RUN yum install -y gcc

## RHEL/CentOS 6 64-Bit ##
RUN wget http://download.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm && rpm -ivh epel-release-6-8.noarch.rpm
# install RPM(s) from EPEL
# RUN yum install -y cowsay fortune-mod

# this scl (Software Collection) contains gcc 4.9
RUN yum install -y centos-release-scl && yum-config-manager --enable rhel-server-rhscl-7-rpms
# RUN yum install -y devtoolset-3
RUN yum install -y devtoolset-3-gcc devtoolset-3-gcc-c++
RUN ln -s /opt/rh/devtoolset-3/root/usr/bin/c++ /usr/bin/
RUN ln -s /opt/rh/devtoolset-3/root/usr/bin/* /usr/local/bin/
RUN mkdir -p                                                        /usr/lib/gcc/
RUN ln -s /opt/rh/devtoolset-3/root/usr/lib/gcc/x86_64-redhat-linux /usr/lib/gcc/

RUN updatedb
RUN echo "#!/bin/bash" > cat-issue.sh && echo cat /etc/issue >> cat-issue.sh && chmod a+x cat-issue.sh
# CMD ["cat-issue.sh"]

# VOLUME [ "/sys/fs/cgroup" ]
#
CMD cat /etc/centos-release && rpm -qa --last | head -n22 \
    && gcc --version

# vim: ft=dockerfile

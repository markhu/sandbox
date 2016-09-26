FROM centos:6.7
MAINTAINER "Mark Hudson" <markhu@gmail.com>
ENV container docker

LABEL org.label-schema.vcs-url="https://github.com/markhu/sandbox/blob/master/docker-cmake/Dockerfile"

RUN yum install -y 'wget' which yum-utils mlocate cmake
# RUN yum install -y gcc  # CentOS 6 comes with gcc-4.4 --replaced with 4.9 below

# Add a newer GCC and a few libs in order to build Apache modules
# install RPM(s) from EPEL repo
RUN wget http://download.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm && rpm -ivh epel-release-6-8.noarch.rpm

# this scl (Software Collection) contains gcc 4.9
RUN yum install -y centos-release-scl && yum-config-manager --enable rhel-server-rhscl-7-rpms
# RUN yum install -y devtoolset-3  # install ALL which is large
RUN yum install -y 'devtoolset-3-gcc' devtoolset-3-gcc-c++ && mkdir -p /usr/lib/gcc/
RUN ln -s /opt/rh/devtoolset-3/root/usr/lib/gcc/x86_64-redhat-linux    /usr/lib/gcc/
RUN ln -s /opt/rh/devtoolset-3/root/usr/bin/c++ /usr/bin/
RUN ln -s /opt/rh/devtoolset-3/root/usr/bin/* /usr/local/bin/

RUN updatedb

CMD cat /etc/centos-release && rpm -qa --last | head -n22 \
    && gcc --version

# vim: ft=dockerfile

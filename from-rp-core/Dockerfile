# Dockerfile
#  build: docker build . --tag blah  # if you don't tag it, it's harder to...
#    run: docker run blah [optional command]
# to tweak /etc/hosts: docker build . --tag blah --build-arg HOSTS="10.96.16.28 yumcache"

FROM docker.rp-core.com/rp_centos_6.7

ENV container=docker
ARG HOSTS
ENV HOSTS ${HOSTS:-}

# RUN command args  # execute as root
RUN [ -n "${HOSTS}" ] && echo "${HOSTS}" >> /etc/hosts && yum install -y python27-pip

RUN echo -e "#!/bin/bash\n\necho cat /etc/issue" > cat-issue.sh
RUN chmod a+x cat-issue.sh

VOLUME [ "/tmp/docker" ]
CMD rpm -qa --last | head

# setting an ENTRYPOINT makes it harder to specify alternative commands
# ENTRYPOINT ["head /etc/issue"]

# vim: ft=dockerfile

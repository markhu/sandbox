# Dockerfile --build: docker build . --tag blah  # if you don't tag it, it's harder to
# --run:  docker run blah [optional command]
FROM alpine:3.4

ENV container=docker

# RUN command args  # execute as root
# RUN apk add --no-cache git2u
# RUN apk add --update python

RUN echo -e "#!/bin/bash\n\necho cat /etc/issue" > cat-issue.sh
RUN chmod a+x cat-issue.sh

# VOLUME [ "/sys/fs/cgroup" ]
# CMD /sbin/apk info -v
CMD /sbin/apk info -v

# setting an ENTRYPOINT makes it harder to specify alternative commands
# ENTRYPOINT ["apk --version"]

# vim: ft=dockerfile

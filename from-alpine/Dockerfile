# Dockerfile-python
# --build EZ: docker build . --tag blah  # tag with a human-readable name
# --build XTRA: docker build -t a34:python -f Dockerfile-python .
# --run:  docker run -i -p 8003:8000 -t a34:python [optional command]
FROM alpine:3.4

ENV container=docker

# RUN command args  # execute as root
# RUN apk add --no-cache git2u
RUN apk add --update python

WORKDIR  /var/tmp/  # WORKDIR is equivalent to `cd`
RUN cat /etc/issue
RUN date > info.htm && ls -la >> info.htm && python -c 'import sys; print "Python", sys.version' >> info.htm
COPY  Dockerfile Dockerfile.txt

# VOLUME [ "/sys/fs/cgroup" ]
# CMD /sbin/apk info -v
CMD python -m SimpleHTTPServer 8000

# setting an ENTRYPOINT makes it harder to specify alternative commands
# ENTRYPOINT ["apk --version"]

# vim: ft=dockerfile

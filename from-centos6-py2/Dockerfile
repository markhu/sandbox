
# docker build -t acct/python2 .
# docker run -i -p 8008:8000 -t acct/python
# curl http://localhost:8008/

FROM  centos:6.7
RUN  rpm -ivh http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm
RUN  yum install -y python-pip  # &&  pip install --upgrade pip
RUN  pip install requests
RUN  yum install -y mlocate  &&  updatedb

# EXPOSE  80

WORKDIR /var/tmp/  # like `cd`
COPY    Dockerfile  Dockerfile.txt

CMD ["python", "-m", "SimpleHTTPServer", "8000"] 


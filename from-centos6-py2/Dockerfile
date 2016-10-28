
# docker build -t acct/python2 .
# docker run -i -p 8008:8000 -t acct/python
# curl http://localhost:8008/

FROM  centos:6.7
RUN  yum update -y
RUN  yum install -y python-devel python-setuptools
RUN  yum install -y mlocate  &&  updatedb
# RUN  yum install -y python-pip  &&  pip install --upgrade pip
# RUN   pip install requests

# EXPOSE  80

WORKDIR /var/tmp/  # like `cd`
COPY    Dockerfile  Dockerfile.txt

CMD ["python", "-m", "SimpleHTTPServer", "8000"] 


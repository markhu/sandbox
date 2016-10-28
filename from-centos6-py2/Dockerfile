
# docker build -t acct/python-2.7 .
# docker run -i -p 8008:8000 -t python/server
# curl http://localhost:8008/

FROM  centos:6
# RUN   yum install -y python27
# RUN   python easy_install pip

# EXPOSE  80

WORKDIR /var/tmp/  # like `cd`
COPY    Dockerfile  Dockerfile.txt

CMD ["python", "-m", "SimpleHTTPServer", "8000"] 


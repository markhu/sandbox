
FROM debian:wheezy

# RUN apt-get update  # think-twice about bloating Docker layered FS...
# RUN apt-get install -y curl wget

# Install Java and maven
RUN apt-get -y install openjdk-7-jdk maven
RUN ln -s /usr/lib/jvm/java-1.7.0-openjdk-amd64/jre/lib/amd64/server/libjvm.so /usr/lib/libjvm.so

# Default DNS cache TTL is -1. DNS records, like, change, man.
RUN grep '^networkaddress.cache.ttl=' /etc/java-7-openjdk/security/java.security || echo 'networkaddress.cache.ttl=60' >> /etc/java-7-openjdk/security/java.security
Contact GitHub API Training Shop Blog About

# Kestrel ...

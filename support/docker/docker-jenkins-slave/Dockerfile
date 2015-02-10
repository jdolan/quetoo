FROM fedora
MAINTAINER Marcel Wysocki "maci.stgn@gmail.com"

RUN yum -y install dnf; yum clean all
RUN dnf -y upgrade; dnf clean all
RUN dnf -y install git subversion @java @buildsys-build yum-utils openssh-server; dnf clean all

ADD start.sh /start.sh
CMD ["./start.sh"]

FROM fedora
MAINTAINER Marcel Wysocki "maci.stgn@gmail.com"

RUN yum -y upgrade; yum clean all
RUN curl -o /etc/yum.repos.d/jenkins.repo http://pkg.jenkins-ci.org/redhat/jenkins.repo
RUN rpm --import http://pkg.jenkins-ci.org/redhat/jenkins-ci.org.key
RUN yum -y install git subversion @java jenkins; yum clean all

ENV JENKINS_HOME /var/lib/jenkins

ENTRYPOINT ["java", "-jar", "/usr/lib/jenkins/jenkins.war"]
EXPOSE 8080
VOLUME ["/var/lib/jenkins"]
CMD [""]

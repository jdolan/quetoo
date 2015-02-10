#!/bin/sh

curl -O http://maven.jenkins-ci.org/content/repositories/releases/org/jenkins-ci/plugins/swarm-client/1.20/swarm-client-1.20-jar-with-dependencies.jar
java -jar /swarm-client-1.20-jar-with-dependencies.jar -master http://jenkins:8080/ -username ${USERNAME} -password ${PASSWORD} -executors 1 -labels ""$(uname -m)" "$(uname -o)""


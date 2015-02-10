docker run -d --link jenkins:jenkins -v /data/jenkins/ssh:/root/.ssh:ro -e USERNAME=swarm -e PASSWORD= maci/jenkins-slave

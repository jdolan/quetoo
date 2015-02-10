docker stop `docker ps -a|grep jenkins-slave|awk '{print $1}'`
docker rm `docker ps -a|grep jenkins-slave|awk '{print $1}'`

docker rmi maci/jenkins-slave
docker build --rm -t maci/jenkins-slave .

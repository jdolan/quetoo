docker stop jenkins
docker rmi maci/jenkins
docker build --rm -t maci/jenkins .

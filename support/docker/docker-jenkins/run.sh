docker stop jenkins
docker rm jenkins
docker run --name=jenkins --hostname=ci.quetoo.org -d -v /data/jenkins:/var/lib/jenkins maci/jenkins

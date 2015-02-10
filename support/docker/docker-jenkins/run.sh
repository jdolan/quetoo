docker stop jenkins
docker rm jenkins
docker run --name=jenkins --hostname=ci.quake2world.net -d -v /data/jenkins:/var/lib/jenkins maci/jenkins

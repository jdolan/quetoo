docker stop nginx
docker rm nginx
docker run --name=nginx -d -p 80:80 -p 443:443  --link jenkins:jenkins -v /data/nginx/conf:/etc/nginx -v /data/nginx/log:/var/log/nginx nginx

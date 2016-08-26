
echo "Remove exited containers..."
echo 'docker rm -v $(docker ps -a -q -f status=exited)'
docker rm -v $(docker ps -a -q --filter status=exited)

echo "Remove dangling images..."
echo 'docker rmi $(docker images -q -f "dangling=true")'
docker rmi $(docker images -q --filter "dangling=true")


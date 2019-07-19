#!/bin/bash
#
# 这个脚本负责启动 docker-compose、上传假文件、设置 cgroup

cd $(dirname $0)

sudo docker-compose up -d
sudo bash ../../exec/create_cgroups.sh domjudge-run
sleep 2
python upload.py

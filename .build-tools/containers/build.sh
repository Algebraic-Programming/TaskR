#!/usr/bin/env bash


if [[ $# -ne 1 ]]; then
   echo "arch not installed. Please provide the folder to build. Usage: $0 <name>"
   exit 1
fi

folder=${1}
echo "Building $folder for arch $target_arch"
docker build -t "ghcr.io/algebraic-programming/taskr/${folder}:latest" ${folder} 

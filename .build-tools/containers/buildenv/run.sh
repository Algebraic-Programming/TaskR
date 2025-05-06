#!/usr/bin/env bash

if [[ $# -ne 2 ]]; then
 echo "Usage: $0 <name> <arch>"
 exit 1
fi

folder=${1}
arch=${2}

docker run --name taskr --shm-size=1024M --privileged -td "ghcr.io/algebraic-programming/taskr/${folder}:latest" bash 

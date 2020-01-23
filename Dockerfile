# This Dockerfile can be used to build a development environment where far2l AKA the Linux port of FAR v2 can be rebuilt from sources.
# Usage:
# docker build -t far2l-dev:1.0 .
# docker run -ti --rm far2l-dev:1.0 bash
# follow the instructions for rebuilding far2l below - starting with git clone

FROM ubuntu:19.10
RUN apt-get update

# https://askubuntu.com/questions/909277/avoiding-user-interaction-with-tzdata-when-installing-certbot-in-a-docker-contai
# https://stackoverflow.com/questions/44331836/apt-get-install-tzdata-noninteractive
# https://serverfault.com/questions/949991/how-to-install-tzdata-on-a-ubuntu-docker-image

ENV TZ=Europe/Berlin
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get install -y tzdata
RUN apt-get install -y gawk m4 libglib2.0-dev libwxgtk3.0-dev
RUN apt-get install -y libssh-dev libsmbclient-dev libnfs-dev libneon27-dev cmake g++ git

# Now far2l can be rebuilt from sources.
#
# git clone https://github.com/elfmz/far2l
# cd far2l
# mkdir build
# cd build
# cmake -DUSEWX=yes -DCMAKE_BUILD_TYPE=Release ..
# make -j4
# ./install/far2l


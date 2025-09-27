# to build a docker image from this file,
# you must be in the root (NachOS) directory and type something like
# docker build -f docker/Dockerfile .
# If you try to build it from this directory, README.Debian wont be available
#
# a docker image from this file should be available as vdanjean/nachos
# in docker registry. Please, report any problem with it.

FROM debian:bullseye

ADD README.Debian /tmp

RUN apt-get -y update && apt-get -y full-upgrade && apt-get -y install build-essential git vim nano
RUN /tmp/README.Debian --force && rm /tmp/README.Debian
RUN apt-get -y install valgrind gdb libc6-dbg:i386

CMD ["/bin/bash","--login"]

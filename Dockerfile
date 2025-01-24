ARG BASE_IMAGE="ubuntu:22.04"
ARG TARGET_LIST="x86_64-softmmu,i386-softmmu,arm-softmmu,aarch64-softmmu,mips-softmmu,mipsel-softmmu,mips64-softmmu,mips64el-softmmu"

### BASE IMAGE
FROM $BASE_IMAGE AS base
ARG BASE_IMAGE

RUN apt-get update && \
    apt-get install -y build-essential \
                        ninja-build \
                        libglib2.0-0 \
                        libcurl3-gnutls \
                        pkg-config \
                        libglib2.0-dev \
                        python3 \
                        python3-pip \
                        python3-setuptools \
                        python3-wheel \
                        ninja-build \
                        libcurl4-gnutls-dev \
                        curl \
                        wget && \
    pip3 install meson pycparser

ARG PANDA_VERSION="7.1.1-pre.55f4ec731"
RUN wget -O /tmp/pandare.deb \
    https://github.com/panda-re/qemu/releases/download/v${PANDA_VERSION}/pandare_22.04.deb && \
    apt install -yy -f /tmp/pandare.deb

COPY . /panda-ng

RUN mkdir -p /panda-ng/build && \
    cd /panda-ng/ && \
    meson setup build && \
    cd /panda-ng/build && \
    ninja

RUN cd /panda-ng/python/core/ &&  \
    python3 setup.py develop
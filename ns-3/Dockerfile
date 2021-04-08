FROM ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    mercurial \
    git \
    gsl-bin \
    libgslcblas0 \
    libgsl-dev \
    flex \
    bison \
    libfl-dev \
    tcpdump \
    sqlite \
    sqlite3 \
    libsqlite3-dev \
    cmake \
    libc6-dev \
    libc6-dev-i386 \
    wget \
    vim \
    python3 \
    python3-dev \
    python3-setuptools \
    python3-monotonic \
    python-subprocess32 \
    python3-pip \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install cxxfilt

WORKDIR /root

# Fetch NS-3 source
RUN wget http://www.nsnam.org/release/ns-allinone-3.31.tar.bz2 \
    && tar -xf ns-allinone-3.31.tar.bz2 \
    && cd ns-allinone-3.31 \
    && ./build.py --enable-examples --enable-tests \
    && cd ns-3.31 \
    && ./waf install
    
ENV LD_LIBRARY_PATH=/usr/local/lib 

WORKDIR /worker
ENTRYPOINT "/bin/bash"

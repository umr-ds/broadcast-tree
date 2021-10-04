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
    openjdk-17-jdk

RUN pip3 install cxxfilt

WORKDIR /root

ARG NS3_VERSION=3.31

# Fetch and install ns-3
RUN wget http://www.nsnam.org/release/ns-allinone-$NS3_VERSION.tar.bz2 \
    && tar -xf ns-allinone-$NS3_VERSION.tar.bz2 \
    && cd ns-allinone-$NS3_VERSION \
    && ./build.py --enable-examples --enable-tests \
    && cd ns-$NS3_VERSION \
    && ./waf install

# ns-3 does not use default library paths, so we have to add this one
ENV LD_LIBRARY_PATH=/usr/local/lib

COPY eval/old/src /root/eval
WORKDIR /root/eval
RUN javac statistics/Statistics.java

WORKDIR /root/ns-allinone-$NS3_VERSION/ns-$NS3_VERSION

# Install the EE-BTP implementation
COPY btp/ scratch/broadcast/
RUN mv $PWD/scratch/broadcast/EEBTPTag.h src/wifi/model/; \
    mv $PWD/scratch/broadcast/EEBTPTag.cc src/wifi/model/
RUN sed -i "/obj.source =/a 'model/EEBTPTag.cc'," src/wifi/wscript; \
    sed -i "/headers.source = /a 'model/EEBTPTag.h'," src/wifi/wscript

# To build the EE-BTP code, we have to call a dummy run with simple parameters
RUN ./waf --run="broadcast --nWifi=1"

# Add the experiment stuff
COPY experiment/ experiment/
ENTRYPOINT ["experiment/entrypoint.sh"]

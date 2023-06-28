FROM ubuntu:18.04 as build

RUN apt-get update && \
    apt-get install -y \
        binutils-mips-linux-gnu \
        bsdmainutils \
        build-essential \
        libaudiofile-dev \
        libsdl2-dev \
        libusb-1.0 \
        libx11-dev \
        libcapstone-dev \
        pkgconf \
        python3 && \
    apt-get clean

ENV PATH="/sm64/tools:${PATH}"

COPY / /sm64
WORKDIR /sm64

RUN make -C tools

CMD echo 'usage: docker run --rm --mount type=bind,source="$(pwd)",destination=/sm64 sm64 make VERSION=us -j4\n' \
         'see https://github.com/n64decomp/sm64/blob/master/README.md for advanced usage'

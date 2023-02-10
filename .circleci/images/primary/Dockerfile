FROM circleci/buildpack-deps:focal

RUN sudo dpkg --add-architecture i386
RUN sudo apt-get -y -qq update
RUN sudo apt-get -y install \
    gcc-multilib-powerpc-linux-gnu gcc-arm-linux-gnueabi \
    libc6-dev-armel-cross gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
    libc6-dev-ppc64-powerpc-cross zstd gzip coreutils \
    libcurl4-openssl-dev

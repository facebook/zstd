FROM circleci/buildpack-deps@sha256:f6f10c11b7b8ccfd4f4a5b830c3256803604ce61292b60cb22e26b12f62b0e8c

RUN sudo dpkg --add-architecture i386
RUN sudo apt-get -y -qq update
RUN sudo apt-get -y install \
    gcc-multilib-powerpc-linux-gnu gcc-arm-linux-gnueabi \
    libc6-dev-armel-cross gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
    libc6-dev-ppc64-powerpc-cross zstd gzip coreutils \
    libcurl4-openssl-dev

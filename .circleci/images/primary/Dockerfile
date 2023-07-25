FROM circleci/buildpack-deps@sha256:f6f10c11b7b8ccfd4f4a5b830c3256803604ce61292b60cb22e26b12f62b0e8c

RUN sudo dpkg --add-architecture i386
RUN sudo apt-get -y -qq update
RUN sudo apt-get -y install \
    gcc-multilib-powerpc-linux-gnu gcc-arm-linux-gnueabi \
    libc6-dev-armel-cross gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
    libc6-dev-ppc64-powerpc-cross zstd gzip coreutils \
    libcurl4-openssl-dev
# TODO: Use apt to install prebuilt risc-v toolchain & qemu in the future
RUN sudo wget https://github.com/riscv-collab/riscv-gnu-toolchain/releases/download/2023.07.05/riscv64-glibc-ubuntu-20.04-llvm-nightly-2023.07.05-nightly.tar.gz
RUN sudo tar -C /opt -zxvf riscv64-glibc-ubuntu-20.04-llvm-nightly-2023.07.05-nightly.tar.gz
RUN rm riscv64-glibc-ubuntu-20.04-llvm-nightly-2023.07.05-nightly.tar.gz
RUN sudo ln -s -f /opt/riscv/bin/clang-16 /usr/bin/riscv64-unknown-linux-gnu-clang
RUN sudo ln -s -f /opt/riscv/bin/llvm-ar /usr/bin/llvm-ar
RUN sudo ln -s -f /opt/riscv/bin/qemu-riscv64 /usr/bin/qemu-riscv64

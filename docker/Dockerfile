FROM ubuntu:jammy

# Install CMake & deps
RUN apt-get update && apt-get -y install \
    cmake protobuf-compiler \
    clang libc++-dev libc++abi-dev

# Copy files inside
WORKDIR /home
COPY . /home

# Build the project
RUN ./scripts/build.sh
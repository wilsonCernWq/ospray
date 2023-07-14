# Example Command to Run
#   docker build -t instantvnr .
#   xhost +si:localuser:root
#   docker run --runtime=nvidia -ti --rm -e DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -w /instantvnr/build instantvnr

FROM intel/oneapi-basekit:devel-ubuntu20.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update 
RUN apt-get install -y --no-install-recommends \
        build-essential mesa-utils pkg-config \
        libglx0 libglvnd0 libglvnd-dev \
        libgl1 libgl1-mesa-dev \
        libegl1 libegl1-mesa-dev \
        libgles2 libgles2-mesa-dev \
        libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libssl-dev \
        wget git ninja-build imagemagick
# RUN rm -rf /var/lib/apt/lists/*

ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility,graphics
ADD https://raw.githubusercontent.com/NVlabs/nvdiffrec/main/docker/10_nvidia.json \
    /usr/share/glvnd/egl_vendor.d/10_nvidia.json

# Install cmake
RUN wget -qO- "https://cmake.org/files/v3.23/cmake-3.23.2-linux-x86_64.tar.gz" | tar --strip-components=1 -xz -C /usr/local

# Create a superbuild
COPY . /ospray

# Config and build
WORKDIR /ospray

ENTRYPOINT [ "/ospray/release.sh" ]

#!/bin/bash

set -euxo pipefail

sudo apt update
sudo apt install -y \
	cmake \
	build-essential \
	git \
    libxcursor-dev \
	libdrm-dev \
	libgbm-dev \
	libinput-dev \
	libudev-dev \
	libgles2-mesa-dev

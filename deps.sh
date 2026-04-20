#!/bin/bash

set -euxo pipefail

sudo apt update
sudo apt install -y \
	cmake \
	build-essential \
	git \
	libxcursor-dev \
	libx11-dev \
	libxinerama-dev \
	libxrandr-dev \
	libxi-dev \
	libwayland-dev \
	libxkbcommon-dev \
	libdrm-dev \
	libgbm-dev \
	libinput-dev \
	libudev-dev \
	libgles2-mesa-dev \
	cage \
	wlr-randr

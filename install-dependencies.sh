#!/bin/bash

# xscreensaver build dependencies installer
# This script installs all the required packages for building xscreensaver

set -e  # Exit on any error

echo "Installing xscreensaver build dependencies..."

# Update package list
sudo apt update

# Install all required dependencies
sudo apt install -y \
    build-essential \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxss-dev \
    libxt-dev \
    libxmu-dev \
    libxpm-dev \
    libxft-dev \
    libxrender-dev \
    libxfixes-dev \
    libxdamage-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxtst-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    freeglut3-dev \
    libgdk-pixbuf2.0-dev \
    libgtk-3-dev \
    libavutil-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libswresample-dev \
    libxml2-dev \
    libsystemd-dev

echo "All dependencies installed successfully!"
echo "You can now run: ./configure && make"

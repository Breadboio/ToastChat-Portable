FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      xvfb x11-utils x11-apps imagemagick \
      libgl1 libglx-mesa0 libgl1-mesa-dri libglu1-mesa mesa-utils \
      libegl1 libopengl0 \
      libxcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
      libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-sync1 \
      libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 libxkbcommon-x11-0 \
      libfontconfig1 libfreetype6 libdbus-1-3 libglib2.0-0 \
      libsm6 libice6 libxi6 libxrender1 libxrandr2 libxtst6 \
      libpulse0 libasound2t64 libusb-1.0-0 libssl3t64 \
      ca-certificates procps \
    && rm -rf /var/lib/apt/lists/*
ENV DISPLAY=:99
ENV LIBGL_ALWAYS_SOFTWARE=1
ENV GALLIUM_DRIVER=llvmpipe
ENV QT_QPA_PLATFORM=xcb

# Azahar needs these from the host; they are not in its AppImage bundle.
RUN apt-get update && apt-get install -y --no-install-recommends \
      libharfbuzz0b libgraphite2-3 && rm -rf /var/lib/apt/lists/*

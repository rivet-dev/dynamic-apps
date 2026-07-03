# Headless test rig for the interactive window (scripts/run-desktop.sh). Provides a virtual X display
# (Xvfb) for winit to open its window on, plus winit's X11 runtime libs and a screenshot->PNG converter.
# This mirrors the runtime deps a MINIMAL Linux desktop needs to run the window build; a normal
# macOS/Linux desktop already has them.
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    xvfb x11-apps imagemagick xdotool python3 \
    libgl1 libx11-6 libxcb1 libxcb-render0 libxcb-shape0 libxcb-xfixes0 \
    libxkbcommon0 libxkbcommon-x11-0 libwayland-client0 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

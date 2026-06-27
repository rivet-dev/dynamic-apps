FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    xvfb mousepad python3 fonts-dejavu-core x11-apps xdotool \
    && rm -rf /var/lib/apt/lists/*
COPY measure.py /measure.py
COPY run.sh /run.sh
RUN chmod +x /run.sh
CMD ["/run.sh"]

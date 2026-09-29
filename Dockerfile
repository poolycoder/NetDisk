FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        default-libmysqlclient-dev \
        libssl-dev \
        openssl \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/netdisk
COPY Version4.0 /opt/netdisk/Version4.0
COPY docker/entrypoint.sh /opt/netdisk/docker/entrypoint.sh

RUN make -C /opt/netdisk/Version4.0/server \
    && chmod +x /opt/netdisk/docker/entrypoint.sh \
    && mkdir -p /opt/netdisk/Version4.0/workspace /opt/netdisk/tls

EXPOSE 2000

ENTRYPOINT ["/opt/netdisk/docker/entrypoint.sh"]

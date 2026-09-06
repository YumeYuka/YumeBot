# telegram-bot-api 使用官方预编译镜像（Alpine/musl 构建），无需从源码编译 tdlib
FROM aiogram/telegram-bot-api:9.6 AS telegram-bot-api

# Export the Linux Bot API binary and its musl runtime libraries as regular files.
RUN mkdir -p /opt/telegram-bot-api \
 && cp -L /usr/local/bin/telegram-bot-api /opt/telegram-bot-api/telegram-bot-api \
 && cp -L /lib/ld-musl-x86_64.so.1 /opt/telegram-bot-api/ld-musl-x86_64.so.1 \
 && cp -L /lib/libc.musl-x86_64.so.1 /opt/telegram-bot-api/libc.musl-x86_64.so.1 \
 && cp -L /usr/lib/libstdc++.so.6 /opt/telegram-bot-api/libstdc++.so.6 \
 && cp -L /usr/lib/libgcc_s.so.1 /opt/telegram-bot-api/libgcc_s.so.1 \
 && cp -L /usr/lib/libssl.so.3 /opt/telegram-bot-api/libssl.so.3 \
 && cp -L /usr/lib/libcrypto.so.3 /opt/telegram-bot-api/libcrypto.so.3 \
 && cp -L /usr/lib/libz.so.1 /opt/telegram-bot-api/libz.so.1

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    ffmpeg \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/*

# telegram-bot-api 二进制是 musl 链接的：需要 Alpine 的 ld-musl 与 libc.musl，
# 仅安装 Debian 的 musl 包不足以运行该二进制。
COPY --from=telegram-bot-api /opt/telegram-bot-api/ /usr/local/lib/telegram-bot-api/
RUN mkdir -p /lib \
 && cp /usr/local/lib/telegram-bot-api/ld-musl-x86_64.so.1 /lib/ld-musl-x86_64.so.1 \
 && cp /usr/local/lib/telegram-bot-api/libc.musl-x86_64.so.1 /lib/libc.musl-x86_64.so.1 \
 && printf '#!/bin/sh\nexport LD_LIBRARY_PATH=/usr/local/lib/telegram-bot-api${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\nexec /usr/local/lib/telegram-bot-api/telegram-bot-api "$@"\n' > /usr/local/bin/telegram-bot-api \
 && chmod +x /usr/local/bin/telegram-bot-api

WORKDIR /app

COPY yumebot /app/yumebot
COPY docker-entrypoint.sh /app/docker-entrypoint.sh

RUN chmod +x /app/yumebot /app/docker-entrypoint.sh

ENTRYPOINT ["/app/docker-entrypoint.sh"]

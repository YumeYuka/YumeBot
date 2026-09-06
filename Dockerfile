# telegram-bot-api 使用官方预编译镜像（Alpine/musl 构建）
FROM aiogram/telegram-bot-api:9.6 AS telegram-bot-api

RUN mkdir -p /opt/telegram-bot-api \
 && for f in \
        /usr/local/bin/telegram-bot-api \
        /lib/ld-musl-x86_64.so.1 \
        /lib/libc.musl-x86_64.so.1 \
        /usr/lib/libstdc++.so.6 \
        /usr/lib/libgcc_s.so.1 \
        /usr/lib/libssl.so.3 \
        /usr/lib/libcrypto.so.3 \
        /usr/lib/libz.so.1 \
    ; do \
        if [ -e "$f" ]; then cp -L "$f" /opt/telegram-bot-api/; fi; \
    done \
 && test -f /opt/telegram-bot-api/telegram-bot-api \
 && test -f /opt/telegram-bot-api/ld-musl-x86_64.so.1 \
 && if [ ! -f /opt/telegram-bot-api/libc.musl-x86_64.so.1 ]; then \
        cp /opt/telegram-bot-api/ld-musl-x86_64.so.1 /opt/telegram-bot-api/libc.musl-x86_64.so.1; \
    fi

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        ffmpeg \
        libcurl4 \
        libssl3 \
    && rm -rf /var/lib/apt/lists/*

# musl 动态链接器必须落在 ELF INTERP 路径 /lib/ld-musl-x86_64.so.1
COPY --from=telegram-bot-api /opt/telegram-bot-api/ /usr/local/lib/telegram-bot-api/
RUN cp /usr/local/lib/telegram-bot-api/ld-musl-x86_64.so.1 /lib/ld-musl-x86_64.so.1 \
 && cp /usr/local/lib/telegram-bot-api/libc.musl-x86_64.so.1 /lib/libc.musl-x86_64.so.1 \
 && printf '%s\n' \
        '#!/bin/sh' \
        'export LD_LIBRARY_PATH=/usr/local/lib/telegram-bot-api${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}' \
        'exec /usr/local/lib/telegram-bot-api/telegram-bot-api "$@"' \
        > /usr/local/bin/telegram-bot-api \
 && chmod +x /usr/local/bin/telegram-bot-api

WORKDIR /app

COPY yumebot /app/yumebot
COPY lib /app/lib
COPY docker-entrypoint.sh /app/docker-entrypoint.sh

RUN chmod +x /app/yumebot /app/docker-entrypoint.sh \
 && ldd /app/yumebot \
 && if ldd /app/yumebot | grep -q 'not found'; then \
        echo 'yumebot is missing shared libraries' >&2; \
        ldd /app/yumebot >&2; \
        exit 1; \
    fi

ENTRYPOINT ["/app/docker-entrypoint.sh"]

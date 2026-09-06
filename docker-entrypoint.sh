#!/bin/sh
set -eu

if [ ! -f /app/config.conf ]; then
    if [ -z "${BOT_TOKEN:-}" ]; then
        echo "BOT_TOKEN is required when /app/config.conf is not mounted." >&2
        exit 1
    fi

    {
        printf 'bot_token=%s\n' "$BOT_TOKEN"
        if [ -n "${MINI_APP_URL:-}" ]; then
            printf 'mini_app_url=%s\n' "$MINI_APP_URL"
        fi
        if [ -n "${BILIBILI_ADMIN_ID:-}" ]; then
            printf 'bilibili_admin_id=%s\n' "$BILIBILI_ADMIN_ID"
        fi
        if [ -n "${NETEASE_MUSIC_U:-}" ]; then
            printf 'netease_music_u=%s\n' "$NETEASE_MUSIC_U"
        fi
        if [ -n "${TELEGRAM_API_BASE_URL:-}" ]; then
            printf 'telegram_api_base_url=%s\n' "$TELEGRAM_API_BASE_URL"
        fi
        if [ -n "${TELEGRAM_API_ID:-}" ]; then
            printf 'app_id=%s\n' "$TELEGRAM_API_ID"
        fi
        if [ -n "${TELEGRAM_API_HASH:-}" ]; then
            printf 'api_hash=%s\n' "$TELEGRAM_API_HASH"
        fi
    } >/app/config.conf
fi

# 可选：容器内启动本地 telegram-bot-api（上传上限 50MB → 2GB）。
# 需要 TELEGRAM_API_ID / TELEGRAM_API_HASH（https://my.telegram.org/apps 申请）。
if [ -n "${TELEGRAM_API_ID:-}" ] && [ -n "${TELEGRAM_API_HASH:-}" ]; then
    TELEGRAM_API_PORT="${TELEGRAM_API_PORT:-8081}"
    export TELEGRAM_API_BASE_URL="${TELEGRAM_API_BASE_URL:-http://127.0.0.1:${TELEGRAM_API_PORT}}"

    mkdir -p /app/data/telegram-bot-api /tmp/telegram-bot-api-temp
    echo "Starting local telegram-bot-api on port ${TELEGRAM_API_PORT}..."
    telegram-bot-api --local \
        --http-port="${TELEGRAM_API_PORT}" \
        --dir=/app/data/telegram-bot-api \
        --temp-dir=/tmp/telegram-bot-api-temp \
        --log=/app/data/telegram-bot-api.log &
    tg_api_pid=$!

    ready=0
    i=0
    while [ "$i" -lt 30 ]; do
        if ! kill -0 "$tg_api_pid" 2>/dev/null; then
            echo "telegram-bot-api exited during startup; last log lines:" >&2
            tail -20 /app/data/telegram-bot-api.log >&2 || true
            exit 1
        fi
        if curl -s -o /dev/null "http://127.0.0.1:${TELEGRAM_API_PORT}/"; then
            ready=1
            break
        fi
        i=$((i + 1))
        sleep 1
    done
    if [ "$ready" -ne 1 ]; then
        echo "telegram-bot-api did not become ready in 30s" >&2
        exit 1
    fi

    if grep -q '^telegram_api_base_url=' /app/config.conf 2>/dev/null; then
        sed -i "s|^telegram_api_base_url=.*|telegram_api_base_url=${TELEGRAM_API_BASE_URL}|" /app/config.conf
    else
        printf 'telegram_api_base_url=%s\n' "$TELEGRAM_API_BASE_URL" >>/app/config.conf
    fi
    echo "telegram-bot-api is ready at ${TELEGRAM_API_BASE_URL}"
fi

exec /app/yumebot "$@"

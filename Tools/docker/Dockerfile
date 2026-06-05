# ═══════════════════════════════════════════════════════════════════════
#  Prismalux — Dockerfile multi-stage (Ubuntu 24.04)
#
#  Stage 1 (builder): compila la GUI Qt6 da sorgente
#  Stage 2 (runtime): immagine minimale con solo le librerie runtime
#
#  Build:
#    docker compose build
#    oppure: docker build -t prismalux -f DOCKER/Dockerfile .
#
#  Avvio:
#    bash DOCKER/avvia_docker.sh
# ═══════════════════════════════════════════════════════════════════════

# ──────────────────────────────────────────────────────────────────────
#  STAGE 1 — Builder
# ──────────────────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/Rome

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    libgl-dev \
    libglib2.0-dev \
    qt6-base-dev \
    qt6-multimedia-dev \
    qt6-webengine-dev \
    libjs-katex \
    qtkeychain-qt6-dev \
    graphviz \
    poppler-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY gui/ ./gui/

RUN cmake -B build_gui gui/ \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O2" \
    && cmake --build build_gui -j"$(nproc)" \
    && strip --strip-unneeded build_gui/Prismalux_GUI

# ──────────────────────────────────────────────────────────────────────
#  STAGE 2 — Runtime minimale
# ──────────────────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/Rome

RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt6widgets6t64 \
    libqt6network6t64 \
    libqt6gui6t64 \
    libqt6core6t64 \
    libqt6dbus6t64 \
    libqt6sql6t64 \
    libqt6sql6-sqlite \
    libqt6multimedia6 \
    libqt6multimediawidgets6 \
    libqt6printsupport6t64 \
    libqt6concurrent6t64 \
    libqt6webenginewidgets6 \
    libqt6webenginecore6 \
    libqt6webenginecore6-bin \
    libqt6webchannel6 \
    libqt6keychain1 \
    libjs-katex \
    qt6-qpa-plugins \
    fonts-liberation \
    fonts-dejavu-core \
    graphviz \
    poppler-utils \
    python3 \
    python3-pip \
    libgl1 \
    libglib2.0-0 \
    libxkbcommon-x11-0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-xinerama0 \
    libxcb-cursor0 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/build_gui/Prismalux_GUI /usr/local/bin/Prismalux_GUI

# Temi: copiati accanto al binario (applicationDirPath()/themes/ — dove Qt li cerca)
COPY --from=builder /build/build_gui/themes/ /usr/local/bin/themes/
COPY gui/themes/                             /usr/local/bin/themes/

COPY ICONA/prismalux.png /usr/local/share/pixmaps/prismalux.png

COPY DOCKER/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

RUN useradd -ms /bin/bash prismalux

# Quiz CCNA DB (209 domande — cercato in ~/.prismalux/)
COPY DOCKER/quiz_ccna.db /home/prismalux/.prismalux/quiz_ccna.db
RUN chown -R prismalux:prismalux /home/prismalux/.prismalux

USER prismalux
WORKDIR /home/prismalux

ENV QT_QPA_PLATFORM=xcb \
    QT_AUTO_SCREEN_SCALE_FACTOR=1 \
    FONTCONFIG_PATH=/etc/fonts

VOLUME ["/home/prismalux/.prismalux"]

ENTRYPOINT ["/entrypoint.sh"]
CMD ["Prismalux_GUI"]

# ═══════════════════════════════════════════════════════════════════════
#  Prismalux — Dockerfile runtime GUI (Ubuntu 24.04 + Qt 6.4)
#
#  Stage 1 (builder): compila la GUI Qt6 da sorgente.
#  Stage 2 (runtime): immagine con solo le librerie necessarie a eseguirla.
#
#  Build (dalla root del repo):
#    docker build -t prismalux -f Tools/docker/Dockerfile .
#    oppure: docker compose -f Tools/docker/docker-compose.yml build
#
#  Avvio:
#    bash Tools/docker/avvia_docker.sh
#
#  Nota: l'AppImage usa un builder separato:
#    Tools/docker/Dockerfile.appimage
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
    ninja-build \
    pkg-config \
    qt6-base-dev \
    qt6-base-private-dev \
    qt6-svg-dev \
    qt6-multimedia-dev \
    qt6-webengine-dev \
    qtkeychain-qt6-dev \
    libgl-dev \
    libglib2.0-dev \
    libjs-katex \
    graphviz \
    poppler-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Il contesto è la root del repo: copiamo solo ciò che serve alla GUI.
COPY gui/ ./gui/

RUN cmake -S gui -B build_gui -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O2" \
    && cmake --build build_gui -j"$(nproc)" \
    && strip --strip-unneeded build_gui/Prismalux_GUI

# ──────────────────────────────────────────────────────────────────────
#  STAGE 2 — Runtime
# ──────────────────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/Rome

RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt6core6t64 \
    libqt6gui6t64 \
    libqt6widgets6t64 \
    libqt6network6t64 \
    libqt6dbus6t64 \
    libqt6sql6t64 \
    libqt6sql6-sqlite \
    libqt6concurrent6t64 \
    libqt6printsupport6t64 \
    libqt6svg6 \
    libqt6multimedia6 \
    libqt6multimediawidgets6 \
    libqt6webenginewidgets6 \
    libqt6webenginecore6 \
    libqt6webenginecore6-bin \
    libqt6webchannel6 \
    libqt6keychain1 \
    qt6-qpa-plugins \
    qt6-wayland \
    libgl1 \
    libegl1 \
    libgl1-mesa-dri \
    libglib2.0-0 \
    libxkbcommon0 \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-xinerama0 \
    fonts-liberation \
    fonts-dejavu-core \
    fonts-noto-color-emoji \
    graphviz \
    poppler-utils \
    python3 \
    python3-pip \
    python3-venv \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/build_gui/Prismalux_GUI /usr/local/bin/Prismalux_GUI
COPY --from=builder /build/build_gui/themes/     /usr/local/bin/themes/
COPY ICONA/prismalux.png                          /usr/local/share/pixmaps/prismalux.png

RUN useradd -ms /bin/bash prismalux \
    && mkdir -p /home/prismalux/.prismalux

COPY --chown=prismalux:prismalux Tools/docker/entrypoint.sh /entrypoint.sh
COPY --chown=prismalux:prismalux Tools/docker/quiz_ccna.db  /home/prismalux/.prismalux/quiz_ccna.db

RUN chmod +x /entrypoint.sh

USER prismalux
WORKDIR /home/prismalux

ENV QT_QPA_PLATFORM=xcb \
    QT_AUTO_SCREEN_SCALE_FACTOR=1 \
    FONTCONFIG_PATH=/etc/fonts

VOLUME ["/home/prismalux/.prismalux"]

ENTRYPOINT ["/entrypoint.sh"]
CMD ["Prismalux_GUI"]

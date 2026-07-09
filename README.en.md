<div align="center">

<img src="ICONA/prismalux.png" width="96" alt="Prismalux logo"/>

# 🍺 Prismalux

### *"Built for mortals who aspire to wisdom."*

[![Tests](https://github.com/wildlux/Prismalux/actions/workflows/tests.yml/badge.svg)](https://github.com/wildlux/Prismalux/actions/workflows/tests.yml)
[![Version](https://img.shields.io/badge/version-3.0-blue?style=flat-square)](CHANGELOG)
[![C++/Qt6](https://img.shields.io/badge/GUI-C%2B%2B%20%2F%20Qt6-41CD52?style=flat-square&logo=qt)](https://www.qt.io/)
[![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)](LICENSE)

**Local, distributed AI platform — zero cloud, zero subscriptions, everything on your own hardware.**

🇮🇹 **[Documentazione completa in italiano → README.md](README.md)** (primary, most detailed)

[![Donate PayPal](https://img.shields.io/badge/Donate-PayPal-00457C?style=flat-square&logo=paypal&logoColor=white)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=wildlux%40gmail.com&item_name=Prismalux&currency_code=EUR&source=url)

</div>

---

## What is Prismalux

Prismalux is a Qt6/C++ desktop application for people who want to run local AI models (Ollama, llama-server) without depending on cloud services or subscriptions. It is a modular platform that integrates in a single window:

- **Multi-agent pipeline** with a 4-stage logical anti-hallucination protocol (Original → Devil's Advocate → Twin → Judge)
- **Multi-agent orchestration with GraphMemory** — task decomposition, specialized sub-agents, SQLite graph memory (nodes, edges, BFS, DOT export)
- **Hybrid RAG + Knowledge Graph** — semantic search over local documents plus LLM-extracted entities/relations rendered as a navigable graph
- **Zero-LLM instant answers** — dozens of local guards (math, dates, finance, unit conversions, classic algorithms, document validation) answer common questions with zero tokens and zero latency, before the model is even called
- **105 algorithm simulations** visualized step by step with Big-O explanations
- **50+ MCP plugins** (JSON-RPC 2.0 over stdio) for Blender, FreeCAD, GNS3, RDKit, Cytoscape, OBS, Godot, security tooling, and more
- **Security tooling** — dedicated MCPs: secrets scanner, CVE audit, NVD lookup, network recon, SAST
- **Distributed WAN compute** (BOINC-like) over the local network, 28 task types
- **Symbolic math** with SymPy, interactive plotting, LaTeX rendering (KaTeX)
- **Voice**: local TTS (Piper / QTextToSpeech) and STT (faster-whisper, GPU-accelerated, with a warm daemon for ~1.7 s/sentence)
- **3D scanning (Vision3D)** — photogrammetry from your phone's browser (HTTPS + gyroscope), COLMAP reconstruction, colored point-cloud OBJ export
- **Embedded LAN web app** — chat, math, voice and more served to any device on your network (HTTPS, QR-code onboarding)
- **Android app** (native Qt6) with chat, quizzes, TTS/STT and LAN sync

Everything runs locally. The only optional network calls are the ones you explicitly trigger (web search, currency rates, model downloads).

## Quick start

### Linux — AppImage (recommended)

Download the latest AppImage from [Releases](https://github.com/wildlux/Prismalux/releases) (verify it against the attached `SHA256SUMS.txt`), then:

```bash
chmod +x Prismalux-x86_64.AppImage
./Prismalux-x86_64.AppImage
```

Requirement: [Ollama](https://ollama.com) running on `localhost:11434` with at least one model pulled.

### Build from source

Requirements: Qt ≥ 6.4 (Widgets, Network; Sql/Svg/Multimedia/WebEngine optional), CMake, a C++17 compiler.

```bash
git clone https://github.com/wildlux/Prismalux.git
cd Prismalux
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release
cmake --build build_gui -j$(nproc)
./build_gui/Prismalux_GUI
```

### Run the tests

60+ ctest suites (the same ones enforced in CI):

```bash
cmake -B build_tests gui/ -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_tests -j$(nproc)
ctest --test-dir build_tests --output-on-failure \
      --exclude-regex "AiIntegration|AiStress|TeamCollab|MultiAgenteLive|AgenteAutonomoLive|ChatLive"
```

(The excluded suites are live end-to-end tests that require a running Ollama with specific models.)

## Interface at a glance

| Tab | Contents |
|---|---|
| 🤖 AI | 6-agent pipeline · Byzantine majority game · RAG chat · autonomous ReAct agent |
| 🛠 Tools | AI assistant · Italian finance calculators (tax, VAT, severance) · cron · learning & quizzes |
| 🎬 Multimedia | Whisper STT + TTS · Stable Diffusion · concept maps · OSM maps with routing · 3D scan |
| 📁 File AI | File/PDF/CSV/Word analysis · wiki & web |
| 💻 Programming | AI editor · reverse engineering · git · Python REPL · network tools |
| π Math | Sequence→formula · step-by-step solver (52 formulas) · calculus with KaTeX |
| 🔬 Research | arXiv papers · patents · chem/bio tooling · RAG knowledge graph |
| 🕹 App Controller | Blender · FreeCAD · Office · OBS · Godot · KiCAD · dev agent |
| 🌐 LAN & WAN | Android via QR/ADB · GNS3 · distributed WAN compute |
| 🕸️ Multi-Agent | MasterAgent → JSON subtasks → sub-agents → GraphMemory |

Note: the UI language is Italian (English i18n is planned — see `TODO.md`, item OS-10).

## Security

Local-first by design: the LAN/web servers bind to the interface you choose, use self-signed TLS and Bearer-token auth. Release binaries ship with SHA-256 checksums. See [SECURITY.md](SECURITY.md) for how to report vulnerabilities.

## License

MIT — see [LICENSE](LICENSE). If Prismalux is useful to you, consider [donating](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=wildlux%40gmail.com&item_name=Prismalux&currency_code=EUR&source=url).

# univ-echo

A terminal-based personal memory tool with a ncurses TUI. Save memories, import your shell history, search past commands by meaning, and peek at your terminal habits — all stored locally in SQLite.

![Image 1](assets/univ-echo-screenshot.png)

Website: https://univ-echo.vercel.app

## What it does

- **Memories**: save titled notes (title + content), list them, and full-text search them with SQLite FTS5.
- **Command history import**: pull history from zsh, bash, powershell, or cmd into a local database.
- **Semantic command search**: search your imported commands by meaning, not just keywords. It hits a local Ollama instance (`qwen3-embedding:0.6b` over `http://localhost:11434/api/embed`), ranks results with semantic similarity plus lexical scoring, and caches embeddings on disk so repeat searches are instant.
- **Terminal DNA**: analytics on your shell usage — top tools, command categories, most active day, peak hour, and a 24-hour activity heatmap.
- **System monitor**: live CPU, per-core, RAM, swap, uptime, and load average read from `/proc` (Linux only).
- **CLI**: run without arguments to open the TUI, or use subcommands like `add`, `list`, `search`, `commands`, and `import-history`.

## How it works

1. On first run it creates a SQLite database under your data directory (`~/.local/share/univecho/univecho.db` on Linux, `%APPDATA%\univecho\univecho.db` on Windows).
2. `import-history` parses your shell history file and inserts each command into `command_history`, tagged with its shell.
3. In the TUI, press `/` to search memories, `c` to search command history, `t` for Terminal DNA, `s` for the system monitor, `n` to add a memory, and `Enter` to open a memory.
4. Semantic search sends your query and candidate commands to Ollama for embeddings, normalizes vectors, computes cosine similarity, blends it with a lexical keyword score, and returns ranked matches. Results are cached locally so you do not re-embed the same text.
5. The system monitor samples `/proc/stat`, `/proc/meminfo`, `/proc/uptime`, and `/proc/loadavg` once per second.

## What it uses

- Language: C++20
- Build: CMake (3.20+), tested with Ninja
- TUI: ncursesw (Linux) / PDCurses (Windows)
- Storage: SQLite3 with FTS5
- HTTP client: libcurl (talks to Ollama on localhost:11434)
- JSON: nlohmann_json
- Embedding model: `qwen3-embedding:0.6b` via Ollama
- Installer: `install.sh` downloads the latest GitHub release, verifies its sha256 checksum, and installs the `univecho` binary to `~/.local/bin`

## Install

Install it now and give it a try:

```bash
curl -fsSL https://raw.githubusercontent.com/0Armaan025/univ-echo/main/install.sh | bash
```

Then make sure `~/.local/bin` is on your PATH and run:

```bash
univecho
```

## Build from source

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Required dependencies: SQLite3, libcurl, nlohmann_json, and ncursesw (or PDCurses on Windows).

## CLI usage

```
univecho             Open the TUI
univecho tui         Open the TUI
univecho add TITLE TEXT   Save a memory
univecho list        List memories
univecho search QUERY  Search memories
univecho commands QUERY  Search command history
univecho import-history [--shell zsh|bash|powershell|cmd FILE]  Import shell history
univecho help        Show this help
```

## TUI keys

```
[n] New memory   [/] Search/Ask   [c] Commands   [t] Terminal DNA
[s] System Monitor  [r] All memories  [Enter] Open
[Up/Down] Navigate   [q] Quit
```

Thanks for checking out univ-echo ⭐ — if you find it useful, please consider installing it and starring the repository.

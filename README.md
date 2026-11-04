# univ-echo

A terminal-based personal memory tool with a ncurses TUI. Save memories, import your shell history, search past commands by meaning, and peek at your terminal habits — all stored locally in SQLite.

<img width="1256" height="220" alt="image(75)" src="https://github.com/user-attachments/assets/cf19d702-65dd-4baa-9b4e-6f784401dfb1" />
<img width="1245" height="1021" alt="image(68)" src="https://github.com/user-attachments/assets/75940351-2147-4b53-87e7-3a215348196d" />
<img width="1240" height="1017" alt="image(69)" src="https://github.com/user-attachments/assets/0438eee5-c40e-4bed-91bd-3db6c99fb83f" />
<img width="1106" height="881" alt="image(70)" src="https://github.com/user-attachments/assets/4030e14d-8d41-43c0-a926-54def9d80df9" />
<img width="1269" height="156" alt="image(71)" src="https://github.com/user-attachments/assets/a16ce0c2-11f6-4ca9-bc2c-9d2047debbd5" />
<img width="1270" height="1007" alt="image(72)" src="https://github.com/user-attachments/assets/93d5e706-ae23-4029-bab3-afb60f260365" />
<img width="1263" height="1004" alt="image(73)" src="https://github.com/user-attachments/assets/8c8cc407-e2ef-4c7b-8869-38c5cfb645a5" />
<img width="891" height="1003" alt="image(74)" src="https://github.com/user-attachments/assets/fad5bd3e-e0b8-4086-9e07-8dae42faff8f" />


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

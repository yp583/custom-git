# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a collection of custom git commands, organized in a scalable directory structure. The primary command `git aicommit` implements AI-powered git commit analysis.

### Repository Architecture

```
custom-git/
├── commands/           # Individual command implementations
│   └── aicommit/      # AI commit analysis command
│       ├── src/       # C++ source files
│       ├── build/     # Build artifacts
│       ├── CMakeLists.txt
│       └── git-aicommit (wrapper script)
├── scripts/           # Installation and build automation
│   ├── setup.sh      # Complete setup and installation
│   └── build_all.sh  # Build all commands for development
└── .env              # API keys and configuration
```

### AI Commit Analysis (`git aicommit`)

The main tool analyzes git diffs by:
1. Parsing git diff output to extract code insertions and deletions
2. Using tree-sitter to parse code changes into AST chunks
3. Generating embeddings via OpenAI API for each chunk
4. Clustering similar changes using hierarchical clustering

## Build System

**Recommended (full setup):**
```bash
./scripts/setup.sh
```

**Development (build only):**
```bash
./scripts/build_all.sh
```

**Individual command:**
```bash
cd commands/aicommit
mkdir build && cd build && cmake .. && make
```

The CMake build automatically downloads CPM package manager and tree-sitter grammars for Python and C++.

## Core Architecture

**Main components:**
- `main.cpp` - Entry point, orchestrates diff parsing and clustering pipeline
- `ast.cpp/h` - Tree-sitter integration for parsing code into chunks
- `openai_api.cpp/h` - OpenAI embeddings API client
- `hierarchal.cpp/h` - Hierarchical clustering implementation  
- `utils.cpp/h` - Utility functions including cosine similarity
- `https_api.cpp/h` - HTTP client for API requests

**Key dependencies:**
- cpp-tree-sitter - Code parsing via tree-sitter
- OpenSSL (ssl, crypto) - HTTPS requests
- Tree-sitter grammars for Python and C++

## Usage

The main executable expects:
- `.env` file with `OPENAI_API_KEY=your_key_here`
- Git diff input via stdin
- Optional distance threshold argument (default 0.5)

Example:
```bash
git diff HEAD^^^..HEAD | ./build/git_aicommit.o 0.3
```

The `git-aicommit` bash script provides a wrapper that generates diffs and processes them.

## Development Notes

- No package.json, Makefile, or standard test framework detected
- Project uses manual compilation via CMake or shell script
- Code expects .env file in current directory for API key
- Output executable is `git_aicommit.o` (note the .o extension)
# Custom Git Commands

A collection of custom git commands that extend Git's functionality with AI-powered tools and automation.

## Quick Start

1. **Clone the repository:**
   ```bash
   git clone <repository-url>
   cd custom-git
   ```

2. **Run the setup script:**
   ```bash
   ./scripts/setup.sh
   ```

3. **Start using the commands:**
   ```bash
   git aicommit        # AI-powered diff clustering with default threshold
   git aicommit 0.3    # Custom similarity threshold
   ```

## Available Commands

### `git aicommit` - Smart AI Commit Tool
A complete commit workflow tool that stages changes, analyzes them using AI, and creates commits automatically.

**Features:**
- Automatically stages all changes (`git add .`)
- Analyzes staged changes using tree-sitter semantic parsing
- Clusters similar changes using AI embeddings and hierarchical clustering
- Generates commit messages using OpenAI Chat API
- Creates commits with AI-generated messages
- Fallback to simple commit if AI APIs fail

**Usage:**
```bash
git aicommit          # Stage all changes and create smart commits (default threshold 0.5)
git aicommit 0.3      # Use custom similarity threshold (0.0-1.0)
```

**Workflow:**
1. Stages all changes in working directory
2. Extracts and clusters code changes by similarity
3. Generates descriptive commit messages for each cluster
4. Creates commits automatically

**Requirements:**
- OpenAI API key (set in `.env` file: `OPENAI_API_KEY=your_key_here`)
- CMake, OpenSSL

## Repository Structure

```
custom-git/
├── commands/           # Individual command implementations
│   └── aicommit/      # AI commit analysis command
│       ├── src/       # C++ source files
│       ├── build/     # Build artifacts (generated)
│       ├── CMakeLists.txt
│       └── git-aicommit
├── scripts/           # Setup and build scripts
│   ├── setup.sh      # Full installation script
│   └── build_all.sh  # Build all commands (dev)
└── README.md
```

## Development

### Building Commands Locally
```bash
# Build all commands without installing
./scripts/build_all.sh

# Test a command locally
cd commands/aicommit
git diff HEAD^^^..HEAD | ./build/git_aicommit.o 0.5
```

### Adding New Commands

1. Create a new directory under `commands/`:
   ```bash
   mkdir commands/mycommand
   cd commands/mycommand
   ```

2. Add your implementation files and `CMakeLists.txt`

3. Create a `git-mycommand` script that calls your executable

4. Run `./scripts/setup.sh` to build and install all commands

## Environment Setup

Create a `.env` file in the root directory:
```bash
OPENAI_API_KEY=your_openai_api_key_here
```

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
   git mcommit        # Simple AI commit message generation
   git gcommit        # Advanced diff clustering (⚠️ UNSTABLE)
   git gcommit 0.3    # Custom similarity threshold
   ```

## Available Commands

### `git mcommit` - AI Commit Message Generator
A simple and reliable tool that generates commit messages for your staged changes using AI.

**Features:**
- Generates commit messages for staged changes using OpenAI Chat API
- Simple, focused workflow - works only with already staged changes
- Reliable fallback to basic commit if AI fails
- Lightweight and fast

**Usage:**
```bash
git add .           # Stage your changes first
git mcommit         # Generate AI commit message and commit
```

**Workflow:**
1. Stage your changes manually (`git add`)
2. Run `git mcommit` to generate AI commit message
3. Creates a single commit with AI-generated message

### `git gcommit` - Smart Git Commit Tool ⚠️ **IN PROGRESS**
An advanced commit workflow tool that stages changes, analyzes them using AI, and creates multiple commits automatically.

> **⚠️ WARNING**: This command is currently unstable and experimental. Use with caution in production repositories. Consider using `git mcommit` for reliable AI-powered commits.

**Features:**
- Automatically stages all changes (`git add .`)
- Analyzes staged changes using tree-sitter semantic parsing
- Clusters similar changes using AI embeddings and hierarchical clustering
- Generates commit messages using OpenAI Chat API
- Creates multiple commits automatically based on change clusters
- Fallback to simple commit if AI APIs fail

**Usage:**
```bash
git gcommit          # Stage all changes and create smart commits (default threshold 0.5)
git gcommit 0.3      # Use custom similarity threshold (0.0-1.0)
```

**Workflow:**
1. Stages all changes in working directory
2. Extracts and clusters code changes by similarity
3. Generates descriptive commit messages for each cluster
4. Creates multiple commits automatically

**Requirements:**
- OpenAI API key (set in `.env` file: `OPENAI_API_KEY=your_key_here`)
- CMake, OpenSSL

## Repository Structure

```
custom-git/
├── commands/           # Individual command implementations
│   ├── mcommit/       # Simple AI commit message generator
│   │   ├── src/       # C++ source files
│   │   ├── build/     # Build artifacts (generated)
│   │   ├── CMakeLists.txt
│   │   └── git-mcommit
│   └── gcommit/       # Advanced commit clustering (unstable)
│       ├── src/       # C++ source files
│       ├── build/     # Build artifacts (generated)
│       ├── CMakeLists.txt
│       └── git-gcommit
├── shared/            # Shared libraries (AI APIs, utilities)
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

# Test commands locally
cd commands/mcommit
echo "test changes" | ./build/git_mcommit.o

cd ../gcommit
git diff HEAD^^^..HEAD | ./build/git_gcommit.o 0.5
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

```bash
export OPENAI_API_KEY=your_openai_api_key_here
```

## TODOs
- make a message for each cluster
- make a terminal frontend
   - shows UMAP or TSNE of embeddings and clustering by color and a legend for each color and message
   - make an interactive ui to edit the clusters, for each cluster see the message, the diff from this and prev clusters
      - involves making a staging  

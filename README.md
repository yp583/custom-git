# Custom Git Commands

A collection of custom git commands that extend Git's functionality with AI-powered tools and automation.

## Installation

### Homebrew (Recommended)

```bash
brew tap yp583/custom-git
brew install custom-git
```

### Manual Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yp583/custom-git.git
   cd custom-git
   ```

2. **Run the setup script:**
   ```bash
   ./scripts/setup.sh
   ```

## Quick Start

```bash
git mcommit           # Simple AI commit message generation
git gcommit           # Smart commit clustering with interactive UI
git gcommit -d 0.3    # Custom similarity threshold
```

## Requirements

Set the `OPENAI_API_KEY` environment variable:

```bash
export OPENAI_API_KEY=your_openai_api_key_here
```

## Available Commands

### `git mcommit` - AI Commit Message Generator

Generates a single commit message for staged changes using AI.

**Usage:**
```bash
git add .              # Stage your changes first
git mcommit            # Generate AI commit message and commit
git mcommit -i         # Edit message in vim before committing
git mcommit -h         # Show help
```

**Workflow:**
1. Stage changes with `git add`
2. Run `git mcommit` - AI generates commit message
3. Review and confirm (y/n)
4. Commit is created

### `git gcommit` - Smart Commit Clustering

Clusters staged changes by semantic similarity and creates separate commits for each cluster, with an interactive terminal UI to review before applying.

**Features:**
- Tree-sitter AST parsing for semantic code analysis
- OpenAI embeddings + hierarchical clustering
- Interactive terminal UI with diff viewer and scatter plot visualization
- Review and navigate commits before applying

**Usage:**
```bash
git gcommit              # Default threshold (0.5)
git gcommit -d 0.3       # Lower threshold = more granular clusters
git gcommit -v           # Verbose output
git gcommit -h           # Show help
```

**Interactive Controls:**
| Key | Action |
|-----|--------|
| `TAB` | Switch between file tree and diff panel |
| `j/k` | Navigate files / scroll diff |
| `Shift+H/L` | Navigate between commits |
| `v` | Toggle scatter plot / diff view |
| `a` | Apply all commits |
| `q` | Cancel and quit |

**Workflow:**
1. Stage changes with `git add`
2. Run `git gcommit` - AI clusters and generates messages
3. Review commits in interactive UI
4. Press `a` to apply or `q` to cancel

**Supported Languages:** Python, C++, Java, JavaScript, Go

## Repository Structure

```
custom-git/
├── commands/
│   ├── mcommit/           # Simple AI commit
│   │   ├── src/           # C++ source
│   │   └── git-mcommit    # Bash wrapper
│   └── gcommit/           # Smart commit clustering
│       ├── src/           # C++ clustering engine
│       └── terminal-ui/   # Node.js interactive UI (Ink)
├── shared/                # Shared C++ libraries (OpenAI API, tree-sitter)
├── scripts/
│   ├── setup.sh           # Build + install
│   └── build_all.sh       # Build only
└── Formula/               # Homebrew formula
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

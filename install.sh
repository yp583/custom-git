#!/bin/bash

# Git AICommit Installation Script
# Installs the git aicommit command to ~/bin

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}Git AICommit Installation Script${NC}"
echo

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Build the project first
echo -e "${YELLOW}Building git aicommit...${NC}"
cd "$PROJECT_ROOT/commands/aicommit"

if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake .. && make -j4

if [ ! -f "git_aicommit.o" ]; then
    echo -e "${RED} Build failed: executable not found${NC}"
    exit 1
fi

echo -e "${GREEN} Build successful!${NC}"

# Create ~/bin directory if it doesn't exist
BIN_DIR="$HOME/bin"
if [ ! -d "$BIN_DIR" ]; then
    echo -e "${YELLOW} Creating ~/bin directory...${NC}"
    mkdir -p "$BIN_DIR"
fi

# Copy the executable to ~/bin
echo -e "${YELLOW}Installing executable to ~/bin...${NC}"
cp "$PROJECT_ROOT/commands/aicommit/build/git_aicommit.o" "$BIN_DIR/"

# Create the git-aicommit wrapper script
echo -e "${YELLOW}Creating git-aicommit wrapper script...${NC}"
cat > "$BIN_DIR/git-aicommit" << 'EOF'
#!/bin/bash

# Smart AI Commit Tool
# Stages changes, clusters them, generates commit messages, and creates commits

set -e  # Exit on any error

# Default distance threshold
DISTANCE_THRESHOLD=0.5

# Get distance threshold from command line argument
if [ ! -z "$1" ]; then
  DISTANCE_THRESHOLD=$1
fi

# Path to the executable
EXECUTABLE="$HOME/bin/git_aicommit.o"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
  echo "Error: git_aicommit.o not found at $EXECUTABLE"
  echo "Please run the installation script again."
  exit 1
fi

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
  echo "Error: Not in a git repository"
  exit 1
fi

# Check if jq is available for JSON parsing
if ! command -v jq &> /dev/null; then
  echo "Error: jq is required for JSON parsing. Please install jq:"
  echo "brew install jq"
  exit 1
fi

echo "Staging all changes..."
git add .

# Check if there are staged changes
if git diff --cached --quiet; then
  echo "No staged changes to commit."
  exit 0
fi

echo "Analyzing staged changes..."

# Generate git diff for staged changes and pipe to the clustering tool
JSON_OUTPUT=$(git diff --cached | "$EXECUTABLE" "$DISTANCE_THRESHOLD" 2>&1)

# Extract just the JSON part (last line that starts with [)
JSON_ONLY=$(echo "$JSON_OUTPUT" | grep '^\[' | tail -1)

# Check if we got valid JSON
if [ -z "$JSON_ONLY" ] || ! echo "$JSON_ONLY" | jq empty 2>/dev/null; then
  echo "Warning: AI commit message generation failed, using fallback"
  git commit -m "update code"
  echo "Created fallback commit"
  exit 0
fi

JSON_OUTPUT="$JSON_ONLY"

# Parse JSON output and create commits
echo "Generating commits..."

# Get number of clusters
CLUSTER_COUNT=$(echo "$JSON_OUTPUT" | jq length)

if [ "$CLUSTER_COUNT" -eq 0 ]; then
  echo "No clusters generated. Creating single commit..."
  COMMIT_MSG="update code"
  git commit -m "$COMMIT_MSG"
  echo "Created commit: $COMMIT_MSG"
  exit 0
fi

echo "Found $CLUSTER_COUNT clusters"

# For each cluster, create a commit
for i in $(seq 0 $((CLUSTER_COUNT - 1))); do
  COMMIT_MESSAGE=$(echo "$JSON_OUTPUT" | jq -r ".[$i].commit_message")
  
  echo "Cluster $((i + 1)): $COMMIT_MESSAGE"
  
  # Create temp files to stage specific changes for this cluster
  TEMP_PATCH="/tmp/git_aicommit_cluster_$i.patch"
  
  # Extract changes for this cluster and create a patch
  echo "$JSON_OUTPUT" | jq -r ".[$i].changes[] | 
    if .type == \"insertion\" then 
      \"+\" + .code 
    else 
      \"-\" + .code 
    end" > "$TEMP_PATCH"
  
  # For now, we'll commit all staged changes with the first cluster's message
  # In a more sophisticated version, we'd selectively stage changes per cluster
  if [ $i -eq 0 ]; then
    git commit -m "$COMMIT_MESSAGE"
    echo "Created commit: $COMMIT_MESSAGE"
    break
  fi
done

# Clean up temp files
rm -f /tmp/git_aicommit_cluster_*.patch

echo "Smart commit complete!"
EOF

# Make the wrapper script executable
chmod +x "$BIN_DIR/git-aicommit"

# Check if ~/bin is in PATH
if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
    echo
    echo -e "${YELLOW}WARNING: ~/bin is not in your PATH${NC}"
    echo -e "${BLUE}Add this line to your shell configuration file:${NC}"
    echo -e "${GREEN}export PATH=\"\$HOME/bin:\$PATH\"${NC}"
    echo
    echo -e "${BLUE}For bash: add to ~/.bashrc or ~/.bash_profile${NC}"
    echo -e "${BLUE}For zsh: add to ~/.zshrc${NC}"
    echo
fi

echo -e "${GREEN}Installation complete!${NC}"
echo
echo -e "${BLUE}Usage:${NC}"
echo "  git aicommit                 # Use default threshold (0.5)"
echo "  git aicommit 0.3             # Use custom threshold"
echo
echo -e "${BLUE}Environment Variable Support:${NC}"
echo "  You can now use OPENAI_API_KEY environment variable:"
echo "  export OPENAI_API_KEY=\"your_key_here\""
echo "  git aicommit"
echo
echo -e "${BLUE}Files installed:${NC}"
echo "  $BIN_DIR/git_aicommit.o      # Main executable"
echo "  $BIN_DIR/git-aicommit        # Git command wrapper"
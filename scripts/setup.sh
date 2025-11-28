#!/bin/bash

# Custom Git Commands Setup Script
# This script builds and installs all custom git commands from this repository

set -e  # Exit on any error

echo "Setting up Custom Git Commands..."

# Get the root directory of the repo
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$HOME/bin"

# Create ~/bin directory if it doesn't exist
if [ ! -d "$BIN_DIR" ]; then
    echo "Creating $BIN_DIR directory..."
    mkdir -p "$BIN_DIR"
fi

# Function to build a command
build_command() {
    local cmd_name=$1
    local cmd_dir="$REPO_ROOT/commands/$cmd_name"

    echo "Building $cmd_name..."

    if [ ! -d "$cmd_dir" ]; then
        echo "ERROR: Command directory $cmd_dir not found"
        return 1
    fi

    cd "$cmd_dir"

    # Create build directory and build C++ binary
    mkdir -p build
    cd build
    cmake .. || { echo "ERROR: CMake configuration failed for $cmd_name"; return 1; }
    make || { echo "ERROR: Build failed for $cmd_name"; return 1; }

    # Copy C++ executable to ~/bin
    if [ -f "git_${cmd_name}.o" ]; then
        echo "Installing git_${cmd_name}.o to $BIN_DIR..."
        cp "git_${cmd_name}.o" "$BIN_DIR/"
        chmod +x "$BIN_DIR/git_${cmd_name}.o"
    else
        echo "ERROR: Executable git_${cmd_name}.o not found"
        return 1
    fi

    # Check if terminal-ui exists (Node CLI is the entrypoint)
    if [ -d "../terminal-ui" ]; then
        echo "Building terminal-ui (Node CLI entrypoint) for $cmd_name..."
        cd ../terminal-ui

        # Install dependencies
        npm install || { echo "ERROR: npm install failed for terminal-ui"; return 1; }

        # Build the CLI
        npm run build || { echo "ERROR: terminal-ui build failed"; return 1; }

        # Copy dist/cli.js as git-gcommit (self-contained bundle)
        if [ -f "dist/cli.js" ]; then
            echo "Installing git-$cmd_name (Node CLI) to $BIN_DIR..."
            cp "dist/cli.js" "$BIN_DIR/git-$cmd_name"
            chmod +x "$BIN_DIR/git-$cmd_name"
        else
            echo "ERROR: dist/cli.js not found"
            return 1
        fi
        cd ../build
    else
        # Fallback: copy bash script if no terminal-ui (for other commands like mcommit)
        if [ -f "../git-$cmd_name" ]; then
            echo "Installing git-$cmd_name to $BIN_DIR..."
            cp "../git-$cmd_name" "$BIN_DIR/"
            chmod +x "$BIN_DIR/git-$cmd_name"
        else
            echo "ERROR: Script git-$cmd_name not found"
            return 1
        fi
    fi

    echo "SUCCESS: $cmd_name installed successfully"
}

# Build all commands in the commands directory
echo "Discovering commands..."
for cmd_dir in "$REPO_ROOT/commands"/*; do
    if [ -d "$cmd_dir" ]; then
        cmd_name=$(basename "$cmd_dir")
        build_command "$cmd_name"
    fi
done

# Add ~/bin to PATH if not already there
add_to_path() {
    local shell_config=""
    
    # Determine shell config file
    if [ "$SHELL" = "/bin/zsh" ] || [ "$SHELL" = "/usr/bin/zsh" ]; then
        shell_config="$HOME/.zshrc"
    elif [ "$SHELL" = "/bin/bash" ] || [ "$SHELL" = "/usr/bin/bash" ]; then
        shell_config="$HOME/.bashrc"
        # On macOS, bash might use .bash_profile
        if [[ "$OSTYPE" == "darwin"* ]] && [ -f "$HOME/.bash_profile" ]; then
            shell_config="$HOME/.bash_profile"
        fi
    else
        echo "WARNING: Unknown shell: $SHELL"
        echo "Please manually add $BIN_DIR to your PATH"
        return 1
    fi
    
    # Check if PATH already contains ~/bin
    if echo "$PATH" | grep -q "$BIN_DIR"; then
        echo "SUCCESS: $BIN_DIR is already in your PATH"
        return 0
    fi
    
    # Add to shell config
    echo "Adding $BIN_DIR to PATH in $shell_config..."
    echo "" >> "$shell_config"
    echo "# Added by custom-git setup script" >> "$shell_config"
    echo "export PATH=\"\$HOME/bin:\$PATH\"" >> "$shell_config"
    
    echo "SUCCESS: PATH updated. Please restart your terminal or run:"
    echo "   source $shell_config"
}

add_to_path

echo ""
echo "Setup complete!"
echo ""
echo "Available commands:"
for cmd_dir in "$REPO_ROOT/commands"/*; do
    if [ -d "$cmd_dir" ]; then
        cmd_name=$(basename "$cmd_dir")
        echo "  git $cmd_name"
    fi
done
echo ""
echo "Usage example:"
echo "  git gcommit              # Use default threshold (0.5)"
echo "  git gcommit -d 0.3       # Use custom threshold"
echo "  git gcommit -v           # Verbose output"
echo ""
echo "Note: You may need to restart your terminal or run 'source ~/.zshrc' (or ~/.bashrc)"
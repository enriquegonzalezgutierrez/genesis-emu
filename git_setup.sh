#!/bin/bash

# ==============================================================================
# genesis-emu - Automated Git Initialization & GitHub Push Script
# ==============================================================================
# This script configures your developer identity, initializes the Git repo,
# commits all architectural files, links it securely to your GitHub repository
# via SSH, and performs the initial push to the 'main' branch.
# ==============================================================================

# Stop the script if any command fails
set -e

echo "===================================================================="
echo " genesis-emu - Automated Git Setup & GitHub Push"
echo "===================================================================="

# 1. Configure local developer identity
echo "[Git] Configuring developer identity..."
git config --global user.name "Enrique González Gutiérrez"
git config --global user.email "enrique.gonzalez.gutierrez@gmail.com"

# 2. Initialize local Git repository
if [ ! -d ".git" ]; then
    echo "[Git] Initializing local Git repository..."
    git init
else
    echo "[Git] Git repository already initialized."
fi

# 3. Add all project files (will automatically respect /.gitignore)
echo "[Git] Staging files..."
git add .

# 4. Perform the initial commit
echo "[Git] Performing initial commit..."
git commit -m "feat: Initial commit. Core Architecture (M68k, VDP, MainBus), Dockerized build system and interactive SDL2 video loop."

# 5. Set default branch to main
echo "[Git] Setting default branch to 'main'..."
git branch -M main

# 6. Configure SSH Remote origin
echo "[Git] Linking local repository to GitHub (SSH)..."
# Safely remove 'origin' if it already exists to avoid conflicts
git remote remove origin 2>/dev/null || true
git remote add origin git@github.com:enriquegonzalezgutierrez/genesis-emu.git

# 7. Push local commits to GitHub
echo "[Git] Pushing code to GitHub (main branch)..."
git push -u origin main

echo "===================================================================="
echo " SUCCESS! Your project is now live on GitHub!"
echo "===================================================================="
echo "Repo URL: https://github.com/enriquegonzalezgutierrez/genesis-emu"
echo "===================================================================="
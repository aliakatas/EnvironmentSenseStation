#!/bin/bash

# Set the full path to your project directory
PROJECT_DIR="~/Github/EnvironmentSenseStation"        # Change this to your actual path
VENV_DIR="$PROJECT_DIR/venv"                          # Adjust if your venv has a different name
SCRIPT_PATH="$PROJECT_DIR/server/main.py"
LOG_PATH="$PROJECT_DIR/capture.log"           # Change this to your desired log file path

# Activate virtual environment
source "$VENV_DIR/bin/activate"

# Change to project directory
cd "$PROJECT_DIR"

# Run the Python script
python3 "$SCRIPT_PATH"

# Log the execution (optional but recommended)
echo "$(date): Data collected" >> "$LOG_PATH"

# Deactivate virtual environment
deactivate
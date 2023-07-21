#!/usr/bin/env bash

function find_python_command() {
    # This function is responsible for identifying the python command that is available on the system.
    # It first checks if 'python' command is available.
    # If not, it checks if 'python3' command is available.
    # If neither of the commands are available, it exits with an error message and a non-zero status code.

    # Check if 'python' command is available
    if command -v python &>/dev/null; then
        echo "python"
    elif command -v python3 &>/dev/null; then
        # 'python' not found, check if 'python3' is available
        echo "python3"
    else
        # Neither 'python' nor 'python3' found, print error message and exit with status 1
        echo "Error: Python not found. Please install Python." >&2
        exit 1
    fi
}

# Store the result of the find_python_command function in the PYTHON_CMD variable
PYTHON_CMD=$(find_python_command)

# Check the requirements for running the script
$PYTHON_CMD scripts/check_requirements.py requirements.txt
if [ $? -eq 1 ]; then
    echo "Installing missing packages..."
    $PYTHON_CMD -m pip install -r requirements.txt
fi

# Pass command line arguments to the euxis python module and execute it
$PYTHON_CMD -m euxis "$@"

# Prompt the user to press any key to continue
read -r -p "Press any key to continue..."

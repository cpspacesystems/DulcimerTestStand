#!/bin/bash
source venv/bin/activate

# set environment variables
. set-env.sh

# Check if a parameter is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <config-file>"
    exit 1
fi

CONFIG_FILE=$1

# Check if the file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found!"
    exit 1
fi

echo "Using config file: $CONFIG_FILE"

# run all sub build scripts
find . -name 'build_*.sh' -type f -perm +111 | while read -r script; do
    echo "Running $script"
    ./"$script"
done


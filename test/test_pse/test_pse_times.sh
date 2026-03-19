#!/bin/bash

# Check if arguments are provided
if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <number_of_times> <command_to_run> [args_for_command...]"
    exit 1
fi

# Store the number of iterations
ITERATIONS=$1
# Shift the arguments so that $@ now contains only the command and its arguments
shift

# The command and its arguments are now stored in $@
COMMAND_TO_RUN="$@"

echo "Running command '$COMMAND_TO_RUN' $ITERATIONS times..."

# Use a 'for' loop to iterate from 1 up to the specified number
for i in $(seq 1 $ITERATIONS); do
    echo "--- Iteration $i of $ITERATIONS ---. press Enter to quit."
    read -t 0 input
    if [ $? -eq 0 ]; then
        echo "Quit command received. Exiting."
        break
    fi
    # Execute the command
    $COMMAND_TO_RUN > /dev/null 2>&1

    # Capture the exit code of the command immediately after it runs
    EXIT_CODE=$?

    # Check the exit code
    if [ $EXIT_CODE -eq 0 ]; then
      echo "Command succeeded (exit code 0)"
    else
      echo "Command failed with exit code $EXIT_CODE"
    # Optional: break the loop immediately on failure
    break
  fi

done

echo "Loop finished."

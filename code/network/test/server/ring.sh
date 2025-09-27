#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <number_of_machines>"
  exit 1
fi

# Read the number of machines
n=$1

# Validate the input
if ! [[ $n =~ ^[0-9]+$ ]] || [ "$n" -le 0 ]; then
  echo "Error: Please provide a positive integer as the number of machines."
  exit 1
fi

# Loop to launch the machines in a ring
for (( i=1; i<n; i++ )); do
  target=$(( (i+1) % n ))
  echo "Launching: ./nachos-step6 -m $i -ring $target"
  ../build/nachos-step6 -m $i -ring $target &
done

target=$(( 1 % n ))
echo "Launching: ./nachos-step6 -m 0 -ring $target"
../build/nachos-step6 -m 0 -ring $target
# Inform the user
echo "All $n machines launched in a ring topology."

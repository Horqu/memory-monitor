#!/usr/bin/env bash

# Kompilacja biblioteki i programów testowych (przykład)
gcc -shared -fPIC src/memory_monitor.c -o src/libmemory_monitor.so -ldl
gcc tests/test_allocations.c -o tests/test_allocations
gcc tests/test_mmap.c -o tests/test_mmap
gcc tests/test_shm.c -o tests/test_shm

# Lista testów do uruchomienia
TESTS=("test_allocations" "test_mmap" "test_shm")

for TEST in "${TESTS[@]}"; do
  echo "Uruchamianie $TEST z memory_monitor..."
  LD_PRELOAD=./src/libmemory_monitor.so ./tests/"$TEST" > "monitor_${TEST}.out" 2>&1

  echo "Uruchamianie $TEST ze strace..."
  strace -o "strace_${TEST}.txt" ./tests/"$TEST"
  
  echo "=== Zakończone $TEST ==="
  echo "Zapisano: monitor_${TEST}.out i strace_${TEST}.txt"
  echo
done

echo "Możesz teraz porównać dane (mallinfo) z plików monitor_*.out z logami wywołań systemowych w strace_*.txt."
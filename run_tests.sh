#!/usr/bin/env bash

# Ścieżka do bibliotek monitorujących pamięć
MONITOR_LIB="src/libmemory_monitor.so"
HELLO_LIB="src/libhello.so"

# Kompilacja bibliotek i programów testowych z obsługą błędów
gcc -shared -fPIC src/memory_monitor.c -o src/libmemory_monitor.so -ldl -pthread -g || { echo "Kompilacja libmemory_monitor.so nie powiodła się"; exit 1; }
gcc -shared -fPIC src/libhello.c -o src/libhello.so -ldl -pthread -g || { echo "Kompilacja libhello.so nie powiodła się"; exit 1; }
gcc tests/test_allocations.c -o tests/test_allocations || { echo "Kompilacja test_allocations nie powiodła się"; exit 1; }
gcc tests/test_mmap.c -o tests/test_mmap || { echo "Kompilacja test_mmap nie powiodła się"; exit 1; }
gcc tests/test_shm.c -o tests/test_shm || { echo "Kompilacja test_shm nie powiodła się"; exit 1; }
gcc tests/test_library_load.c -o tests/test_library_load -ldl || { echo "Kompilacja test_library_load nie powiodła się"; exit 1; }

# Sprawdzenie istnienia bibliotek monitorujących
if [ ! -f "$MONITOR_LIB" ] || [ ! -f "$HELLO_LIB" ]; then
  echo "Błąd: Biblioteki monitorujące nie zostały znalezione."
  exit 1
fi

# Ustawienie ścieżki do bibliotek
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:src"

# Usunięcie poprzednich wyników
rm -f monitor_*.out strace_*.txt

# Lista testów do uruchomienia
TESTS=("test_allocations" "test_mmap" "test_shm" "test_library_load")

for TEST in "${TESTS[@]}"; do
  echo "Uruchamianie $TEST z memory_monitor..."
  LD_PRELOAD="$MONITOR_LIB" ./tests/"$TEST" > "monitor_${TEST}.out" 2>&1
  if [ $? -ne 0 ]; then
    echo "Test $TEST z memory_monitor zakończył się błędem."
  fi

  echo "Uruchamianie $TEST ze strace..."
  strace -o "strace_${TEST}.txt" ./tests/"$TEST"
  if [ $? -ne 0 ]; then
    echo "Test $TEST ze strace zakończył się błędem."
  fi

  echo "=== Zakończone $TEST ==="
  echo "Zapisano: monitor_${TEST}.out i strace_${TEST}.txt"
  echo
done

echo "Można teraz porównać dane (mallinfo) z plików monitor_*.out z logami wywołań systemowych w strace_*.txt."
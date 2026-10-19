# Test Results

Executed in the project workspace using GCC and GNU Make.

Commands:

```bash
make clean
make
make test
```

Observed result:

```text
All automated smoke tests passed.
```

The compiler completed all source files with `-std=c11 -Wall -Wextra -Wpedantic -O2 -pthread` and reported no warnings. Automated smoke tests verified both synchronization strategies and the process, IPC, memory, and file-I/O demonstrations.

For a fresh Ubuntu machine, rerun the commands above to reproduce the build and test results.

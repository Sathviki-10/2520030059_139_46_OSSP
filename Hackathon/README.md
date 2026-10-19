# Dining Philosophers: Deadlock Prevention & OS Concepts Lab

A terminal-only C project for Ubuntu/Linux. The main simulation models philosophers sharing adjacent forks and compares two deadlock-prevention strategies. The default **fair FIFO waiter** coordinates fork acquisition, prevents circular wait, and queues requests by ticket so newer requests cannot repeatedly bypass older requests. The alternative **resource-ordering** strategy imposes a global order on fork locks to break circular wait; it does not promise FIFO fairness.

This is an educational, controlled simulation. It does not alter kernel settings, kill unrelated processes, or require root privileges.

## Requirements

- Ubuntu or another POSIX/Linux environment
- GCC and GNU Make
- POSIX threads (provided by normal Linux toolchains)

Install build tools if needed:

```bash
sudo apt update
sudo apt install build-essential
```

## Build and run

From the project directory:

```bash
make
./dining_os_lab help
./dining_os_lab simulate --philosophers 5 --meals 3
```

Useful examples:

```bash
# 5 philosophers, each must eat 4 times, FIFO waiter strategy
./dining_os_lab simulate --philosophers 5 --meals 4

# Compare resource ordering; logs go to a separate file
./dining_os_lab simulate --philosophers 7 --meals 3 --strategy ordered --log ordered_run.txt

# Run until interrupted with Ctrl+C; shutdown is handled safely
./dining_os_lab simulate --meals 0

# Print only the summary (the event log is still written)
./dining_os_lab simulate --philosophers 5 --meals 2 --quiet

# Run focused demonstrations of related OS concepts
./dining_os_lab process
./dining_os_lab ipc
./dining_os_lab memory
./dining_os_lab files
```

`--philosophers` accepts 2–20. `--meals` accepts 0 or a positive integer; zero runs until Ctrl+C. Simulation output is appended to `dining_log.txt` by default. Use `--log PATH` to select another file. The `--quiet` option suppresses live event lines but retains the summary and file log.

You can also use the wrapper:

```bash
./run.sh simulate --philosophers 5 --meals 3
```

## Test

```bash
make test
```

The smoke tests build and run both synchronization strategies and check process control, IPC, memory and file demonstrations. They validate normal completion and expected output; they are not a formal proof of correctness.

## Project structure

```text
.
├── src/
│   ├── main.c             CLI dispatch and help
│   ├── philosophers.c     Dining Philosophers simulation and synchronization
│   ├── process_demo.c     fork(), exec(), waitpid()
│   ├── ipc_demo.c         anonymous pipe, FIFO, SIGUSR1 handler
│   ├── memory_demo.c      malloc(), page-size and getrusage demonstration
│   ├── file_demo.c        open/write/close and buffered stdio
│   └── oslab.h            shared declarations
├── tests/run_tests.sh
├── docs/PROJECT_REPORT.md
├── Makefile
├── run.sh
└── README.md
```

## CO mapping

| Course outcome | Project implementation |
|---|---|
| CO-1: OS service layer and system software | CLI/shell launch, user-space C program, PID display, system calls used in the demos |
| CO-2: Processes and control | `process`: `fork`, `exec`, `waitpid`; simulation thread lifecycle and orderly joins |
| CO-3: IPC and signals | `ipc`: anonymous pipe, named FIFO, `SIGUSR1` signal handler; Ctrl+C handling in simulation |
| CO-4: Memory management | `memory`: dynamic allocation, page-size query, demand-touching allocated pages, `getrusage` page-fault statistics |
| CO-5: File systems and I/O | `files`: file descriptors with `open/write/close`, buffered stdio, simulation event log |
| CO-6: Concurrency and synchronization | POSIX threads, mutexes, condition variables, race-free shared counters, deadlock prevention and starvation mitigation |

Only topics that reinforce the dining-philosophers problem or demonstrate closely related Linux APIs are included. ext4 internals, page-table inspection and kernel modifications are intentionally excluded because they are not needed for a safe, reproducible user-space prototype.

## Design notes

### FIFO waiter strategy (default)

Each philosopher receives a monotonically increasing ticket when hungry. Only the ticket at the head may attempt to acquire both adjacent fork mutexes. The waiter claims both forks while holding the coordination mutex, so another philosopher cannot interleave a partial grant. When a philosopher finishes eating, both forks are released and waiting threads are notified. This prevents circular wait and mitigates starvation by preventing later requests from bypassing earlier tickets. As with all thread scheduling, OS scheduling delays can affect wall-clock wait time.

### Resource ordering strategy

Each philosopher locks the lower-numbered fork first and then the higher-numbered fork. A global strict order prevents a cycle in the resource wait graph, so deadlock cannot occur under this lock protocol. This strategy is included for comparison and does not guarantee fairness.

## Limitations

- This is a simulation of synchronization concepts, not a real dining-room controller.
- Runtime scheduling and timing vary between machines.
- The fairness guarantee is about queue admission order, not equal elapsed waiting time.
- `ru_maxrss` units are platform-specific, as reported in the output.
- No UI, network service, external database, elevated permissions or real payment/identity data are required.

## GitHub

Create a new, separate repository for the hackathon. From this project directory:

```bash
git init
git add .
git commit -m "Add Dining Philosophers OS lab prototype"
git branch -M main
git remote add origin https://github.com/<your-username>/<new-repository>.git
git push -u origin main
```

Replace the placeholder URL with the repository you create on GitHub. Do not commit private credentials or unrelated course/project files.

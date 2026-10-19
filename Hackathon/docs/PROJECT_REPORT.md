# Project Report: Dining Philosophers Deadlock Prevention System

## 1. Abstract

The Dining Philosophers problem is a classic concurrency problem in which multiple threads compete for shared resources. A philosopher needs two adjacent forks to eat. If every philosopher holds one fork while waiting for the other, the system can enter deadlock. The project implements a Linux terminal-based C prototype using POSIX threads and synchronization primitives. It demonstrates a fair FIFO waiter strategy and a resource-ordering strategy, records execution events, summarizes completed meals, and provides small supporting demonstrations of process control, IPC, memory allocation and file I/O.

## 2. Problem statement

Implement and visualize synchronization strategies to prevent deadlock and starvation among competing processes/threads, with particular focus on the Dining Philosophers problem. The implementation must be runnable from the Ubuntu terminal, require no graphical interface, and be testable in a controlled environment.

## 3. Objectives

- Model philosophers as concurrent POSIX threads and forks as mutex-protected resources.
- Prevent deadlock by avoiding circular wait and partial resource allocation.
- Mitigate starvation with FIFO ticket-based admission in the default strategy.
- Compare the FIFO waiter with strict resource ordering.
- Handle Ctrl+C / SIGTERM as a controlled stop request.
- Log events to a file and report each philosopher's meal count.
- Demonstrate relevant Linux system programming topics across CO-1 through CO-6.

## 4. System design

### Main simulation

Each philosopher repeatedly thinks, requests its two adjacent forks, eats when granted both, increments its completed-meal count, and releases both forks. The default waiter assigns tickets in arrival order. Only the head ticket can try to claim both forks. The coordinator mutex makes the two-fork grant atomic relative to other waiters. A condition variable wakes waiting threads after state changes; timed condition waits allow the simulation to notice a stop signal.

### Alternative strategy

The resource-ordering strategy always locks the lower-indexed fork before the higher-indexed fork. Because every thread follows one global ordering, the circular-wait condition is removed. It is included to compare deadlock prevention with fairness-oriented admission.

### Supporting OS demos

- Process control: child process creation with `fork`, program replacement with `exec`, and child collection with `waitpid`.
- IPC: anonymous pipe, named FIFO, and a `SIGUSR1` handler.
- Memory: dynamic allocation, touching one byte per page, page-size query, and `getrusage` statistics.
- File I/O: unbuffered file descriptor operations and buffered standard I/O.

## 5. Technology stack

- Language: C11
- Platform: Ubuntu/Linux (POSIX APIs)
- Compiler/build: GCC and Make
- Concurrency: pthreads, mutexes and condition variables
- OS interfaces: `fork`, `exec`, `waitpid`, `pipe`, `mkfifo`, `sigaction`, `malloc`, `getrusage`, `open`, `read`, `write`, `close`

## 6. CO coverage

| CO | Relevant content in this project |
|---|---|
| CO-1 | User-space C program, shell execution, process ID, system calls and Linux service APIs |
| CO-2 | Thread creation/join, process creation, execution replacement, wait and termination |
| CO-3 | Anonymous pipe, named FIFO, signal handler, SIGINT/SIGTERM stop handling |
| CO-4 | Virtual allocation through `malloc`, page-size query, demand-touching pages, page-fault/resource statistics |
| CO-5 | File descriptors, unbuffered system-call I/O, buffered stdio, persistent event log |
| CO-6 | POSIX threads, mutual exclusion, condition variables, shared-state protection, deadlock prevention and starvation mitigation |

## 7. Build and execution

```bash
sudo apt update
sudo apt install build-essential
make
./dining_os_lab help
./dining_os_lab simulate --philosophers 5 --meals 3
make test
```

To compare the strategies:

```bash
./dining_os_lab simulate --strategy waiter --philosophers 5 --meals 5 --log waiter.txt
./dining_os_lab simulate --strategy ordered --philosophers 5 --meals 5 --log ordered.txt
```

## 8. Testing and validation

The automated smoke test checks that:

1. Five philosophers each complete two meals under the waiter strategy (10 meals total).
2. Seven philosophers each complete one meal under resource ordering (7 meals total).
3. The process demo invokes the `waitpid` path.
4. IPC output contains both anonymous pipe and named FIFO messages and a successful signal-handler result.
5. Memory and file-I/O demos produce their expected summary output.

Tests can be run with `make test`. Results depend on the local compiler and POSIX environment. The tests are useful validation of expected execution paths, but they do not constitute exhaustive concurrency verification.

## 9. Expected results

- In finite mode, every philosopher that starts successfully completes its configured meal target unless a stop signal is received.
- No two neighboring philosophers can eat simultaneously because both require the same fork mutexes.
- The waiter strategy prevents circular wait and uses FIFO ticket admission to reduce starvation risk.
- Resource ordering prevents circular wait by enforcing a global fork acquisition order.
- Event logs make state transitions and final meal counts inspectable after execution.
- Supporting commands show representative process, IPC, memory and file-system APIs from Linux user space.

## 10. Safety, originality and limitations

The program operates only in user space, uses a bounded number of threads, creates only temporary local demo files, and needs no root permissions. The FIFO demo file is removed after use; the file-I/O demo removes its temporary file. The simulation log remains for review. The program does not alter kernel configuration or interact with unrelated processes. The implementation is educational and should not be interpreted as a hard real-time fairness guarantee.

## 11. Conclusion

The prototype demonstrates two sound approaches to deadlock prevention and shows how synchronization primitives can coordinate concurrent threads. The FIFO waiter is the recommended default because it combines prevention of circular wait with queue-based starvation mitigation. The additional command-line demonstrations connect the main problem to the relevant system-programming outcomes without introducing an unnecessary UI or unrelated features.

## 12. References

- `pthread_create(3)`, `pthread_mutex_lock(3)`, `pthread_cond_wait(3)` Linux/POSIX manual pages.
- `fork(2)`, `exec(3)`, `waitpid(2)`, `pipe(2)`, `mkfifo(3)`, `sigaction(2)` Linux manual pages.
- `malloc(3)`, `getrusage(2)`, `open(2)`, `read(2)`, `write(2)`, `close(2)` Linux manual pages.

# Operating System Simulator

This project simulates a basic operating system scheduler (`oss`) managing multiple concurrent worker processes (`worker`). It utilizes Inter-Process Communication (IPC) mechanisms like shared memory and message queues to coordinate activities and manage a simulated system clock.

## Description

The simulation consists of two main components:

1.  **`oss` (Operating System Simulator):** The main process that acts as the scheduler and resource manager. It forks worker processes, manages a simulated clock, tracks active processes using a Process Control Block (PCB) table, and communicates with worker processes via message queues.
2.  **`worker`:** Represents a user process. Each worker is launched by `oss`, runs for a predetermined duration, interacts with `oss` by reading the shared clock and exchanging messages, and then terminates.

The `oss` process controls how many total worker processes are generated (`-n`), how many can run simultaneously (`-s`), the maximum lifetime for each worker (`-t`), and the interval between launching new workers (`-i`). All significant events and the state of the process table are logged to a specified file (`-f`) and printed to standard output.

## Features

*   **Process Management:** Simulates creation (`fork`/`execlp`), tracking (PCB table), and termination (`wait`) of child processes.
*   **Concurrency Control:** Limits the number of simultaneously running worker processes.
*   **Simulated System Clock:** Maintains a clock (seconds and nanoseconds) in shared memory, accessible by all processes.
*   **Inter-Process Communication (IPC):**
    *   **Shared Memory:** Used to share the system clock between `oss` and `worker` processes.
    *   **Message Queues:** Used for bidirectional communication between `oss` and individual `worker` processes.
*   **Command-Line Configuration:** Allows setting simulation parameters via command-line arguments.
*   **Logging:** Records simulation progress, process table status, and communication events to a log file and standard output.
*   **Signal Handling:** Gracefully handles `SIGINT` (Ctrl+C) for cleanup and uses `SIGPROF` with an interval timer for a simulation timeout (60 seconds).

## How it Works

1.  **Initialization:** `oss` starts, parses command-line arguments, initializes shared memory for the clock, sets up a message queue, and prepares the PCB table. Signal handlers for `SIGINT` and `SIGPROF` are set up.
2.  **Process Launching:** `oss` periodically checks if it can launch a new `worker` process based on the total count (`-n`), simultaneous limit (`-s`), and launch interval (`-i`).
3.  **Worker Execution:** When launched, a `worker` process:
    *   Receives its maximum lifespan (relative to its start time) via command-line arguments from `oss`.
    *   Attaches to the shared memory to read the current system time.
    *   Attaches to the message queue.
    *   Calculates its absolute termination time.
    *   Enters a loop, waiting for messages from `oss`. Upon receiving a message, it checks if its termination time has passed. If not, it sends an acknowledgment message back to `oss`.
4.  **OSS-Worker Communication:** `oss` iterates through active workers (based on the PCB table) and sends a message (effectively a turn signal) to one worker at a time. It then waits for a response message from that worker.
5.  **Worker Termination:** When a `worker`'s simulated time expires, it sends a final message to `oss` indicating termination and then exits.
6.  **Cleanup:** `oss` detects the termination message, calls `wait()` to reap the terminated child, updates the PCB table, and decrements the count of current children.
7.  **Simulation End:** The simulation ends when all requested worker processes (`-n`) have been launched and have terminated. `oss` then cleans up IPC resources (shared memory, message queue) and exits.
8.  **Timeouts:** The simulation automatically terminates after 60 real-time seconds via `SIGPROF` and `setitimer`, ensuring it doesn't run indefinitely. Pressing Ctrl+C (`SIGINT`) also triggers cleanup and termination.

## IPC Mechanisms Used

*   **Shared Memory (`shmget`, `shmat`, `shmdt`, `shmctl`):** Primarily used to provide a globally accessible simulated system clock (`seconds`, `nanoseconds`).
*   **Message Queues (`msgget`, `msgsnd`, `msgrcv`, `msgctl`):** Used for control messages between `oss` and `worker` processes. `oss` sends messages to grant a 'turn' to a worker, and workers respond with acknowledgments or termination notices.

## Components

*   **`oss.cpp`:** The main scheduler process. Manages the clock, process table, process lifecycle, IPC setup/cleanup, and logging.
*   **`worker.cpp`:** The child process simulating user work. Reads the shared clock, communicates with `oss` via message queues, and runs for a specified duration.

## Build Instructions

You can compile the project using a C++ compiler like g++.

```bash
g++ oss.cpp -o oss -lrt
g++ worker.cpp -o worker -lrt
```
*(Note: `-lrt` might be needed on some systems for `shm_open` related functions, although this code uses System V IPC which often doesn't require it explicitly. Included for safety, remove if unnecessary on your system).*

## Usage

Run the `oss` executable with optional command-line arguments:

```bash
./oss [-h] [-n proc] [-s simul] [-t timeLimitForChildren] [-i intervalInMsToLaunchChildren] [-f logFile]
```

**Arguments:**

*   `-h`: Display help message and exit.
*   `-n proc`: Total number of worker processes to launch during the simulation (Default: 1).
*   `-s simul`: Maximum number of worker processes allowed to run concurrently (Default: 1, Max: 15).
*   `-t timeLimitForChildren`: Maximum lifespan *in seconds* for each worker process (Default: 1). A random nanosecond value (1-1,000,000,000) is added to this.
*   `-i intervalInMsToLaunchChildren`: Minimum interval *in simulated milliseconds* between launching new child processes (Default: 0, meaning launch as soon as possible).
*   `-f logFile`: Path to the log file where simulation details will be written (Default: `log.txt`).

**Example:**

```bash
./oss -n 15 -s 4 -t 2 -i 250 -f simulation.log
```

This command will:
*   Launch a total of 15 worker processes.
*   Allow a maximum of 4 workers to run concurrently.
*   Give each worker a maximum lifespan of 2 seconds (plus some nanoseconds).
*   Wait at least 250 simulated milliseconds between launching new workers.
*   Log output to `simulation.log`.

## Logging

The simulation logs information to both the standard output and the specified log file (`-f`). This includes:
*   Periodic printing of the Process Control Table.
*   Messages sent and received between `oss` and `workers`.
*   Worker process launch and termination events.
*   Current simulated system time at various points.

## Signal Handling

*   **`SIGINT` (Ctrl+C):** Catches the interrupt signal, attempts to kill all running child processes, detaches and removes shared memory, removes the message queue, and exits gracefully.
*   **`SIGPROF` (via `setitimer`):** A timer is set for 60 seconds. If the simulation runs for this long, the `SIGPROF` handler is invoked, which performs the same cleanup actions as the `SIGINT` handler and terminates the simulation.

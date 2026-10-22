# Parallel Computing

A systems programming project exploring multithreading, lock-free data structures, and GPU-accelerated computation.

## Overview

This repository contains practical implementations and experiments designed to understand parallel programming and performance optimization on modern computing systems.

The project explores workload distribution across CPU threads, synchronization using atomic operations, memory access patterns, and GPU-based computation.

Each task includes its implementation, technical documentation, build instructions, and experimental observations.

## Project Tasks

| Task | Description | Status |
|---|---|---|
| Task I | Multithreaded array summation using strided and block partitioning | Completed |
| Task II | Lock-free SPSC ring buffer | Planned |
| Task III | GPU-accelerated forest-fire simulation | Planned |

### Task I — Multithreaded Array Summation

Implement and compare three approaches to summing an array of 64-bit unsigned integers:

- Single-threaded summation
- Four-thread strided partitioning
- Four-thread block partitioning

The experiment examines correctness, execution time, thread management overhead, and the impact of memory access patterns on performance.

**[View Task I Documentation](task1/README.md)**

### Task II — Lock-Free SPSC Ring Buffer

Implement a fixed-capacity, single-producer single-consumer ring buffer using atomic operations without traditional locking primitives.

The implementation will explore concurrent data structures, memory ordering, and producer-consumer synchronization.

**[View Task II Documentation](task2/README.md)**

*Documentation and implementation will be added when this task is completed.*

### Task III — GPU Forest-Fire Simulation

Develop a GPU-accelerated forest-fire simulation on a two-dimensional grid using compute shaders, with a corresponding CPU implementation for comparison.

The experiment will explore GPU parallelism, simulation rules, and performance optimization.

**[View Task III Documentation](task3/README.md)**

*Documentation and implementation will be added when this task is completed.*

## Technologies

- C
- POSIX threads (Pthreads)
- Linux system programming
- Atomic operations and lock-free data structures
- OpenGL ES and EGL
- GPU compute shaders
- Performance measurement and analysis

## Repository Structure

```text
parallel-computing/
├── README.md
├── task1/
│   ├── README.md
│   ├── main.c
│   └── screenshots/
│       └── benchmark-results.png
├── task2/
│   └── README.md
└── task3/
    └── README.md
```


## Development Environment

The CPU-based tasks target Linux and use standard Linux development tools. The GPU simulation targets a compatible graphics environment, depending on the implementation and available hardware.

Refer to each task's README for its specific dependencies, compilation instructions, execution steps, and results.

## Progress

Task I has been implemented and tested with three summation strategies.

Tasks II and III are planned for subsequent development. Their documentation and experimental results will be added as the implementations progress.

---

This repository is maintained as a practical exploration of parallel computing and systems programming.
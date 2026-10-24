# Parallel Computing

A systems programming project exploring parallel computation, concurrent data structures, and GPU-accelerated simulation.

## Overview

This repository contains practical implementations developed to understand parallel programming, concurrency, memory access patterns, and performance optimization.

Each task includes its own source code, documentation, execution instructions, and experimental results.

## Project Tasks

| Task | Description | Status |
|---|---|---|
| Task I | Multithreaded array summation using strided and block partitioning | Completed |
| Task II | Lock-free SPSC ring buffer using atomic operations | Implemented; validation ongoing |
| Task III | GPU-accelerated forest-fire simulation | Planned |

### Task I — Multithreaded Array Summation

Compare sequential summation with two four-thread approaches: strided partitioning and contiguous block partitioning.

The experiment investigates workload distribution, execution time, and memory access patterns.

**[View Task I Documentation](task1/README.md)**

### Task II — Lock-Free SPSC Ring Buffer

Implement a fixed-capacity, single-producer single-consumer ring buffer using atomic cursors and a power-of-two backing array.

The implementation explores:

- Atomic operations and memory ordering.
- Producer-consumer coordination.
- Cache alignment and false-sharing considerations.
- RAII-based access handles.
- Throughput measurement under concurrent execution.

**[View Task II Documentation](task2/README.md)**

### Task III — GPU Forest-Fire Simulation

Develop a GPU-based forest-fire simulation on a two-dimensional grid, with a corresponding CPU implementation for comparison.

**[View Task III Documentation](task3/README.md)**

*Implementation and experimental results will be added when this task is completed.*

## Technologies

- C and C++
- POSIX threads
- C++ atomics and concurrency
- Linux and WSL
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
├── task2/
│   ├── README.md
│   ├── main.cpp
│   └── screenshots/
└── task3/
    └── README.md
```

## Project Objectives

- Understand practical parallel programming.
- Compare sequential and parallel algorithms.
- Explore atomic operations and concurrent data structures.
- Investigate memory access patterns and cache behavior.
- Measure throughput and execution time.
- Document implementation decisions and performance trade-offs.

## Development Environment

The CPU-based tasks target Linux-compatible environments. The GPU task will require compatible graphics APIs and hardware.

Refer to each task's README for its dependencies, build commands, execution instructions, and results.

## Progress

Tasks I and II have implementations available in the repository. Their correctness and performance are being evaluated through testing and benchmarking.

Task III remains planned.

---

This repository is an ongoing exploration of parallel computing and systems programming.
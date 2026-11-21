# Latency Investigation and Tracing

## Response-Time Guarantee

In this system, the response-time guarantee means the `control` thread must be scheduled and start handling an event within **10 ms** (`MAX_ALLOWED_LATENCY_MS = 10`) after the periodic timer (`EVENT_PERIOD_MS = 250`) fires.

## Measured Events

- **Start Event:** `event_created` inside `event_timer_expiry()`. When the timer ISR fires, it timestamps `event.ready_ms = k_uptime_get_32()`, pushes the event to `control_queue`, and gives the `maintenance_start` semaphore.
- **End Event:** When `control_fn()` unblocks from `k_msgq_get()` and samples `now = k_uptime_get_32()`. Latency is calculated as `now - event.ready_ms` (logged right after via `control_done`).

## Start Latency vs. Full Processing Time

- **Start Latency** is the scheduling delay between event creation (`event_created`) and the `control` thread actually waking up and starting execution. Our 10 ms deadline specifically governs this dispatch delay.
- **Full Processing Time** is the total time from `event_created` until `control_fn()` finishes its actual work and outputs `control_done` (start latency + execution time + any mid-task preemptions). Since `control_fn()` does almost no compute work here, full processing time is nearly identical to start latency.

## Root Cause Analysis

Looking at `artifacts/threads_before_fix.txt`, `maintenance` was configured with priority **4** while `control` had priority **7**. In Zephyr, lower numbers mean higher priority, so `maintenance` preempted `control`.

In `artifacts/ctf_trace_before_fix.txt`, every timer expiry (e.g., at `02:00:00.500`) marks both `control` and `maintenance` as ready (`thread_sched_ready`). As soon as the ISR exits:

1. The scheduler switches in `maintenance` (`thread_switched_in: maintenance`) instead of `control`.
2. `maintenance` runs `k_busy_wait(45000)`, holding the CPU for 45 ms until `02:00:00.545`.
3. `control` stays stuck in the ready queue until `maintenance` finishes and blocks on `semaphore_take_blocking`.
4. When `control` finally runs at `02:00:00.545`, the 10 ms system tick reads `now = 540 ms`, resulting in a **40 ms** latency (`540 - 500`) and missing the 10 ms deadline.

## Why Swapping Priorities Fixes It

Swapping the priorities (`CONTROL_PRIORITY = 4`, `MAINTENANCE_PRIORITY = 7` in `artifacts/threads_after_fix.txt`) gives `control` higher execution priority.

As confirmed in `artifacts/ctf_trace_after_fix.txt`, once the timer ISR exits at `02:00:00.500`, the scheduler immediately switches in `control` first. It reads the message queue, logs `control_done` with **0 ms** latency (`500 - 500`), and pends on the next message. Only then does `maintenance` switch in to run its 45 ms busy-wait in the background without blocking time-critical control work.

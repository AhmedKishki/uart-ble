Async UART Worker (Zephyr / nRF Connect SDK)

A small, production-style skeleton that shows async UART RX/TX with:

lock-free 4-byte payload caches,

an IPC layer (queues, semaphore, soft half-duplex flag),

a parser thread (bytes → commands),

a TX worker (serializes sends, handles -EBUSY),

and two example producers.

It's split into clean modules so ISR logic, protocol parsing, and workers don't step on each other.


Directory layout

```
app/
├─ CMakeLists.txt
├─ prj.conf
└─ src/
   ├─ main.c
   ├─ config.h
   ├─ ipc.c
   ├─ ipc.h
   ├─ serial/
   │  ├─ serial_io.c
   │  └─ serial_io.h
   ├─ proto/
   │  ├─ ab_payload.c
   │  ├─ ab_payload.h
   │  ├─ parser_worker.c
   │  └─ parser_worker.h
   ├─ workers/
   │  ├─ tx_worker.c
   │  └─ tx_worker.h
   └─ producers/
      ├─ producers.h
      ├─ producer_a.c
      └─ producer_b.c
```

What it does (data flow)

Producers publish 4-byte values for A and B →
ab_payload stores them atomically and raises a “ready” flag →
Parser waits for serial frames of the form 'S' <cmd> (where <cmd> is 'A' or 'B') and enqueues the command →
TX worker sees the command, snapshots the corresponding 4-byte payload, opens a soft half-duplex window (mutes RX echoes), starts UART TX (retrying on -EBUSY), waits for TX_DONE, clears the ready flag, and closes the window.

serial_io owns the UART device, double-buffered async RX, and the ISR.
ipc owns the RX/command queues, the TX-done semaphore, and the “TX active” flag used by both ISR and worker.
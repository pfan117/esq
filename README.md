# ESQ - Event Session Queues
### How it works
---
When startup, worker threads will be created, the count can be specified by configuration file. The main thread will keep running, it waits for a OS event object, currently a io_uring object, and dispatch events to worker threads. It also detect timeout events and dispatch timeout events to worker threads.

nghttp2 is supported for https connection and data posting.

Kyotocabinet is the data container, will be replaced by butter-db later.

# MiniRedis

A small in-memory key–value server that speaks the Redis wire protocol (RESP). It is not a full Redis replacement: no persistence, replication, or advanced data types.

## Build

```bash
make
```

Requires a C++17 compiler. Produces `miniredis-server` and `miniredis-cli`.

With CMake (optional):

```bash
cmake -S . -B build && cmake --build build
```

## Run

**Server** (default port `6380`):

```bash
./miniredis-server
./miniredis-server 9000
```

**Client**:

```bash
./miniredis-cli
./miniredis-cli -h 127.0.0.1 -p 6380
```

At the prompt, enter commands such as `PING`, `SET key value`, `GET key`, `KEYS *`, or `EXIT` to leave.

## Supported commands

`PING`, `ECHO`, `GET`, `SET`, `DEL`, `EXISTS`, `KEYS`, `FLUSHDB`, `FLUSHALL`, `QUIT`.

## Windows

Use [MSYS2](https://www.msys2.org/) (MINGW64) and run `mingw32-make` in this directory.

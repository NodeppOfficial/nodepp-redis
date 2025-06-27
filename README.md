# NodePP Redis: High-Performance Redis Client for NodePP

This project provides a robust and efficient Redis client specifically built for the NodePP asynchronous and event-driven C++ framework. Redis is an in-memory data structure store, used as a database, cache, message broker, stream engine, and more. By leveraging NodePP's performance, this client enables you to interact with Redis servers with high throughput and low latency within your NodePP applications.

## Key Features

- **Asynchronous Operations:** All Redis commands are executed asynchronously, preventing blocking within your NodePP event loop.
- **Connection Pooling:** Efficiently manages connections to your Redis server(s).
- **Pipelining:** Supports sending multiple commands to the server without waiting for each response individually, improving performance.
- **High Performance:** Benefits from NodePP's C++ foundation for fast communication with the Redis server.

## Example
```cpp
#include <nodepp/nodepp.h>
#include <redis/redis.h>

using namespace nodepp;

void onMain() {

    auto db = redis::add("db://auth@localhost:8000");

    db.exec(" SET FOO BAT ");

    db.exec("GET FOO",[]( string_t data ){
        console::log( "->", data )
    });

}
```

## Compilation
``` bash
g++ -o main main.cpp -I ./include ; ./main
```

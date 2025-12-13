# Nodepp Asynchronous Redis Client
A high-performance, asynchronous TCP client for the Redis in-memory data store, built specifically for the Nodepp framework. This client wraps raw TCP sockets to handle the Redis Serialization Protocol (RESP) using Nodepp's promises, coroutines, and generators for efficient, non-blocking network I/O.

## Dependencies & Cmake Integration
```bash
include(FetchContent)

FetchContent_Declare(
	nodepp
	GIT_REPOSITORY   https://github.com/NodeppOfficial/nodepp
	GIT_TAG          origin/main
	GIT_PROGRESS     ON
)
FetchContent_MakeAvailable(nodepp)

FetchContent_Declare(
	nodepp-redis
	GIT_REPOSITORY   https://github.com/NodeppOfficial/nodepp-redis
	GIT_TAG          origin/main
	GIT_PROGRESS     ON
)
FetchContent_MakeAvailable(nodepp-redis)

#[...]

target_link_libraries( #[...]
	PUBLIC nodepp nodepp-redis #[...]
)
```

##  Connection and Authentication
Connections are managed using standard Redis URI format. Authentication credentials provided in the URI are automatically handled by the client upon connection.

```cpp
#include <nodepp/nodepp.h>
#include <redis/redis.h>

using namespace nodepp;

void main() {
    
    // Standard Redis URI format: redis://[user]:[password]@[host]:[port]
    // Standard Redis URI format: redis://[host]:[port]
    const string_t uri = "redis://user:password@localhost:6379"; 

    // Asynchronous Connection
    redis::connect(uri)

    .then([]( redis_t client ) {
        console::log("Connected to Redis. Pinging...");
    })

    .fail([]( const except_t& error ) {
        console::error("Connection failed:", error.get());
    });
}
```

## Asynchronous API Reference
The redis_t class provides three core methods that align with Nodepp's event loop structure for executing raw RESP commands. Note: Commands must be sent in the raw RESP format (e.g., SET mykey myvalue\r\n).

**1. Promise-based (.resolve()) - Preferred Method**
Runs the command and returns a promise that resolves with an array of responses (or rejects on error/closed connection).

```cpp
db.resolve("GET FOO")

.then([]( array_t<string_t> results ) {
    console::log("Fetched %d items.", results.size());
})

.fail([]( except_t error ) {
    console::error("Async query failed:", error.what());
});
```

**2. Synchronous/Blocking (.await()) - Fiber Only**
A convenience method for use within a Nodepp coroutine (fiber), allowing the code to look synchronous while internally yielding the fiber until the response is ready.

```cpp
try {

    auto results = db.await("LRANGE FOO");
    console::log( "Current item:", results[0] );

} catch( except_t error ) {
    console::error("Synchronous read failed:", error.what());
}
```

**3. Fire-and-Forget (.emit())**
Used primarily for commands where you don't need to wait for a full response (like simple SETs) or for streaming results (though streaming is limited by the current parser).

```cpp
db.emit("SET FOO 10");
```
```cpp
db.emit("LRANGE FOO", []( string_t row ) {
    console::log("Processing row", row );
});
```

## Compilation
``` bash
g++ -o main main.cpp -I ./include ; ./main
```

## License
**Nodepp-redis** is distributed under the MIT License. See the LICENSE file for more details.
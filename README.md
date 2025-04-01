# NODEPP-REDIS
Run a **Redis** client in Nodepp

## Example
```cpp
#include <nodepp/nodepp.h>
#include <redis/tcp.h>

using namespace nodepp;

void onMain() {

    redis_t db ("db://auth@localhost:8000");

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

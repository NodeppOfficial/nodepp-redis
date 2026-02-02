
#include <nodepp/nodepp.h>
#include <redis/redis.h>
#include <nodepp/worker.h>

using namespace nodepp;

void onMain() {

    auto db = redis::add("db://localhost:6379");

    for( auto x=100; x-->0; ){ process::add([=](){
         auto db = redis::add("db://localhost:6379");

         db.emit("LRANGE FOO 0 -1",[=]( string_t data ){
             console::log( "->", x, data );
         }); return -1;

    }); }

    db.await( R"( MULTI
        DEL   FOO
        LPUSH FOO 1
        LPUSH FOO 2
        LPUSH FOO 3
        LPUSH FOO 4
        LPUSH FOO 5
    EXEC )" );

}

#include <nodepp/nodepp.h>
#include <redis/tcp.h>

using namespace nodepp;

void onMain() {

    auto db = redis::tcp::add("db://localhost:6379");

    db.exec("LPUSH FOO BAT1");
    db.exec("LPUSH FOO BAT2");
    db.exec("LPUSH FOO BAT3");
    db.exec("LPUSH FOO BAT4");
    db.exec("LPUSH FOO BAT5");
    db.exec("LPUSH FOO BAT6");

    db.exec("LRANGE FOO 0 -1",[]( string_t data ){
        console::log( "->", data );
    });

}

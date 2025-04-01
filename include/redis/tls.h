/*
 * Copyright 2023 The Nodepp Project Authors. All Rights Reserved.
 *
 * Licensed under the MIT (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/NodeppOficial/nodepp/blob/main/LICENSE
 */

/*────────────────────────────────────────────────────────────────────────────*/

#ifndef NODEPP_REDIS_TLS
#define NODEPP_REDIS_TLS

/*────────────────────────────────────────────────────────────────────────────*/

#include <nodepp/nodepp.h>
#include <nodepp/stream.h>
#include <nodepp/tls.h>
#include <nodepp/url.h>

/*────────────────────────────────────────────────────────────────────────────*/

#ifndef NODEPP_REDIS_GENERATOR
#define NODEPP_REDIS_GENERATOR

namespace nodepp { namespace _redis_ { GENERATOR( cb ){
protected:

    string_t raw, data;
    ptr_t<ulong> pos;

public:

    template< class T, class V, class U > coEmit( T& fd, V& cb, U& self ){
    coStart pos = ptr_t<ulong>({ 1, 0 }); coYield(1); raw = fd.read_line();

        if(  regex::test( raw, "[$*]-1",true ) ){ coEnd; }
        if(  regex::test( raw, "^[+]" ) || raw.empty() ){ coEnd; }
        if( !regex::test( raw, "[$*:]-?\\d+" ) ){ process::error( raw.slice(0,-2) ); }

        if( regex::test( raw, "[*]\\d+" ) ){
            pos[0] = string::to_ulong( regex::match( raw, "\\d+" ) );
            if( pos[0] == 0 ){ coEnd; } coGoto(1);
        } elif( regex::test( raw, "[$]\\d+" ) ) {
            pos[1] = string::to_ulong( regex::match( raw, "\\d+" ) ) + 2;
        } elif( regex::test( raw, "[:]\\d+" ) ) {
            cb( regex::match( raw, "\\d+" ) ); coEnd;
        }

        while( pos[0]-->0 ){ data.clear();
        while( data.size() != pos[1] ){
               data += fd.read( pos[1]-data.size() );
        }      cb( data.slice( 0,-2 ) ); coNext;
        if ( pos[0] != 0 ){ coGoto(1); }
        }

    coStop
    }

};}}

#endif

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { class redis_tls_t {
protected:

    struct NODE {
        int state =1;
        ssocket_t fd;
    };  ptr_t<NODE> obj;

public:

    virtual ~redis_tls_t() noexcept {
        if( obj.count() > 1 )
          { return; } free();
    }

    /*─······································································─*/

    virtual void free() const noexcept {
        if( obj->state == 0 ){ return; }
            obj->state  = 0; obj->fd.free();
    }

    /*─······································································─*/

    redis_tls_t ( string_t uri, ssl_t* ssl ) : obj( new NODE ) {
        if( ssl.create_client() == -1 )
          { process::error("Error Initializing SSL context"); }
        if( !url::is_valid( uri ) )
          { process::error("Invalid Redis Url"); }

        auto host = url::hostname( uri );
        auto port = url::port( uri );
        auto auth = url::auth( uri );
        auto user = url::user( uri );
        auto pass = url::pass( uri );
        auto Auth = string_t();

        if( !user.empty() && !pass.empty() ){
             Auth = string::format("AUTH %s %s\n", user.get(), pass.get() );
        } elif( !auth.empty() ) {
             Auth = string::format("AUTH %s\n", auth.get() );
        }

        obj->fd = ssocket_t();
        obj->fd.IPPROTO = IPPROTO_TCP;
        obj->fd.onError([]( ... ){ });
        obj->fd.socket( dns::lookup(host), port );

        if( obj->fd.connect() < 0 ){
            process::error("While Connecting to Redis");
        }

        obj->fd.ssl = new ssl_t( ssl, obj->fd.get_fd() );
        obj->fd.ssl->set_hostname( host );

        if( obj->fd.ssl->connect() <= 0 ){
            process::error("While Handshaking TLS to Redis");
        }

        if( !Auth.empty() ){ exec( Auth ); }

    }

    redis_tls_t () : obj( new NODE ) { obj->state = 0; }

    /*─······································································─*/

    void exec( const string_t& cmd, const function_t<void,string_t>& cb ) const {
        if( obj->state == 0 || obj->fd.is_closed() ) { return; }
        auto self = type::bind( this ); obj->fd.write( cmd + "\n" );
        _redis_::cb task; process::add( task, obj->fd, cb, self );
    }

    array_t<string_t> exec( const string_t& cmd ) const {
        if( obj->state == 0 || obj->fd.is_closed() ) { return nullptr; }
        array_t<string_t> res; auto self = type::bind( this ); obj->fd.write( cmd + "\n" );
        function_t<void,string_t> cb([&]( string_t data ){ res.push( data ); });
        _redis_::cb task; process::await( task, obj->fd, cb, self ); return res;
    }

    /*─······································································─*/

    string_t raw( const string_t& cmd ) const noexcept {
        if( obj->state == 0 || obj->fd.is_closed() )
          { return nullptr; }  obj->fd.write( cmd + "\n" );
        return obj->fd.read();
    }

};}

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { namespace redis { namespace tls {

    template<class...T>
    redis_tls_t add( const T&... args ) {
        return redis_tls_t( args... );
    }

}}}

/*────────────────────────────────────────────────────────────────────────────*/

#endif

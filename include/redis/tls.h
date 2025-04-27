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
#include <nodepp/promise.h>
#include <nodepp/stream.h>
#include <nodepp/tls.h>
#include <nodepp/url.h>

/*────────────────────────────────────────────────────────────────────────────*/

#ifndef NODEPP_REDIS_GENERATOR
#define NODEPP_REDIS_GENERATOR

namespace nodepp { namespace _redis_ { GENERATOR( cb ){
protected:
    _file_::line line; _file_::read read;
    string_t raw, data; ptr_t<ulong> pos;
public:

    template< class T, class V, class U > coEmit( T& fd, V& cb, U& self ){
        if( fd.is_closed() ){ return -1; }
    gnStart

        pos = ptr_t<ulong>({ 1, 0 }); coYield(1);

        while( this->line( &fd )==1 ){ coNext; }
           if( this->line.state ==0 ){ coEnd;  }
         raw = this->line.data;

        if(  regex::test( raw, "[$*]-1",true ) )        { coEnd; }
        if(  regex::test( raw, "^[+]" ) || raw.empty() ){ coEnd; }
        if( !regex::test( raw, "[$*:]-?\\d+" ) )        { process::error( raw.slice(0,-2) ); coEnd; }

        if( regex::test( raw, "[*]\\d+" ) ){
            pos[0] = string::to_ulong( regex::match( raw, "\\d+" ) );
        if( pos[0] == 0 ){ coEnd; } coGoto(1);
        } elif( regex::test ( raw, "[$]\\d+" ) ) {
            pos[1] = string::to_ulong( regex::match( raw, "\\d+" ) ) + 2;
        } elif( regex::test ( raw, "[:]\\d+" ) ) {
            cb( regex::match( raw, "\\d+" ) ); coEnd;
        }

        while( pos[0]-->0 )           { data.clear();
        while( data.size() != pos[1] ){
        while( this->read( &fd, pos[1]-data.size() )==1 ){ coNext; }
           if( this->read.state==0 ){ coEnd; }data+=this->read.data;
        }      cb( data.slice( 0,-2 ) );

        if ( pos[0] != 0 ){ coGoto(1); } }

    gnStop
    }

};}}

#endif

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { class redis_tls_t {
protected:

    struct NODE {
        bool state=0;
        ssocket_t fd;
    };  ptr_t<NODE> obj;

public:

    redis_tls_t ( ssocket_t cli ) : obj( new NODE ) { set_fd(cli); }

    redis_tls_t () : obj( new NODE ) {}

    /*─······································································─*/

    virtual ~redis_tls_t() noexcept { if( obj.count()>1 ) { return; } free(); }

    /*─······································································─*/

    void set_fd( ssocket_t cli ) const noexcept { obj->fd=cli; obj->state=1;     }
    bool is_available()          const noexcept { return obj->fd.is_available(); }
    bool is_closed()             const noexcept { return obj->fd.is_closed();    }

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

    /*─······································································─*/

    virtual void free() const noexcept {
        if( obj->state == 0 ){ return; }
            obj->state  = 0; obj->fd.free();
    }

};}

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { namespace redis { namespace tls {

    promise_t<redis_tls_t,except_t> connect( const string_t& uri ) {
    return promise_t<redis_tls_t,except_t> ([=]( function_t<void,redis_tls_t> res, function_t<void,except_t> rej ){
        if( !url::is_valid( uri ) ){ rej( except_t("Invalid Redis URL") ); return; }

        auto rdis = type::bind( new redis_tls_t() );
        auto host = url::hostname( uri );
        auto port = url::port( uri );
        auto auth = url::auth( uri );
        auto user = url::user( uri );
        auto pass = url::pass( uri );
        auto Auth = string_t();
        auto ssl  = ssl_t();

        if( !user.empty() && !pass.empty() ){
            Auth = string::format("AUTH %s %s\n", user.get(), pass.get() );
        } elif( !auth.empty() ) {
            Auth = string::format("AUTH %s\n", auth.get() );
        }

        auto client = tls_t ([=]( ssocket_t cli ){
             rdis->set_fd( cli ); if( !Auth.empty() )
             { rdis->exec( Auth ); } res(*rdis); return;
        }, &ssl );

        client.onError([=]( except_t error ){ rej(error); });
        client.connect( host, port );

    }); }

    /*─······································································─*/

    template<class...T>
    redis_tls_t await( const T&... args ) {
        auto raw = connect( args... ).await();
        if( !raw.has_value() ){ throw raw.error(); } return raw.value();
    }

    /*─······································································─*/

    template<class...T>
    redis_tls_t add( const T&... args ) {
        auto raw = connect( args... ).await();
        if( !raw.has_value() ){ throw raw.error(); } return raw.value();
    }

}}}

/*────────────────────────────────────────────────────────────────────────────*/

#endif

/*
 * Copyright 2023 The Nodepp Project Authors. All Rights Reserved.
 *
 * Licensed under the MIT (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/NodeppOfficial/nodepp/blob/main/LICENSE
 */

/*────────────────────────────────────────────────────────────────────────────*/

#ifndef NODEPP_REDIS_TCP
#define NODEPP_REDIS_TCP

/*────────────────────────────────────────────────────────────────────────────*/

#include <nodepp/nodepp.h>
#include <nodepp/promise.h>
#include <nodepp/stream.h>
#include <nodepp/tcp.h>
#include <nodepp/url.h>

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { namespace redis {

    regex_t reg0 = regex_t( "([^\r]+)\r\n" );
    regex_t reg1 = regex_t( "^[+]" );
    regex_t reg2 = regex_t( "^[-]" );
    regex_t reg3 = regex_t( "^[:]" );
    regex_t reg4 = regex_t( "^[$]" );
    regex_t reg5 = regex_t( "^[!]" );

} }

/*────────────────────────────────────────────────────────────────────────────*/

#ifndef NODEPP_REDIS_GENERATOR
#define NODEPP_REDIS_GENERATOR

namespace nodepp { namespace _redis_ { GENERATOR( stream ){
protected:
    generator::file::write write; 
    generator::file::read  read;
public:

    template< class V, class U >
    coEmit( string_t cmd, const V& cb, const U& self ){
    auto fd = self->get_fd() ; gnStart

        coWait( self->is_used() ==1 ); self->use();
        coWait( write( &fd,cmd )==1 );
            if( write.state     <=0 ){ coGoto(2); }
        coWait( read ( &fd )    ==1 );
            if( read .state     <=0 ){ coGoto(2); }

        do { redis::reg0.search_all(read.data); 
        auto list =redis::reg0.get_memory(); redis::reg0.clear_memory();
        for( ulong x=0; x<list.size(); ++x ){ /*----------------------*/
            
            if( redis::reg1.test( list[x] ) ){ cb( list[x].slice(1) );  }
          elif( redis::reg2.test( list[x] ) ){ cb( list[x].slice(1) );  }
          elif( redis::reg3.test( list[x] ) ){ cb( list[x].slice(1) );  }
          elif( redis::reg4.test( list[x] ) ){
            if( string::to_int( list[x].slice(1) )>0 ){ cb( list[++x] ); }}
          elif( redis::reg5.test( list[x] ) ){
            if( string::to_int( list[x].slice(1) )>0 ){ cb( list[++x] ); }}
          else{ continue; }

        } } while(0);

        coYield(2); self->release();

    gnStop
    }

};}}

#endif

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { class redis_t {
protected:

    struct NODE {
        bool used =0;
        bool state=0;
        socket_t  fd;
    };  ptr_t<NODE> obj;

public:

    event_t<> onRelease;
    event_t<> onUse;

    /*─······································································─*/
     
   ~redis_t () noexcept { if( obj.count()>1 ) { return; } free(); }
    redis_t ( socket_t cli ) :obj( new NODE ) { set_fd(cli); }
    redis_t () : obj( new NODE ) {}

    /*─······································································─*/

    void set_fd( socket_t cli ) const noexcept { obj->fd=cli; obj->state=1; }
    socket_t&  get_fd()         const noexcept { return obj->fd;            }

    /*─······································································─*/

    void use()       const noexcept { if( obj->used==1 ){ return; } obj->used=1; onUse    .emit(); }
    void release()   const noexcept { if( obj->used==0 ){ return; } obj->used=0; onRelease.emit(); }
    bool is_closed() const noexcept { return obj->state==0; }
    bool is_used()   const noexcept { return obj->used ==1; }

    /*─······································································─*/

    void exec( const string_t& cmd, const function_t<void,string_t>& cb ) const {
        if( cmd.empty() || is_closed() || obj->fd.is_closed() ) { return; }
        auto self = type::bind(this); _redis_::stream task;
        process::add( task, cmd+"\n", cb, self );
    }

    array_t<string_t> exec( const string_t& cmd ) const {
        if( cmd.empty() || is_closed() || obj->fd.is_closed() ) { return nullptr; }
        queue_t<string_t> res; auto self = type::bind(this); _redis_::stream task;
        function_t<void,string_t> cb([&]( string_t data ){ res.push(data); });
        process::await( task, cmd+"\n", cb, self ); return res.data();
    }

    /*─······································································─*/

    void async( const string_t& cmd ) const {
        if( cmd.empty() || is_closed() || obj->fd.is_closed() ) { return; }
        auto self = type::bind(this); _redis_::stream task;
        function_t<void,string_t> cb([=]( string_t ){});
        process::add( task, cmd+"\n", cb, self );
    }

    void await( const string_t& cmd ) const {
        if( cmd.empty() || is_closed() || obj->fd.is_closed() ) { return; }
        auto self = type::bind(this); _redis_::stream task;
        function_t<void,string_t> cb([&]( string_t ){});
        process::await( task, cmd+"\n", cb, self );
    }

    /*─······································································─*/

    virtual void free() const noexcept {
        if( obj->state==0 ){ return; }
            obj->state =0; release(); 
            onUse.clear(); get_fd().free();
    }

};}

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { namespace redis {

    promise_t<redis_t,except_t> connect( const string_t& uri ) {
    return promise_t<redis_t,except_t> ([=]( function_t<void,redis_t> res, function_t<void,except_t> rej ){
        if( !url::is_valid( uri ) ){ rej( except_t("Invalid Redis URL") ); return; }

        auto host = url::hostname( uri );
        auto port = url::port( uri );
        auto auth = url::auth( uri );
        auto user = url::user( uri );
        auto pass = url::pass( uri );
        auto Auth = string_t();

        if( !user.empty() && !pass.empty() ){
            Auth = string::format("AUTH %s %s", user.get(), pass.get() );
        } elif( !auth.empty() ) {
            Auth = string::format("AUTH %s", auth.get() );
        }

        auto client= tcp_t ([=]( socket_t cli ){
        auto rdis  = redis_t();
             rdis.set_fd( cli );  if(!Auth.empty() )
           { rdis.exec( Auth ); } res(rdis); return;
        });

        client.onError([=]( except_t error ){ rej(error); });
        client.connect( host, port );

    }); }

    /*─······································································─*/

    template<class...T>
    redis_t add( const T&... args ) {
        auto raw = connect( args... ).await();
        if( !raw.has_value() ){ throw raw.error(); } return raw.value();
    }

}}

/*────────────────────────────────────────────────────────────────────────────*/

#endif
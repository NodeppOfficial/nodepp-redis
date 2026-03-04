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
#include <nodepp/expected.h>
#include <nodepp/optional.h>
#include <nodepp/promise.h>
#include <nodepp/stream.h>
#include <nodepp/tcp.h>
#include <nodepp/url.h>

/*────────────────────────────────────────────────────────────────────────────*/

#ifndef NODEPP_REDIS_GENERATOR
#define NODEPP_REDIS_GENERATOR

namespace nodepp { namespace _redis_ { GENERATOR( pipe ){
protected:
//  generator::file::write write;
    generator::file::read  read ;
public:

    template< class V, class U >
    coEmit( const V& cb, const U& fd ){

        thread_local static ptr_t<regex_t> reg ({
            regex_t( "([^\r]+)\r\n" ),
            regex_t( "^[+]" ),
            regex_t( "^[-]" ),
            regex_t( "^[:]" ),
            regex_t( "^[$]" ),
            regex_t( "^[!]" )
        });

    coBegin

        coWait( read( &fd ) ==1 );
            if( read.state  <=0 ){ coGoto(2); }

        do { reg[0].search_all(read.data); 
        auto list =reg[0].get_memory(); reg[0].clear_memory();
        for( ulong x=0; x<list.size(); ++x ){ /*--------*/
            
          if  ( reg[1].test( list[x] ) ){ cb( list[x].slice(1) );  }
          elif( reg[2].test( list[x] ) ){ cb( list[x].slice(1) );  }
          elif( reg[3].test( list[x] ) ){ cb( list[x].slice(1) );  }
          elif( reg[4].test( list[x] ) ){
          if  ( string::to_int( list[x].slice(1) )>0 ){ cb( list[++x] ); }}
          elif( reg[5].test( list[x] ) ){
          if  ( string::to_int( list[x].slice(1) )>0 ){ cb( list[++x] ); }}
          else{ continue; }

        } } while(0); coYield(2);

    coFinish
    }

};}}

#endif

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { class redis_t {
protected:

    enum STATE {
         REDIS_STATE_UNKNOWN = 0b00000000,
         REDIS_STATE_OPEN    = 0b00000001,
         REDIS_STATE_USED    = 0b10000000,
         REDIS_STATE_CLOSE   = 0b00000010
    };

    struct NODE {
        int  state=0; socket_t fd;
    };  ptr_t<NODE> obj;

    void use()     const noexcept { obj->state|= STATE::REDIS_STATE_USED ; }
    void release() const noexcept { obj->state&=~STATE::REDIS_STATE_USED ; }

public:

    socket_t& get_fd() /*---------*/ const noexcept { return obj->fd; }

    /*─······································································─*/

    redis_t () : obj( new NODE ) { obj->state=STATE::REDIS_STATE_CLOSE; }

   ~redis_t () noexcept { if( obj.count()>1 ) { return; } free(); }
    
    redis_t ( socket_t cli ) :obj( new NODE ) { 
        obj->fd    = cli; /*-------------*/
        obj->state = STATE::REDIS_STATE_OPEN;
    }

    /*─······································································─*/

    bool is_closed()    const noexcept { return obj->state & STATE::REDIS_STATE_CLOSE; }
    bool is_used()      const noexcept { return obj->state & STATE::REDIS_STATE_USED ; }
    void close()        const noexcept { /*--*/ obj->state = STATE::REDIS_STATE_CLOSE; }
    bool is_available() const noexcept { return !is_closed(); }

    /*─······································································─*/

    expected_t<redis_t,except_t>
    emit( const string_t& cmd, function_t<void,string_t> cb=nullptr ) const noexcept {
    except_t err; do {

        if( is_used() )
          { return except_t( "redis Error: already in use" ); }
        
        if( cmd.empty() || is_closed() || obj->fd.is_closed() )
          { err = except_t( "Redis Error: closed" ); break; }

        auto pipe = type::bind( _redis_::pipe() );
        auto self = type::bind( this ); 
        obj->fd.write( cmd + "\n" ); use();

        process::poll( get_fd(), POLL_STATE::READ | POLL_STATE::EDGE, [=](){
            while( (*pipe)(cb,self->get_fd())==1 ){ /*unused*/ }
            self->release(); /*------------------*/ return -1;
        });

        /*--------------*/ return *this; 
    } while(0); release(); return  err ; }

    /*─······································································─*/

    promise_t<ptr_t<string_t>,except_t> resolve( const string_t& cmd ) const noexcept { 

        auto self = type::bind( this ); queue_t<string_t> list; 

    return promise_t<ptr_t<string_t>,except_t>([=]( 
        res_t<ptr_t<string_t>> res, 
        rej_t<except_t> /*--*/ rej
    ){ except_t err; do {

        if( self->is_used() )
          { rej( except_t( "redis Error: already in use" ) ); return; }
        
        if( cmd.empty() || self->is_closed() || self->obj->fd.is_closed() )
          { err = except_t( "redis Error: closed" ); break; }

        function_t<void,string_t> cb ([=]( string_t args ){ 
            list.push(args); 
        });

        auto pipe = type::bind( _redis_::pipe() ); 
        self->obj->fd.write( cmd + "\n" ); 
        self->use();
        
        process::poll( self->get_fd(), POLL_STATE::READ | POLL_STATE::EDGE, [=](){
            while( (*pipe)(cb,self->get_fd())==1 ){ /*unused*/ }
            res( list.data() ); self->release(); return -1; 
        }); 
    
    return; } while(0); release(); rej( err ); }); }

    /*─······································································─*/

    void free() const noexcept {
        if( obj->state==0 ){ return ; }
            obj->state =0; release(); 
        get_fd().free();
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
        auto rdis  = redis_t(cli); if( !Auth.empty() )
           { rdis.resolve( Auth ).await(); } 
        res(rdis); return; });

        client.onError([=]( except_t error ){ rej(error); });
        client.connect( host, port );

    }); }

    /*─······································································─*/

    template<class...T> expected_t<redis_t,except_t> 
    add( const T&... args ) { return connect( args... ).await(); }

}}

/*────────────────────────────────────────────────────────────────────────────*/

#endif
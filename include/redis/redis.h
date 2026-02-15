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

namespace nodepp { namespace _redis_ { GENERATOR( cb ){
protected:
    generator::file::write write; 
    generator::file::read  read;
public:

    template< class V, class U >
    coEmit( string_t cmd, const V& cb, const U& self ){

        thread_local static ptr_t<regex_t> reg ({
            regex_t( "([^\r]+)\r\n" ),
            regex_t( "^[+]" ),
            regex_t( "^[-]" ),
            regex_t( "^[:]" ),
            regex_t( "^[$]" ),
            regex_t( "^[!]" )
        });

    auto fd = self->get_fd() ; coBegin

        coWait( self->is_used() ==1 ); self->use();
        coWait( write( &fd,cmd )==1 );
            if( write.state     <=0 ){ coGoto(2); }
        coWait( read ( &fd )    ==1 );
            if( read .state     <=0 ){ coGoto(2); }

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

        } } while(0);

        coYield(2); self->release();

    coFinish
    }

};}}

#endif

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp { class redis_t {
protected:

    enum STATE {
        SQL_STATE_UNKNOWN = 0b00000000,
        SQL_STATE_OPEN    = 0b00000001,
        SQL_STATE_USED    = 0b10000000,
        SQL_STATE_CLOSE   = 0b00000010
    };

    struct NODE {
        int  state=0; socket_t fd;
    };  ptr_t<NODE> obj;

public:

    void       set_fd( socket_t cli ) const noexcept { obj->fd=cli; obj->state = STATE::SQL_STATE_OPEN; }
    socket_t&  get_fd() /*---------*/ const noexcept { return obj->fd; }

    /*─······································································─*/

    redis_t () : obj( new NODE ) { obj->state=STATE::SQL_STATE_CLOSE; }
   ~redis_t () noexcept { if( obj.count()>1 ) { return; } free(); }
    redis_t ( socket_t cli ) :obj( new NODE ) { set_fd(cli); }

    /*─······································································─*/

    bool is_closed()    const noexcept { return obj->state & STATE::SQL_STATE_CLOSE; }
    bool is_used()      const noexcept { return obj->state & STATE::SQL_STATE_USED ; }
    void close()        const noexcept { /*--*/ obj->state = STATE::SQL_STATE_CLOSE; }
    void use()          const noexcept { /*--*/ obj->state|= STATE::SQL_STATE_USED ; }
    void release()      const noexcept { /*--*/ obj->state&=~STATE::SQL_STATE_USED ; }
    bool is_available() const noexcept { return !is_closed(); }

    /*─······································································─*/

    promise_t<array_t<string_t>,except_t> resolve( const string_t& cmd ) const { 
           queue_t<string_t> arr; auto self = type::bind( this );
    return promise_t<array_t<string_t>,except_t>([=]( 
        res_t<array_t<string_t>> res, 
        rej_t<except_t> /*----*/ rej
    ){

        function_t<void,string_t> cb ([=]( string_t args ){ arr.push(args); });
        
        if( cmd.empty() || self->is_closed() || self->obj->fd.is_closed() )
          { rej(except_t( "redis Error: closed" )); return; }

        auto task = type::bind( _redis_::cb() ); process::add([=](){
            while( (*task)( cmd+"\n", cb, self )==1 ){ return 1; }
            res( arr.data() ); return -1; 
        }); 
    
    }); }

    /*─······································································─*/

    expected_t<array_t<sql_item_t>,except_t> 
    await( const string_t& cmd ) const { return resolve( cmd ).await(); }

    /*─······································································─*/

    optional_t<except_t>
    emit( const string_t& cmd, function_t<void,string_t> cb=nullptr ) const {
        
        if( cmd.empty() || is_closed() || obj->fd.is_closed() )
          { return except_t( "Redis Error: closed" ); }

        _redis_::cb task; auto self = type::bind( this );
        process::add( task, cmd+"\n", cb, self );

    return nullptr; }

    /*─······································································─*/

    void free() const noexcept {
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
             rdis.set_fd( cli ); if( !Auth.empty() )
           { rdis.await( Auth ); } res(rdis); return;
        });

        client.onError([=]( except_t error ){ rej(error); });
        client.connect( host, port );

    }); }

    /*─······································································─*/

    template<class...T> expected_t<redis_t,except_t> 
    add( const T&... args ) { return connect( args... ).await(); }

}}

/*────────────────────────────────────────────────────────────────────────────*/

#endif
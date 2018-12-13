// Copyright (c) 2014-2019 Robert A. Alfieri
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Analysis.h - class for analyzing logs generated by Logger.h
//
//              range analysis
//              redundancy analysis
//              pipeline analysis
//              precision analysis
//
#ifndef _Analysis_h
#define _Analysis_h

#include <string>
#include <cmath>
#include <iostream>
#include <vector>
#include <map>

#include "Cordic.h"

template< typename T=int64_t, typename FLT=double >
class Analysis
{
public:
    Analysis( std::string file_name = "" );     // "" means use stdin
    ~Analysis();

private:
    std::istream *      in;
    bool                in_text;

    struct FuncInfo
    {
        size_t   func_i;
    };

    struct FrameInfo
    {
        size_t   func_i;
    };

    struct CordicInfo
    {
        size_t   cordic_i;
        bool     is_alive;
        uint32_t int_w;
        uint32_t frac_w;
        uint32_t n;
    };

    struct ValInfo
    {
        bool    is_alive;
        bool    is_assigned;
        size_t  cordic_i;
        size_t  opnd_i[3];
        T       encoded;
        bool    is_constant;
        FLT     constant;
        FLT     min;
        FLT     max;
    };

    enum class KIND
    {
        cordic_constructed,
        cordic_destructed,
        enter,
        leave,
        constructed,
        destructed,
        op1, 
        op1i, 
        op1f, 
        op2, 
        op2i, 
        op2f, 
        op3, 
        op4, 
    };

    static constexpr uint32_t KIND_cnt = 12;

    using OP = typename Cordic<T,FLT>::OP;

    std::map<std::string, KIND>                 kinds;
    std::map<std::string, OP>                   ops;
    std::map<std::string, FuncInfo>             funcs;
    std::vector<FrameInfo>                      stack;
    std::map<uint64_t, CordicInfo>              cordics;
    std::map<uint64_t, ValInfo>                 vals;

    static constexpr uint32_t                   VAL_STACK_CNT_MAX = 2;
    ValInfo                                     val_stack[VAL_STACK_CNT_MAX];
    uint32_t                                    val_stack_cnt;

    void                val_stack_push( const ValInfo& val );
    ValInfo             val_stack_pop( void );

    static void         _skip_junk( char *& c );
    static std::string  parse_name( char *& c );
    static KIND         parse_kind( char *& c );
    static uint64_t     parse_addr( char *& c );
    static T            parse_int( char *& c );
    static FLT          parse_flt( char *& c );
};

//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
// 
// IMPLEMENTATION  IMPLEMENTATION  IMPLEMENTATION
//
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
//-----------------------------------------------------
static inline void _die( std::string msg )
{
    std::cout << "ERROR: " << msg << "\n";
    exit( 1 );
}

template< typename T, typename FLT >
inline void Analysis<T,FLT>::_skip_junk( char *& c )
{
    // skip spaces, '(' and ','
    for( ;; )
    {
        char ch = *c;
        if ( ch == '\0' ) return;
        if ( ch == ' ' || ch == ',' || ch == '(' ) {
            c++;
        } else {
            return;
        }
    }
}

template< typename T, typename FLT >
inline std::string Analysis<T,FLT>::parse_name( char *& c )
{
    // read string of letters, "::" and numbers
    _skip_junk( c );
    std::string name = "";
    for( ;; )
    {
        char ch = *c;
        if ( ch == ':' || ch == '_' || ch == '-' || ch == '.' || 
             (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ) {
            name += ch;
            c++;
        } else {
            break;
        }
    }

    cassert( name.size() > 0, "could not parse a name" );
    return name;
}

template< typename T, typename FLT >
inline typename Analysis<T,FLT>::KIND Analysis<T,FLT>::parse_kind( char *& c )
{
    std::string name = parse_name( c );

    if ( name == "cordic_constructed" ) return KIND::cordic_constructed;
    if ( name == "cordic_destructed" )  return KIND::cordic_destructed;
    if ( name == "enter" )              return KIND::enter;
    if ( name == "leave" )              return KIND::leave;
    if ( name == "constructed" )        return KIND::constructed;
    if ( name == "destructed" )         return KIND::destructed;
    if ( name == "op1" )                return KIND::op1;
    if ( name == "op1i" )               return KIND::op1i;
    if ( name == "op1f" )               return KIND::op1f;
    if ( name == "op2" )                return KIND::op2;
    if ( name == "op2i" )               return KIND::op2i;
    if ( name == "op2f" )               return KIND::op2f;
    if ( name == "op3" )                return KIND::op3;
    if ( name == "op4" )                return KIND::op4;
    return KIND(-1);
}

template< typename T, typename FLT >
inline uint64_t Analysis<T,FLT>::parse_addr( char *& c )
{
    std::string addr_s = parse_name( c );
    char * end;
    return std::strtoull( addr_s.c_str(), &end, 16 );
}

template< typename T, typename FLT >
inline T Analysis<T,FLT>::parse_int( char *& c )
{
    std::string int_s = parse_name( c );
    return std::atoi( int_s.c_str() );
}

template< typename T, typename FLT >
inline FLT Analysis<T,FLT>::parse_flt( char *& c )
{
    std::string flt_s = parse_name( c );
    return std::atof( flt_s.c_str() );
}

template< typename T, typename FLT > inline void Analysis<T,FLT>::val_stack_push( const ValInfo& info )
{
    cassert( val_stack_cnt < VAL_STACK_CNT_MAX, "depth of val_stack exceeded" );
    val_stack[val_stack_cnt++] = info;
}

template< typename T, typename FLT >
inline typename Analysis<T,FLT>::ValInfo Analysis<T,FLT>::val_stack_pop( void )
{
    cassert( val_stack_cnt > 0, "can't pop an empty val_stack" );
    return val_stack[--val_stack_cnt];
}

template< typename T, typename FLT >
Analysis<T,FLT>::Analysis( std::string file_name )
{
    in_text = file_name == "";
    if ( in_text ) {
        in = &std::cin;
    }

    // set up ops map
    for( uint32_t o = 0; o < Cordic<T,FLT>::OP_cnt; o++ )
    {
        std::string name = Cordic<T,FLT>::op_to_str( o );
        ops[name] = OP(o);
    }

    // set up kinds map
    kinds["cordic_constructed"] = KIND::cordic_constructed;
    kinds["cordic_destructed"]  = KIND::cordic_destructed;
    kinds["enter"]              = KIND::enter;
    kinds["leave"]              = KIND::leave;
    kinds["constructed"]        = KIND::constructed;
    kinds["destructed"]         = KIND::destructed;
    kinds["op1"]                = KIND::op1;
    kinds["op2"]                = KIND::op2;
    kinds["op2i"]               = KIND::op2i;
    kinds["op2f"]               = KIND::op2f;
    kinds["op3"]                = KIND::op3;
    kinds["op4"]                = KIND::op4;

    // assume parsing text at this point
    std::string line;
    char cs[1024];
    val_stack_cnt = 0;
    while( std::getline( *in, line ) )
    {
        if ( debug ) std::cout << line << "\n";
        strcpy( cs, line.c_str() );
        char * c = cs;
        const KIND kind = parse_kind( c );
        switch( kind )
        {
            case KIND::cordic_constructed:
            {
                CordicInfo info;
                uint64_t cordic = parse_addr( c );
                info.is_alive   = true;
                info.int_w      = parse_int( c );
                info.frac_w     = parse_int( c );
                info.n          = parse_int( c );
                auto it = cordics.find( cordic );
                cassert( it == cordics.end() || !it->second.is_alive, "Cordic reconstructed before previous was destructed" );
                cordics[cordic] = info;
                break;
            }

            case KIND::cordic_destructed:
            {
                uint64_t cordic = parse_addr( c );
                auto it = cordics.find( cordic );
                cassert( it != cordics.end() && it->second.is_alive, "Cordic destructed before being constructed" );
                it->second.is_alive = false;
                break;
            }

            case KIND::enter:
            {
                std::string name = parse_name( c );
                auto it = funcs.find( name );
                if ( it == funcs.end() ) {
                    FuncInfo info;
                    info.func_i = funcs.size();
                    funcs[name] = info;
                    it = funcs.find( name );
                } 
                FrameInfo frame;
                frame.func_i = it->second.func_i;
                stack.push_back( frame );
                break;
            }

            case KIND::leave:
            {
                std::string name = parse_name( c );
                auto it = funcs.find( name );
                cassert( it != funcs.end(), "leave should have found function " + name );
                cassert( stack.size() > 0, "trying to leave routine " + name + " when stack is already empty" );
                FrameInfo& frame = stack[stack.size()-1];
                cassert( frame.func_i == it->second.func_i, "trying to leave a routine that's not at the top of the stack" );
                stack.pop_back();
                break;
            }

            case KIND::constructed:
            {
                ValInfo info;
                info.is_alive    = true;
                info.is_assigned = false;
                info.is_constant = false;
                uint64_t val     = parse_addr( c );
                uint64_t cordic  = parse_addr( c );
                if ( cordic != 0 ) {
                    auto cit = cordics.find( cordic );
                    cassert( cit != cordics.end() && cit->second.is_alive, "val constructed using unknown cordic" );
                    info.cordic_i = cit->second.cordic_i;
                } else {
                    info.cordic_i = size_t(-1);
                }
                auto vit = vals.find( val );
                if ( vit == vals.end() ) {
                    vals[val] = info;
                } else {
                    //cassert( !vit->second.is_alive, "val constructed before previous was desctructed" );
                    vit->second = info;
                }
                break;
            }

            case KIND::destructed:
            {
                uint64_t val = parse_addr( c );
                auto it = vals.find( val );
                cassert( it != vals.end() && it->second.is_alive, "val destructed before being constructed" );
                it->second.is_alive = false;
                break;
            }

            case KIND::op1:
            case KIND::op2:
            case KIND::op3:
            case KIND::op4:
            {
                std::string name = parse_name( c );
                OP op = ops[name];
                uint32_t opnd_cnt = uint32_t(kind) - uint32_t(KIND::op1) + 1;
                uint64_t opnd[4];
                for( uint32_t i = 0; i < opnd_cnt; i++ )
                {
                    opnd[i] = parse_addr( c );
                    auto it = vals.find( opnd[i] );
                    cassert( it != vals.end() && it->second.is_alive, name + " opnd[" + std::to_string(i) + "] does not exist" );
                    if ( i != 0 || op != OP::assign ) {
                        cassert( it != vals.end() && it->second.is_alive, name + " opnd[" + std::to_string(i) + "] does not exist" );
                        cassert( it->second.is_assigned, name + " opnd[" + std::to_string(i) + "] used when not previously assigned" );
                        if ( i == 1 && op == OP::assign ) vals[opnd[0]] = it->second;
                    }
                }
                
                // push result if not assign
                if ( op != OP::assign ) {
                    ValInfo val;
                    val.is_alive    = true;
                    val.is_assigned = true;
                    val.is_constant = false;
                    val_stack_push( val );
                }
                break;
            }

            case KIND::op1i:
            {
                std::string name = parse_name( c );
                _die( "should not have gotten op1i " + name );
                break;
            }

            case KIND::op1f:
            {
                // push constant
                std::string name = parse_name( c );
                OP op = ops[name];
                cassert( op == OP::push_constant, "op1f allowed only for make_constant" );
                ValInfo val;
                val.is_alive    = true;
                val.is_assigned = true;
                val.is_constant = true;
                val.constant    = parse_flt( c );
                val_stack_push( val );
                break;
            }

            case KIND::op2i:
            {
                std::string name = parse_name( c );
                OP op = ops[name];
                cassert( op == OP::lshift || op == OP::rshift || op == OP::pop_value, "op2i allowed only for shifts" );
                uint64_t opnd0 = parse_addr( c );
                T        opnd1 = parse_int( c );
                auto it = vals.find( opnd0 );
                cassert( it != vals.end() && it->second.is_alive, name + " opnd[0] does not exist" );
                switch( op )
                {
                    case OP::pop_value:
                    {
                        // pop result
                        it->second  = val_stack_pop();
                        it->second.encoded = opnd1;
                        break;
                    }

                    default:
                    {
                        // push result
                        ValInfo val;
                        val.is_alive    = true;
                        val.is_assigned = true;
                        val.is_constant = false;
                        val.encoded     = opnd1;
                        val_stack_push( val );
                        break;
                    }
                }
                break;
            }

            case KIND::op2f:
            {
                // push result
                std::string name = parse_name( c );
                OP op = ops[name];
                uint64_t opnd0 = parse_addr( c );   
                FLT      opnd1 = parse_flt( c );   
                auto it = vals.find( opnd0 );
                cassert( it != vals.end() && it->second.is_alive, name + " opnd[0] does not exist" );
                cassert( it->second.is_assigned,                  name + " opnd[0] is used before being assigned" );
                ValInfo val;
                val.is_alive    = true;
                val.is_assigned = true;
                val.is_constant = false;
                val_stack_push( val );
                break;
            }

            default:
            {
                continue;
            }
        }
    }
}

template< typename T, typename FLT >
Analysis<T,FLT>::~Analysis()
{
}

#endif

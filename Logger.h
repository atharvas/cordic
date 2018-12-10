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
// Logger.h - class for logging operations
//
#ifndef _Logger_h
#define _Logger_h

#include <string>
#include <cmath>
#include <iostream>

template< typename T=int64_t, typename FLT=double >
class Logger
{
public:
    typedef std::string (*op_to_str_fn_t)( uint16_t op );
    typedef std::string (*t_to_str_fn_t)(  const T& x );

    Logger( op_to_str_fn_t op_to_str,
            t_to_str_fn_t  t_to_str,
            std::string    file_name = "" );       // "" means text output to std::cout
    ~Logger();

    // log construction/destruction of Cordic objects
    virtual void cordic_constructed( const void * cordic, uint32_t int_w, uint32_t frac_w, uint32_t n );
    virtual void cordic_destructed(  const void * cordic );

    // log construction/destruction of T values
    virtual void constructed( const T * v, void * cordic );
    virtual void destructed(  const T * v );

    // log operations
    //
    virtual void op1( uint16_t op, const T * opnd1 );
    virtual void op2( uint16_t op, const T * opnd1, const T * opnd2 );
    virtual void op3( uint16_t op, const T * opnd1, const T * opnd2, const T * opnd3 );

private:
    op_to_str_fn_t      op_to_str;
    t_to_str_fn_t       t_to_str;
    std::ostream&       out;
    bool                out_text;
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
template< typename T, typename FLT >
Logger<T,FLT>::Logger( op_to_str_fn_t _op_to_str,
                       t_to_str_fn_t  _t_to_str,
                       std::string    file_name )
{
    op_to_str = _op_to_str;
    t_to_str  = _t_to_str;
    out_text  = file_name == "";
    if ( out_text ) {
        out = std::cout;
    }
}

template< typename T, typename FLT >
Logger<T,FLT>::~Logger()
{
}

template< typename T, typename FLT >
inline void Logger<T,FLT>::cordic_constructed( const void * cordic, uint32_t int_w, uint32_t frac_w, uint32_t n )
{
    if ( out_text ) {
        out << "cordic_constructed( " << cordic << ", " << int_w << ", " << frac_w << ", " << n << " );\n";
    }
}

template< typename T, typename FLT >
inline void Logger<T,FLT>::cordic_destructed(  const void * cordic )
{
    if ( out_text ) {
        out << "cordic_destructed( " << cordic << " );\n";
    }
}

template< typename T, typename FLT >
inline void Logger<T,FLT>::constructed( const T * v, void * cordic )
{
    if ( out_text ) {
        out << "constructed( " << v << ", "  << cordic << " );\n";
    }
}

template< typename T, typename FLT >
inline void Logger<T,FLT>::destructed(  const T * v )
{
    if ( out_text ) {
        out << "constructed( " << v << " );\n";
    }
}

template< typename T, typename FLT >
inline void Logger<T,FLT>::op1( uint16_t op, const T * opnd1 )
{
    if ( out_text ) {
        out << "op1( " << op_to_str( op ) << ", " << opnd1 << " );\n";
    }
}

template< typename T, typename FLT >
inline void Logger<T,FLT>::op2( uint16_t op, const T * opnd1, const T * opnd2 )
{
    if ( out_text ) {
        out << "op2( " << op_to_str( op ) << ", " << opnd1 << ", " << opnd2 << " );\n";
    }
}

template< typename T, typename FLT >
inline void Logger<T,FLT>::op3( uint16_t op, const T * opnd1, const T * opnd2, const T * opnd3 )
{
    if ( out_text ) {
        out << "op2( " << op_to_str( op ) << ", " << opnd1 << ", " << opnd2 << ", " << opnd3 << " );\n";
    }
}

#endif

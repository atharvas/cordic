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
#include "Cordic.h"

#ifdef DEBUG_LEVEL
static constexpr uint32_t debug = DEBUG_LEVEL;
#else
static constexpr uint32_t debug = 0;
#endif

//-----------------------------------------------------
// INTERNAL IMPL STRUCTURE 
//-----------------------------------------------------
template< typename T, typename FLT >
struct Cordic<T,FLT>::Impl
{
    uint32_t                    int_w;
    uint32_t                    frac_w;
    bool                        do_reduce;
    uint32_t                    nc;
    uint32_t                    nh;
    uint32_t                    nl;

    T                           maxint;
    T                           zero;
    T                           one;
    T                           quarter;

    std::unique_ptr<T[]>        circular_atan;                  // circular atan values
    T                           circular_gain;                  // circular gain
    T                           circular_one_over_gain;         // circular 1/gain

    std::unique_ptr<T[]>        hyperbolic_atanh;               // hyperbolic atanh values
    T                           hyperbolic_gain;                // hyperbolic gain
    T                           hyperbolic_one_over_gain;       // hyperbolic 1/gain

    std::unique_ptr<T[]>        linear_pow2;                    // linear 2^(-i) values

    T                           log2;                           // log(2)
    T                           log10;                          // log(10)
    T                           log10_div_e;                    // log(10) / e

    std::unique_ptr<T[]>        reduce_angle_addend;            // for each possible integer value, an addend to help normalize
    std::unique_ptr<uint32_t[]> reduce_angle_quadrant;          // 00,01,02,03
    std::unique_ptr<FLT[]>      reduce_exp_factor;              // for each possible integer value, exp(i)
    std::unique_ptr<T[]>        reduce_log_addend;              // for each possible lshift value, log( 1 << lshift )

    inline void                 do_lshift( T& x, int32_t lshift ) const
    {
        if ( lshift > 0 ) {
            x <<= lshift;
        } else if ( lshift < 0 ) {
            x >>= -lshift;
        }
    }
};

//-----------------------------------------------------
// Constructor
//-----------------------------------------------------
template< typename T, typename FLT >
Cordic<T,FLT>::Cordic( uint32_t int_w, uint32_t frac_w, bool do_reduce, uint32_t nc, uint32_t nh, uint32_t nl )
{
    if ( nc == 0 ) nc = frac_w;
    if ( nh == 0 ) nh = frac_w;
    if ( nl == 0 ) nl = frac_w;

    impl = std::make_unique<Impl>();

    impl->int_w   = int_w;
    impl->frac_w  = frac_w;
    impl->do_reduce = do_reduce;
    impl->maxint  = (T(1) << int_w) - 1;
    impl->zero    = 0;
    impl->one     = T(1) << frac_w;
    impl->quarter = T(1) << (frac_w-2);
    impl->nc      = nc;
    impl->nh      = nh;
    impl->nl      = nl;

    impl->circular_atan    = std::unique_ptr<T[]>( new T[nc+1] );
    impl->hyperbolic_atanh = std::unique_ptr<T[]>( new T[nh+1] );
    impl->linear_pow2      = std::unique_ptr<T[]>( new T[nl+1] );

    // compute atan/atanh table and gains in high-resolution floating point
    FLT pow2  = 1.0;
    FLT gain_inv  = 1.0;
    FLT gainh_inv = 1.0;
    uint32_t n_max = (nh > nc)    ? nh : nc;
             n_max = (nl > n_max) ? nl : n_max;
    uint32_t next_dup_i = 4;     // for hyperbolic 
    for( uint32_t i = 0; i <= n_max; i++ )
    {
        FLT a  = std::atan( pow2 );
        if ( i <= nc ) impl->circular_atan[i]    = T( a    * FLT( T(1) << T(frac_w) ) );
        if ( i <= nl ) impl->linear_pow2[i]      = T( pow2 * FLT( T(1) << T(frac_w) ) );
        FLT ah = std::atanh( pow2 );
        //FLT ah = 0.5 * std::log( (1.0+pow2) / (1-pow2) );
        if ( i <= nh ) impl->hyperbolic_atanh[i] = T( ah   * FLT( T(1) << T(frac_w) ) );
        if ( i <= nc ) gain_inv *= std::cos( a );

        if ( i != 0 && i <= nh ) {
            gainh_inv *= std::cosh( ah );
            if ( i == next_dup_i ) {
                // for hyperbolic, we must duplicate iterations 4, 13, 40, 121, ..., 3*i+1
                gainh_inv *= std::cosh( ah );
                next_dup_i = 3*i + 1;
            }
        }
        pow2 /= 2.0;

        if ( debug ) printf( "i=%2d a=%30.27g ah=%30.27g y=%30.27g gain_inv=%30.27g gainh_inv=%30.27g\n", i, double(a), double(ah), double(pow2), double(gain_inv), double(gainh_inv) );

    }

    // now convert those last two to fixed-point
    impl->circular_gain            = T( 1.0/gain_inv  * FLT( T(1) << T(frac_w) ) );
    impl->circular_one_over_gain   = T(     gain_inv  * FLT( T(1) << T(frac_w) ) );
    impl->hyperbolic_gain          = T( 1.0/gainh_inv * FLT( T(1) << T(frac_w) ) );
    impl->hyperbolic_one_over_gain = T(     gainh_inv * FLT( T(1) << T(frac_w) ) );

    // constants
    impl->log2        = T( std::log( FLT( 2  ) )       * FLT( T(1) << T(frac_w) ) );
    impl->log10       = T( std::log( FLT( 10 ) )       * FLT( T(1) << T(frac_w) ) );
    impl->log10_div_e = T( std::log( FLT( 10 ) / M_E ) * FLT( T(1) << T(frac_w) ) );

    // construct LUT used by reduce_angle()
    cassert( int_w < 14 && "too many cases to worry about" );
    uint32_t N = 1 << (1+int_w);
    T *        addend   = new T[N];
    uint32_t * quadrant = new uint32_t[N];
    impl->reduce_angle_addend   = std::unique_ptr<T[]>( addend );
    impl->reduce_angle_quadrant = std::unique_ptr<uint32_t[]>( quadrant );
    const FLT PI       = M_PI;
    const FLT PI_DIV_2 = PI / 2.0;
    for( T i = 0; i <= maxint(); i++ )
    {
        FLT cnt = FLT(i) / PI_DIV_2;
        T   cnt_i = cnt;
        if ( debug ) std::cout << "cnt_i=" << cnt_i << "\n";
        FLT add_f = FLT(cnt_i) * PI_DIV_2;
        if ( i > 0 ) add_f = -add_f;
        addend[i]   = to_fp( add_f );
        quadrant[i] = cnt_i % 4;
        if ( debug ) std::cout << "reduce_angle_arg LUT: cnt_i=" << cnt_i << " addend[" << i << "]=" << to_flt(addend[i]) << " quadrant=" << quadrant[i] << "\n";
    }

    // construct LUT used by reduce_exp_arg()
    FLT * factor = new FLT[N];
    impl->reduce_exp_factor = std::unique_ptr<FLT[]>( factor );
    for( T i = 0; i <= maxint(); i++ )
    {
        factor[i] = std::exp(FLT(i));
        if ( debug ) std::cout << "reduce_exp_arg LUT: factor[" << i << "]=" << factor[i] << "\n";
    }

    // construct LUT used by reduce_log_arg()
    addend = new T[frac_w+int_w];
    impl->reduce_log_addend = std::unique_ptr<T[]>( addend );
    for( int32_t i = -frac_w; i <= int32_t(int_w); i++ )
    {
        double addend_f = std::log( std::pow( 2.0, double( i ) ) );
        addend[frac_w+i] = to_fp( addend_f );
        if ( debug ) std::cout << "addend[]=0x" << std::hex << addend[frac_w+i] << "\n" << std::dec;
        if ( debug ) std::cout << "reduce_log_arg LUT: addend[" << i << "]=" << to_flt(addend[frac_w+i]) << " addend_f=" << addend_f << "\n";
    }
}

template< typename T, typename FLT >
Cordic<T,FLT>::~Cordic( void )
{
    impl = nullptr;
}

//-----------------------------------------------------
// Constants
//-----------------------------------------------------
template< typename T, typename FLT >
uint32_t Cordic<T,FLT>::int_w( void ) const
{
    return impl->int_w;
}

template< typename T, typename FLT >
uint32_t Cordic<T,FLT>::frac_w( void ) const
{
    return impl->frac_w;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::maxint( void ) const
{
    return impl->maxint;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::zero( void ) const
{
    return impl->zero;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::one( void ) const
{
    return impl->one;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::quarter( void ) const
{
    return impl->quarter;
}

template< typename T, typename FLT >
uint32_t Cordic<T,FLT>::n_circular( void ) const
{
    return impl->nc;
}

template< typename T, typename FLT >
uint32_t Cordic<T,FLT>::n_hyperbolic( void ) const
{
    return impl->nh;
}

template< typename T, typename FLT >
uint32_t Cordic<T,FLT>::n_linear( void ) const
{
    return impl->nl;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::gain( void ) const
{
    return impl->circular_gain;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::gainh( void ) const
{
    return impl->hyperbolic_gain;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::one_over_gain( void ) const
{
    return impl->circular_one_over_gain;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::one_over_gainh( void ) const
{
    return impl->hyperbolic_one_over_gain;
}

//-----------------------------------------------------
// Conversion
//-----------------------------------------------------
template< typename T, typename FLT >
T Cordic<T,FLT>::to_fp( FLT _x ) const
{
    FLT x = _x;
    bool is_neg = x < 0.0;
    if ( is_neg ) x = -x;
    T x_fp = x * FLT(T(1) << T(frac_w()));
    //std::cout << "to_fp: abs(x)=" << x << " x_fp=0x" << std::hex << x_fp << std::dec << " to_flt=" << to_flt(x_fp) << "\n";
    if ( is_neg ) x_fp = -x_fp;
    return x_fp;
}

template< typename T, typename FLT >
FLT Cordic<T,FLT>::to_flt( const T& _x ) const
{
    T x = _x;
    bool is_neg = x < 0;
    if ( is_neg ) x = -x;
    FLT x_f = FLT( x ) / std::pow( 2.0, frac_w() );
    //std::cout << "to_flt: abs(x)=0x" << std::hex << x << " x_f=" << std::dec << x_f << "\n";
    if ( is_neg ) x_f = -x_f;
    return x_f;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::make_fp( bool sign, T i, T f )
{
    cassert( i >= 0 && i <= maxint()               && "make_fp integer part must be in range 0 .. maxint()" );
    cassert( f >= 0 && f <= ((T(1) << frac_w())-1) && "make_fp fractional part must be in range 0 .. (1 << frac_w)-1" );

    return (T(sign) << (int_w() + frac_w())) |
           (T(i)    << frac_w())             |
           (T(f)    << 0);
}

//-----------------------------------------------------
// The CORDIC Functions
//-----------------------------------------------------
template< typename T, typename FLT >
void Cordic<T,FLT>::circular_rotation( const T& x0, const T& y0, const T& z0, T& x, T& y, T& z ) const
{
    //-----------------------------------------------------
    // d = (z >= 0) ? 1 : -1
    // xi = x - d*(y >> i)
    // yi = y + d*(x >> i)
    // zi = z - d*arctan(2^(-i))
    //-----------------------------------------------------
    x = x0;
    y = y0;
    z = z0;
    uint32_t n = impl->nc;
    for( uint32_t i = 0; i <= n; i++ )
    {
        T xi;
        T yi;
        T zi;
        if ( debug ) printf( "circular_rotation: i=%d xyz=[%f,%f,%f] test=%d\n", i, to_flt(x), to_flt(y), to_flt(z), int(z >= zero()) );
        if ( z >= zero() ) {
            xi = x - (y >> i);
            yi = y + (x >> i);
            zi = z - impl->circular_atan[i];
        } else {
            xi = x + (y >> i);
            yi = y - (x >> i);
            zi = z + impl->circular_atan[i];
        }
        x = xi;
        y = yi;
        z = zi;
    }
}

template< typename T, typename FLT >
void Cordic<T,FLT>::circular_vectoring( const T& x0, const T& y0, const T& z0, T& x, T& y, T& z ) const
{
    //-----------------------------------------------------
    // d = (xy < 0) ? 1 : -1
    // xi = x - d*(y >> i)
    // yi = y + d*(x >> i)
    // zi = z - d*arctan(2^(-i))
    //-----------------------------------------------------
    x = x0;
    y = y0;
    z = z0;
    uint32_t n = impl->nc;
    for( uint32_t i = 0; i <= n; i++ )
    {
        T xi;
        T yi;
        T zi;
        if ( debug ) printf( "circular_vectoring: i=%d xyz=[%f,%f,%f] test=%d\n", i, to_flt(x), to_flt(y), to_flt(z), int((x < zero()) != (y < zero())) );
        if ( (x < zero()) != (y < zero()) ) {
            xi = x - (y >> i);
            yi = y + (x >> i);
            zi = z - impl->circular_atan[i];
        } else {
            xi = x + (y >> i);
            yi = y - (x >> i);
            zi = z + impl->circular_atan[i];
        }
        x = xi;
        y = yi;
        z = zi;
    }
}

template< typename T, typename FLT >
void Cordic<T,FLT>::hyperbolic_rotation( const T& x0, const T& y0, const T& z0, T& x, T& y, T& z ) const
{
    //-----------------------------------------------------
    // d = (z >= 0) ? 1 : -1
    // xi = x - d*(y >> i)
    // yi = y + d*(x >> i)
    // zi = z - d*arctanh(2^(-i))
    //-----------------------------------------------------
    x = x0;
    y = y0;
    z = z0;
    uint32_t n = impl->nh;
    uint32_t next_dup_i = 4;     
    for( uint32_t i = 1; i <= n; i++ )
    {
        T xi;
        T yi;
        T zi;
        if ( debug ) printf( "hyperbolic_rotation: i=%d xyz=[%f,%f,%f] test=%d\n", i, to_flt(x), to_flt(y), to_flt(z), int(z >= zero()) );
        if ( z >= zero() ) {
            xi = x + (y >> i);
            yi = y + (x >> i);
            zi = z - impl->hyperbolic_atanh[i];
        } else {
            xi = x - (y >> i);
            yi = y - (x >> i);
            zi = z + impl->hyperbolic_atanh[i];
        }
        x = xi;
        y = yi;
        z = zi;

        if ( i == next_dup_i ) {
            // for hyperbolic, we must duplicate iterations 4, 13, 40, 121, ..., 3*i+1
            next_dup_i = 3*i + 1;
            i--;
        }
    }
}

template< typename T, typename FLT >
void Cordic<T,FLT>::hyperbolic_vectoring( const T& x0, const T& y0, const T& z0, T& x, T& y, T& z ) const
{
    //-----------------------------------------------------
    // d = (xy < 0) ? 1 : -1
    // xi = x - d*(y >> i)
    // yi = y + d*(x >> i)
    // zi = z - d*arctanh(2^(-i))
    //-----------------------------------------------------
    x = x0;
    y = y0;
    z = z0;
    uint32_t n = impl->nh;
    uint32_t next_dup_i = 4;     
    for( uint32_t i = 1; i <= n; i++ )
    {
        T xi;
        T yi;
        T zi;
        if ( (x < zero()) != (y < zero()) ) {
            xi = x + (y >> i);
            yi = y + (x >> i);
            zi = z - impl->hyperbolic_atanh[i];
        } else {
            xi = x - (y >> i);
            yi = y - (x >> i);
            zi = z + impl->hyperbolic_atanh[i];
        }
        x = xi;
        y = yi;
        z = zi;

        if ( i == next_dup_i ) {
            // for hyperbolic, we must duplicate iterations 4, 13, 40, 121, ..., 3*i+1
            next_dup_i = 3*i + 1;
            i--;
        }
    }
}

template< typename T, typename FLT >
void Cordic<T,FLT>::linear_rotation( const T& x0, const T& y0, const T& z0, T& x, T& y, T& z ) const
{
    //-----------------------------------------------------
    // d = (z >= 0) ? 1 : -1
    // xi = x 
    // yi = y + d*(x >> i)
    // zi = z - d*2^(-i)
    //-----------------------------------------------------
    x = x0;
    y = y0;
    z = z0;
    uint32_t n = impl->nl;
    for( uint32_t i = 0; i <= n; i++ )
    {
        if ( debug ) printf( "linear_rotation: i=%d xyz=[%f,%f,%f] test=%d\n", i, to_flt(x), to_flt(y), to_flt(z), int(z >= zero()) );
        T yi;
        T zi;
        if ( z >= zero() ) {
            yi = y + (x >> i);
            zi = z - impl->linear_pow2[i];
        } else {
            yi = y - (x >> i);
            zi = z + impl->linear_pow2[i];
        }
        y = yi;
        z = zi;
    }
}

template< typename T, typename FLT >
void Cordic<T,FLT>::linear_vectoring( const T& x0, const T& y0, const T& z0, T& x, T& y, T& z ) const
{
    //-----------------------------------------------------
    // d = (y <= 0) ? 1 : -1
    // xi = x
    // yi = y + d*(x >> i)
    // zi = z - d*2^(-i)
    //-----------------------------------------------------
    x = x0;
    y = y0;
    z = z0;
    uint32_t n = impl->nl;
    for( uint32_t i = 0; i <= n; i++ )
    {
        if ( debug ) printf( "linear_vectoring: i=%d xyz=[%f,%f,%f] test=%d\n", i, to_flt(x), to_flt(y), to_flt(z), int((x < zero()) != (y < zero())) );
        T yi;
        T zi;
        if ( (x < zero()) != (y < zero()) ) {
            yi = y + (x >> i);
            zi = z - impl->linear_pow2[i];
        } else {
            yi = y - (x >> i);
            zi = z + impl->linear_pow2[i];
        }
        y = yi;
        z = zi;
    }
}

template< typename T, typename FLT >
T Cordic<T,FLT>::mad( const T& _x, const T& _y, const T addend, bool do_reduce ) const
{
    T x = _x;
    T y = _y;
    cassert( x >= 0 && "mad x must be non-negative" );
    cassert( y >= 0 && "mad y must be non-negative" );
    cassert( do_reduce || addend >= 0 && "mad addend must be non-negative" );
    int32_t x_lshift;
    int32_t y_lshift;
    if ( do_reduce ) reduce_mul_args( x, y, x_lshift, y_lshift );

    T xx, yy, zz;
    linear_rotation( x, do_reduce ? zero() : addend, y, xx, yy, zz );
    if ( do_reduce ) impl->do_lshift( yy, x_lshift + y_lshift );
    if ( do_reduce ) yy += addend;
    if ( debug ) std::cout << "mad: x_orig=" << to_flt(x) << " y_orig=" << to_flt(y) << " addend=" << to_flt(addend) << " yy=" << to_flt(yy) << 
                                  " x_lshift=" << x_lshift << " y_lshift=" << y_lshift << "\n";
    return yy;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::mad( const T& _x, const T& _y, const T addend ) const
{
    return mad( _x, _y, addend, impl->do_reduce );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::mul( const T& x, const T& y ) const
{
    return mad( x, y, zero() );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::mul( const T& x, const T& y, bool do_reduce ) const
{
    return mad( x, y, zero(), do_reduce );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::dad( const T& _y, const T& _x, const T addend, bool do_reduce ) const
{
    T x = _x;
    T y = _y;
    cassert( y >= 0  && "dad y must be non-negative" );
    cassert( x > 0 && "dad x must be positive" );
    cassert( do_reduce || addend >= 0 && "dad addend must be non-negative" );
    int32_t x_lshift;
    int32_t y_lshift;
    if ( do_reduce ) reduce_div_args( x, y, x_lshift, y_lshift );

    T xx, yy, zz;
    linear_vectoring( x, y, do_reduce ? zero() : addend, xx, yy, zz );
    if ( debug ) std::cout << "dad: zz_preshift=" << to_flt(zz) << "\n";
    if ( do_reduce ) impl->do_lshift( zz, y_lshift-x_lshift );
    if ( debug ) std::cout << "dad: zz_postshift=" << to_flt(zz) << "\n";
    if ( do_reduce ) zz += addend;
    if ( debug ) std::cout << "dad: x_orig=" << to_flt(x) << " y_orig=" << to_flt(y) << " addend=" << to_flt(addend) << " zz_final=" << to_flt(zz) << 
                                  " x_lshift=" << x_lshift << " y_lshift=" << y_lshift << "\n";
    return zz;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::dad( const T& _y, const T& _x, const T addend ) const
{
    return dad( _y, _x, addend, impl->do_reduce );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::div( const T& y, const T& x ) const
{
    return dad( y, x, zero() );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::div( const T& y, const T& x, bool do_reduce ) const
{
    return dad( y, x, zero(), do_reduce );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::one_over( const T& x ) const
{
    return div( one(), x );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::sqrt( const T& _x ) const
{ 
    T x = _x;
    cassert( x >= 0 && "sqrt x must be non-negative" );
    int32_t x_lshift;
    if ( impl->do_reduce ) reduce_sqrt_arg( x, x_lshift );

    // sqrt( (x+0.25)^2 - (x-0.25)^2 ) = normh( x+0.25, x-0.25 )
    T n = normh( x + quarter(), x - quarter() );
    if ( impl->do_reduce ) impl->do_lshift( n, x_lshift );
    return n;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::one_over_sqrt( const T& _x ) const
{ 
    T x = _x;
    cassert( x > 0 && "one_over_sqrt x must be positive" );
    int32_t x_lshift;
    if ( impl->do_reduce ) reduce_sqrt_arg( x, x_lshift );

    // do basic 1/sqrt for now
    // try pow( x, -0.5 ) later
    T n = div( one(), sqrt( x ), false );
    if ( impl->do_reduce ) n >>= x_lshift;
    return n;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::exp( const T& _x ) const
{ 
    T x = _x;
    cassert( x >= 0 && "exp x must be non-negative" );
    T factor;
    if ( impl->do_reduce ) reduce_exp_arg( M_E, x, factor );

    T xx, yy, zz;
    hyperbolic_rotation( one_over_gainh(), one_over_gainh(), x, xx, yy, zz );
    if ( impl->do_reduce ) xx = mul( xx, factor, true );
    return xx;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::pow( const T& b, const T& x ) const
{ 
    cassert( b >= 0 && "pow b must be non-negative" );
    cassert( x >= 0 && "pow x must be non-negative" );
    return exp( mul( x, log( b, true ) ) );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::powc( const FLT& b, const T& x ) const
{ 
    cassert( b >= 0.0 && "powc b must be non-negative" );
    cassert( x >= 0   && "powc x must be non-negative" );
    const FLT log_b_f = std::log( b );
    cassert( log_b_f >= 0.0 && "powc log(b) must be non-negative" );
    const T   log_b   = to_fp( log_b_f );
    return exp( mul( x, log_b ) );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::pow2( const T& x ) const
{ 
    cassert( x >= 0 && "pow2 x must be non-negative" );
    return powc( 2.0, x );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::pow10( const T& x ) const
{ 
    cassert( x >= 0 && "pow10 x must be non-negative" );
    return powc( 10.0, x );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::log( const T& _x, bool do_reduce ) const
{ 
    T x = _x;
    cassert( x >= 0 && "log x must be non-negative" );
    T addend;
    if ( do_reduce ) reduce_log_arg( x, addend );
    T lg = atanh2( x-one(), x+one(), false ) << 1;
    if ( do_reduce ) lg += addend;
    if ( debug ) std::cout << "log: x_orig=" << to_flt(_x) << " reduced_x=" << to_flt(x) << " log=" << to_flt(lg) << "\n";
    return lg;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::log( const T& _x ) const
{ 
    return log( _x, impl->do_reduce );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::logb( const T& x, const T& b ) const
{ 
    cassert( x >= 0 && "logb x must be non-negative" );
    cassert( b > 0  && "logb b must be positive" );
    return div( log(x), log(b) );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::logc( const T& x, const FLT& b ) const
{ 
    cassert( x >= 0  && "logc x must be non-negative" );
    cassert( b > 0.0 && "logc b must be positive" );
    const FLT  one_over_log_b_f = FLT(1) / std::log( b );
    const T    one_over_log_b   = to_fp( one_over_log_b_f );
          T    log_x            = log( x );
    const bool log_x_sign       = log_x < 0;
    if ( log_x_sign ) log_x = -log_x;
    T z = mul( log_x, one_over_log_b );
    if ( log_x_sign ) z = -z;
    return z;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::log2( const T& x ) const
{ 
    cassert( x >= 0 && "log2 x must be non-negative" );
    return logc( x, 2.0 );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::log10( const T& x ) const
{ 
    cassert( x >= 0 && "log10 x must be non-negative" );
    return logc( x, 10.0 );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::sin( const T& _x ) const
{ 
    T x = _x;
    cassert( x >= 0 && "sin x must be non-negative" );
    uint32_t quadrant;
    if ( impl->do_reduce ) reduce_angle_arg( x, quadrant );

    T xx, yy, zz;
    circular_rotation( one_over_gain(), zero(), x, xx, yy, zz );
    if ( impl->do_reduce ) {
        if ( quadrant&1 )    yy = xx;      // use cos
        if ( quadrant >= 2 ) yy = -yy;
    }
    return yy;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::cos( const T& _x ) const
{ 
    T x = _x;
    cassert( x >= 0 && "cos x must be non-negative" );
    uint32_t quadrant;
    if ( impl->do_reduce ) reduce_angle_arg( x, quadrant );

    T xx, yy, zz;
    circular_rotation( one_over_gain(), zero(), x, xx, yy, zz );
    if ( impl->do_reduce ) {
        if ( quadrant&1 )    xx = yy;      // use sin
        if ( quadrant == 1 || quadrant == 2 ) xx = -xx;
    }
    return xx;
}

template< typename T, typename FLT >
void Cordic<T,FLT>::sin_cos( const T& _x, T& si, T& co ) const             
{ 
    T x = _x;
    cassert( x >= 0 && "sin_cos x must be non-negative" );
    uint32_t quadrant;
    if ( impl->do_reduce ) reduce_angle_arg( x, quadrant );

    T zz;
    circular_rotation( one_over_gain(), zero(), x, co, si, zz );
    if ( impl->do_reduce ) {
        if ( quadrant&1 ) {
            T tmp = co;
            co = si;
            si = tmp;
        }
        if ( quadrant == 1 || quadrant == 2 ) co = -co;
        if ( quadrant >= 2 ) si = -si;
    }
}

template< typename T, typename FLT >
T Cordic<T,FLT>::tan( const T& x ) const
{ 
    cassert( x >= 0 && "tan x must be non-negative" );
    T si, co;
    sin_cos( x, si, co );
    return div( si, co );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::asin( const T& x ) const
{ 
    cassert( x >= 0 && "asin x must be non-negative" );
    return atan2( x, normh( one(), x ) );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::acos( const T& x ) const
{ 
    cassert( x >= 0 && "acos x must be non-negative" );
    return atan2( normh( one(), x ), x );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::atan( const T& x ) const
{ 
    cassert( x >= 0 && "atan x must be non-negative" );
    cassert( !impl->do_reduce && "TODO" );
    T xx, yy, zz;
    circular_vectoring( one(), x, zero(), xx, yy, zz );
    return zz;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::atan2( const T& y, const T& x, bool do_reduce ) const
{ 
    cassert( y >= 0 && "atan2 y must be non-negative" );
    cassert( x > 0  && "atan2 x must be positive" );
    cassert( !do_reduce && "TODO" );
    T xx, yy, zz;
    circular_vectoring( x, y, zero(), xx, yy, zz );
    return zz;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::atan2( const T& y, const T& x ) const
{ 
    return atan2( y, x, impl->do_reduce );
}

template< typename T, typename FLT >
void Cordic<T,FLT>::polar_to_rect( const T& r, const T& a, T& x, T& y ) const
{
    cassert( r >= 0 && "polar_to_rect r must be non-negative" );
    cassert( a >= 0 && "polar_to_rect a must be non-negative" );
    cassert( !impl->do_reduce && "TODO" );
    T xx, yy, zz;
    circular_rotation( r, zero(), a, xx, yy, zz );
    x = mul( xx, one_over_gain() );  // TODO: multiply r once to avoid these?
    y = mul( yy, one_over_gain() );
}

template< typename T, typename FLT >
void Cordic<T,FLT>::rect_to_polar( const T& _x, const T& _y, T& r, T& a ) const
{
    T x = _x;
    T y = _y;
    cassert( x >= 0 && "rect_to_polar x must be non-negative" );
    cassert( y >= 0 && "rect_to_polar y must be non-negative" );
    int32_t lshift;
    if ( impl->do_reduce ) reduce_norm_args( x, y, lshift );

    T rr;
    T yy;
    circular_vectoring( x, y, zero(), rr, yy, a );
    if ( impl->do_reduce ) impl->do_lshift( rr, lshift );
    r = mul( rr, one_over_gain() );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::norm( const T& _x, const T& _y ) const
{
    T x = _x;
    T y = _y;
    cassert( x >= 0 && "norm x must be non-negative" );
    cassert( y >= 0 && "norm y must be non-negative" );
    int32_t lshift;
    if ( impl->do_reduce ) reduce_norm_args( x, y, lshift );

    T xx, yy, zz;
    circular_vectoring( x, y, zero(), xx, yy, zz );
    if ( impl->do_reduce ) impl->do_lshift( xx, lshift );
    return mul( xx, one_over_gain() );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::normh( const T& _x, const T& _y ) const
{
    T x = _x;
    T y = _y;
    cassert( x >= 0 && "normh x must be non-negative" );
    cassert( y >= 0 && "normh y must be non-negative" );
    cassert( x >= y && "normh x must >= y" );
    int32_t lshift;
    if ( impl->do_reduce ) reduce_norm_args( x, y, lshift );

    T xx, yy, zz;
    hyperbolic_vectoring( x, y, zero(), xx, yy, zz );
    if ( impl->do_reduce ) impl->do_lshift( xx, lshift );
    return mul( xx, one_over_gainh() );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::sinh( const T& _x ) const
{ 
    T x = _x;
    cassert( x >= 0 && "sinh x must be non-negative" );
    uint32_t quadrant;
    if ( impl->do_reduce ) reduce_angle_arg( x, quadrant );

    T xx, yy, zz;
    hyperbolic_rotation( one_over_gainh(), zero(), x, xx, yy, zz );
    if ( impl->do_reduce ) {
        if ( quadrant&1 )    yy = xx;      // use cos
        if ( quadrant >= 2 ) yy = -yy;
    }
    return yy;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::cosh( const T& _x ) const
{ 
    T x = _x;
    cassert( x >= 0 && "cosh x must be non-negative" );
    uint32_t quadrant;
    if ( impl->do_reduce ) reduce_angle_arg( x, quadrant );

    T xx, yy, zz;
    hyperbolic_rotation( one_over_gainh(), zero(), x, xx, yy, zz );
    if ( impl->do_reduce ) {
        if ( quadrant&1 )    xx = yy;      // use sin
        if ( quadrant == 1 || quadrant == 2 ) xx = -xx;
    }
    return xx;
}

template< typename T, typename FLT >
void Cordic<T,FLT>::sinh_cosh( const T& _x, T& sih, T& coh ) const
{ 
    T x = _x;
    cassert( x >= 0 && "sinh_cosh x must be non-negative" );
    uint32_t quadrant;
    if ( impl->do_reduce ) reduce_angle_arg( x, quadrant );

    T zz;
    hyperbolic_rotation( one_over_gainh(), zero(), x, coh, sih, zz );
    if ( impl->do_reduce ) {
        if ( quadrant&1 ) {
            T tmp = coh;
            coh = sih;
            sih = tmp;
        }
        if ( quadrant == 1 || quadrant == 2 ) coh = -coh;
        if ( quadrant >= 2 ) sih = -sih;
    }
}

template< typename T, typename FLT >
T Cordic<T,FLT>::tanh( const T& x ) const
{ 
    T sih, coh;
    cassert( x >= 0 && "tanh x must be non-negative" );
    sinh_cosh( x, sih, coh );
    return div( sih, coh );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::asinh( const T& x ) const
{ 
    cassert( x >= 0 && "asinh x must be non-negative" );
    return log( x + norm( one(), x ) );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::acosh( const T& x ) const
{ 
    cassert( x >= 0 && "acosh x must be non-negative" );
    return log( x + normh( x, one() ) );
}

template< typename T, typename FLT >
T Cordic<T,FLT>::atanh( const T& x ) const
{ 
    cassert( x >= 0 && "atanh x must be non-negative" );
    cassert( !impl->do_reduce && "TODO" );
    T xx, yy, zz;
    hyperbolic_vectoring( one(), x, zero(), xx, yy, zz );
    return zz;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::atanh2( const T& y, const T& x, bool do_reduce ) const             
{ 
    cassert( y >= 0 && "atanh y must be non-negative" );
    cassert( x >  0 && "atanh x must be positive" );
    cassert( !do_reduce && "TODO" );
    T xx, yy, zz;
    hyperbolic_vectoring( x, y, zero(), xx, yy, zz );
    return zz;
}

template< typename T, typename FLT >
T Cordic<T,FLT>::atanh2( const T& y, const T& x ) const             
{ 
    return atanh2( y, x, impl->do_reduce );
}

template< typename T, typename FLT >
void Cordic<T,FLT>::reduce_arg( T& x, int32_t& x_lshift, bool shift_x, bool normalize ) const
{
    T x_orig = x;
    cassert( x >= 0 );
    T other = T(1) << frac_w();
    x_lshift = 0;
    while( x > other ) 
    {
        x_lshift++;
        if ( shift_x ) {
            x >>= 1;
        } else {
            other <<= 1;
        }
    }
    while( normalize && x < one() )
    {
        x_lshift--;
        if ( shift_x ) {
            x <<= 1;
        } else {
            other >>= 1;
        }
    }
    if ( debug && shift_x ) std::cout << "reduce_arg: x_orig=" << to_flt(x_orig) << " x_reduced=" << to_flt(x) << " x_lshift=" << x_lshift << "\n"; 
}

template< typename T, typename FLT >
void Cordic<T,FLT>::reduce_mul_args( T& x, T& y, int32_t& x_lshift, int32_t& y_lshift ) const
{
    if ( debug ) std::cout << "reduce_mul_args: x_orig=" << to_flt(x) << " y_orig=" << to_flt(y) << "\n";
    reduce_arg( x, x_lshift );
    reduce_arg( y, y_lshift );
}

template< typename T, typename FLT >
void Cordic<T,FLT>::reduce_div_args( T& x, T& y, int32_t& x_lshift, int32_t& y_lshift ) const
{
    if ( debug ) std::cout << "reduce_div_args: x_orig=" << to_flt(x) << " y_orig=" << to_flt(y) << "\n";
    reduce_arg( x, x_lshift, true, true );
    reduce_arg( y, y_lshift );
}

template< typename T, typename FLT >
void Cordic<T,FLT>::reduce_sqrt_arg( T& x, int32_t& x_lshift ) const
{
    //-----------------------------------------------------
    // Reduce without right-shifting x yet.
    // Then round the x_lshift up to even number.
    // And *then* shift.
    //-----------------------------------------------------
    T x_orig = x;
    reduce_arg( x, x_lshift, false );   
    if ( x_lshift & 1 ) x_lshift++;
    x >>= x_lshift;
    if ( debug ) std::cout << "reduce_sqrt_arg: x_orig=" << to_flt(x_orig) << " x_reduced=" << to_flt(x) << " x_lshift=" << x_lshift << "\n"; 
}

template< typename T, typename FLT >
void Cordic<T,FLT>::reduce_exp_arg( FLT b, T& x, T& factor ) const 
{
    //-----------------------------------------------------
    // Assume: x = i + f  (integer plus fraction)
    // exp(x) = exp(i) * exp(f)
    // pow(b,x) = log(b) * exp(x) = [log(b)*exp(i)] * exp(f)
    //
    // exp(i) comes from a pre-built LUT kept in FLT
    // so we can multiply it by log(b) before converting to type T and
    // then multiplying by exp(f).
    //-----------------------------------------------------
    T x_orig = x;
    const FLT * factors_f = impl->reduce_exp_factor.get();
    T   index    = (x >> frac_w()) & maxint();
    FLT factor_f = std::log(b) * factors_f[index];   // could build per-b factors_f[] LUT with multiply already done
    factor       = to_fp( factor_f );
    x           &= (T(1) << frac_w())-T(1); // fraction only
    if ( debug ) std::cout << "reduce_exp_arg: x_orig=" << to_flt(x_orig) << " x_reduced=" << to_flt(x) << " factor=" << factor << "\n"; 
}

template< typename T, typename FLT >
void Cordic<T,FLT>::reduce_log_arg( T& x, T& addend ) const 
{
    //-----------------------------------------------------
    // log(ab) = log(a) + log(b)
    // 
    // Normalize x so that it's in 1.00 .. 2.00.
    // Then addend = log(1 << shift).
    //-----------------------------------------------------
    T x_orig = x;
    int32_t x_lshift;
    reduce_arg( x, x_lshift, true, true );
    const T * addends = impl->reduce_log_addend.get();
    addend = addends[frac_w()+x_lshift];
    if ( debug ) std::cout << "reduce_log_arg: x_orig=" << to_flt(x_orig) << " x_reduced=" << to_flt(x) << " addend=" << to_flt(addend) << "\n"; 
}

template< typename T, typename FLT >
void Cordic<T,FLT>::reduce_norm_args( T& x, T& y, int32_t& lshift ) const
{
    //-----------------------------------------------------
    // Must shift both x and y by max( x_lshift, y_lshift ).
    //-----------------------------------------------------
    T x_orig = x;
    T y_orig = y;
    int32_t x_lshift;
    int32_t y_lshift;
    reduce_arg( x, x_lshift, false );   
    reduce_arg( y, y_lshift, false );   
    lshift = (x_lshift > y_lshift) ? x_lshift : y_lshift;
    x >>= lshift;
    y >>= lshift;
    if ( debug ) std::cout << "reduce_norm_arg: xy_orig=[" << to_flt(x_orig) << "," << to_flt(y_orig) << "]" << 
                                              " xy_reduced=[" << to_flt(x) << "," << to_flt(y) << "] lshift=" << lshift << "\n"; 
}

template< typename T, typename FLT >
void Cordic<T,FLT>::reduce_angle_arg( T& a, uint32_t& quad ) const
{
    //-----------------------------------------------------
    // Use LUT to find addend.
    //-----------------------------------------------------
    const T a_orig = a;
    cassert( a >= 0 );
    const T *  addend   = impl->reduce_angle_addend.get();
    uint32_t * quadrant = impl->reduce_angle_quadrant.get();

    T index = (a >> frac_w()) & maxint();
    quad = quadrant[index];
    a += addend[index];
    if ( debug ) std::cout << "reduce_angle_arg: a_orig=" << to_flt(a_orig) << " a_reduced=" << to_flt(a) << " quadrant=" << quad << "\n"; 
}

template class Cordic<int64_t, double>;

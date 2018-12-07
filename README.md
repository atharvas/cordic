<p>
This repository contains some C++ code that shows how to implement CORDIC math. <b>See Cordic.h.</b>
</p>

<p>
<b>This is all open-source.  Refer to the LICENSE.md for licensing details.
This code is intended for tutorial purposes only. You should not assume that the library is bug-free or accurate enough
for use as a production reference model.</b>
</p>

<h1>Motivation</h1>

<p>
Addition, subtraction, and multiplication are relatively easy tasks to implement in a computer chip.  Divide, sqrt(), and other
transcendental functions are challenging even when high precision is not required.  CORDIC math makes it easy and cheap to implement
these complicated math functions by using a simple sequence of shifts and adds.  The only complication is that inputs need to 
be reduced to a small range of values, typically -1 .. 1, or -PI/4 .. PI/4.
</p>

<p>
CORDIC was invented in 1956 by Jack Volder using mathematics from 1624 and 1771.  CORDIC, which stands for for 
<b>CO</b>ordinate <b>R</b>otation <b>DI</b>gital <b>C</b>omputer
was first used in the navigation system of the B-58 bomber, which used an early digital computer
that did even have a multiply instruction.  Prior to that, navigation systems were done using
analog circuitry.  So CORDIC is over 60 years old and all been done before.  The hope of
this library is to create a small, yet complete, tutorial package.
</p>

<h1>Fixed-Point</h1>

<p>
The library assumes that values are stored as fixed-point with user-defined integer width (int_w) and fraction width (frac_w).  
The fixed-point container type T must be a signed integer at least as wide as 1+int_w+frac_w. A fixed-point number stores
the sign in the most-significant bit, followed by the int_w binary integer bits, followed by the frac_w binary fraction bits 
in the least-significant
bits.  If T is larger than the required number of bits, the extra upper bits are assumed to contain replications of the sign bit
(1=negative, 0=non-negative).  In other words, fixed-point values are stored in 2's-complement integer containers, so -A == ~A + 1.
</p>

<p>
Here are some examples of fixed-point numbers.  1.3.8 means 1 sign bit, 3 integer bits, and 8 fraction bits.
</p>
<pre>
Format         value            binary (spaces added for readability)
---------------------------------------------------------------------
1.3.8           0.0             0 000.00000000
1.3.8           1.0             0 001.00000000
1.3.8           2.0             0 010.00000000
1.3.8           0.5             0 000.10000000
1.3.8           2^(-8)          0 000.00000001          smallest positive value
1.3.8           -1.0            1 111.00000000          ~(0 001 00000000) + 1  == (1 110 11111111) + 1
</pre>

<p>
This code automatically performs appropriate argument range reductions and post-CORDIC adjustments, 
but it can be turned off if you know that the inputs to the math functions are in the proper range already.
</p>

<p>
The CORDIC routines perform frac_w iterations in order to arrive at frac_w precision.  You may, however,
reduce the number of iterations by passing a different value for n to the constructor.  The numeric encoding
will be the same, but the result will be less precise.  Again, two different Cordic() instances with idential parameters
except for do_reduce and n can be used on an operation-by-operation basis.
</p>

<h1>Floating-Point</h1>

<p>
At some point in the near future, the library will be enhanced to allow T to hold IEEE floating-point numbers with
arbitrary exponent width (exp_w). So the library will allow T (still an integer container) to hold 
fixed-point OR floating-point encodings including, but not
limited to, standard IEEE floating-point formats such as float (fp32), double (fp64), quadruple (fp128), half (fp16), and quarter (fp8).
Only one of int_w or exp_w may be non-zero.  In IEEE floating-point numbers, int_w is 0 because the binary fraction is 
assumed to have an implied '1' before the 
binary point, which is known as a normalized number (e.g., 1.110110<sub>2</sub> * 2<sup>24</sup>).  One exception is when
the value is less than the smallest normalized number 1.0 * 2<sup>MIN_EXP</sup>, which makes it a denormalized number or "denorm." Other special
numbers include +Infinity (e.g., from 1/0), -Infinity (e.g., from -1/0), or NaN (not a number, e.g., from sqrt(-1)), which 
are identified using special encodings of the exponent.
</p>

<p>
Fixed-point numbers naturally support denorms (i.e., they are all denorms), but have no way to indicate a value 
outside their allowed range.  The library needs
an option to mark a number as +Infinity, -Infinity, or NaN.  An additional needed option is
to gracefully flush large numbers to +/- "max value" and NaNs to zero.
</p>

<p>
Also not handled are the different IEEE rounding modes: round-to-nearest (current behavior 
and what one normally expects), 
round-toward-zero, round-toward-plus-infinity, round-toward-minus-infinity.  There's another one worth doing
called round-away-from-zero (aka round-toward-plus-or-minus-infinity). The rounding mode would be set
during the constructor and used for all operations.  These modes will be implemented for both fixed-point and floating-point
encodings.
</p>

<p>
There are a couple other options supported in most floating-point libraries that we'll need to add for
floating-point encodings only.
Flush-To-Zero (FTZ) means that any time an operation would produce a denom (again, a number smaller than the smallest normalized
number), it is changed to zero.
Denorm-As-Zero (DAZ) means that any denorm input to an operation is first changed to zero.
</p>

<h1>freal</h1>

<p>
The library provides an "freal" flexible real number class that follows all the rules of any C++ floating-point number 
(e.g., double, float), but uses Cordic as its underlying implementation. <b>See freal.h for the full list of constructors,
conversions, operators, and std::xxx functions.</b>
</p>

<p>
If you use freal.h, you don't need to use Cordic.h unless you want to set up a default Cordic to
use for implicit conversions to freal from int, double, etc.
</p>

<p>
The default freal container T is int64_t and the default floating-point format for any constants is double.  
These are the same defaults as for Cordic<>.  If you use freal<> as your number type, then you'll pick up these defaults.
</p>

<p>
By default, implicit conversions to/from freal will cause an error.  If you would like to enable both, then create
a Cordic and call these static functions one time:
</p>

<pre>
#include "freal.h"
typedef freal&lt;&gt; real;           // so you don't have to type freal<> everywhere :-)

inline void real_init( void ) 
{
    // one-time calls to static methods
    real::implicit_to_set( new Cordic&lt;&gt;( 7, 24 ) ); // allow implicit conversion  TO   freal (fixed-point 1.7.24)
    real::implicit_from_set( true );                // allow implicit conversions FROM freal (to int, double, etc.)
}

[have your main program call real_init() before using real numbers.]
</pre>

<p>
Note that implicit conversions from int,double,etc. are not allowed for binary operators like +, -, etc.  You must 
explicitly convert them as in this example:
</p>

<pre>
real a = 5.2;          // this will implicitly convert 5.2 to real because no operator involved
real c = real(1) + a;  // this is an operator, so must explicity convert the 1 to disambiguate for C++
</pre>

<h1>Complex Numbers</h1>

<p>
This library does nothing special for complex numbers. Simply use the C++ complex&lt;freal&lt;&gt;&gt; template class
and all the associated complex math functions will just work:
</p>

<pre>
#include &lt;complex&gt;
#include "freal.h"
typedef freal&lt;&gt; real;
typedef complex&lt;real&gt; creal;
</pre>

<h1>Installation</h1>

<p>
To install this on your computer, you'll need git and a C++ compiler, then:
</p>
<pre>
git clone https://github.com/balfieri/cordic
cd cordic
</pre>

<p>
For normal usage, there are no .cpp files to compile.  Everything is in the Cordic.h and freal.h headers.
Simply add this directory to your compiler search path and enable -std=c++17.
</p>

<p>
To build and run the basic "smoke" test, <b>test_basic.cpp</b>, on Linux, Cygwin, or macOS, run:
</p>
<pre>
doit.test
doit.test 1                             - run with debug spew 
doit.test 0 test_basic                  - same as doit.test with no args
doit.test 1 test_basic -new_bugs        - run new bugs too (those not fixed yet)
doit.test 0 test_basic -int_w 8         - change int_w from default to 8 bits
</pre>

<p>
test_basic.cpp does its own checking using macros in test_helpers.h.  In the near future, 
Cordic should do optional checking of computations so that test_helpers.h can be deleted or greatly simplified.
</p>

<p>
Bob Alfieri<br>
Chapel Hill, NC
</p>

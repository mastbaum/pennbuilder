pennbuilder Event Builder
=========================
Introduction
------------
pennbuilder is a fast, lightweight event builder for the SNO+ experiment. It was developed primarily for support of monitoring tool development, since those tools require build data. Designed from the ground up for speed, it makes use of threading, the jemalloc thread-optimized memory allocator, and optimized data structures to achieve very high throughput. Logic tracking the readout of the front-end electronics provides an additional performance boost, since a fixed timeout is no longer needed to tell whether an event is complete.

Installation
------------
Dependencies:
* C compiler. Only tested with GCC.
* jemalloc library (http://www.canonware.com/jemalloc/)
* pthreads library (-lpthread) (POSIX) 
* rt (time) library (-lrt) (POSIX)

To make:

    $ make

To make with debugging flags:

    $ make CXXFLAGS=-g

More Information
----------------
For a detailed technical description of pennbuilder, see the PDF document in the whitepaper directory. The whitepaper may contain references to future plans.



pennbuilder Event Builder
=========================
Introduction
------------
pennbuilder is a fast, lightweight event builder for the SNO+ experiment. It was developed primarily for support of monitoring tool development, since those tools require build data. Designed from the ground up for speed, it makes use of threading, the jemalloc thread-optimized memory allocator, and optimized data structures to achieve high throughput.

Installation
------------
Dependencies:

* C++ compiler. Only tested with GCC.
* pthreads library (-lpthread) (POSIX) 
* rt (time) library (-lrt) (POSIX)
* jemalloc library (http://www.canonware.com/jemalloc/)
* ROOT 5.32 (http://root.cern.ch)
* OrcaRoot (http://orca.physics.unc.edu/~markhowe/Subsystems/ORCARoot.html)
* avalanche (http://github.com/mastbaum/avalanche)

To make:

    $ make

To make with debugging flags:

    $ make CXXFLAGS=-g

More Information
----------------
For a detailed technical description of pennbuilder, see the PDF document in the whitepaper directory. The whitepaper may contain references to future plans.



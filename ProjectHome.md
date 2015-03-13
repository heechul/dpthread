DPTHREAD is a deterministic alternative of standard pthread. We call a program execution is deterministic, if its correctness does not depend on the thread interleaving order.

When we write a multi-threaded program, the most difficult part, compared to a single thread program, is the fact that the thread scheduling order is non-deterministic. While we use synchronization operations, such as mutex and condition variable, the number of potential interleaving is huge (exponential); and we have to check all the possible interleaving to make sure there is no bug, which is almost impossible for any realistic program.

The key benefit that DPTHREAD gives to programmers is easy of debugging and testing of multi-threaded programs because it makes thread scheduling deterministic so that the program behavior only depend on its input.

Check more details in the following. [README](http://code.google.com/p/dpthread/wiki/README)

Also, there is some performance number available  in the following. [Performance](http://code.google.com/p/dpthread/wiki/Performance)
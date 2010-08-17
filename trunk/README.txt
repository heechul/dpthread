
			Dpthread - Deterministic Pthread
			--------------------------------

Dpthread is a thread runtime that provide deterministic order of all synchronization operations (i.e., mutex) in shared memory multi-processor environment. 

The key idea of dpthread is the same as Kendo (ASPLOS09): determine the global order of all synchronization accesses based on instruction count. However, dpthread is built on top of standard pthread runtime and perf_event, interface found in linux 2.6.32 or newer kernel, while Kendo use custom kernel interface. 

Also, dpthread provides more complete set of deterministic alternatives of pthread synchronization APIs (mutex, condition variable, and barriers.)

For further information, see the website -- http://code.google.com/p/dpthread. 

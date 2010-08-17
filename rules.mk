.SUFFIXES: .c .S .o .lo .cpp

.S.o:
	$(CC) $(CFLAGS) -c $*.S
.c.o:
	$(CC) $(CFLAGS) -c $*.c
.cpp.o:
	$(CXX) $(CFLAGS) -c $*.cpp
.c.lo:
	$(CC) -fPIC -DPIC $(CFLAGS) -c $*.c -o $*.lo
.S.lo:
	$(CC) -fPIC -DPIC $(CFLAGS) -c $*.S -o $*.lo


CC = g++
CFLAGS = -g -lpthread -fgnu-tm
CONFF =  
all: randtrack 

randtrack: list.h hash.h defs.h randtrack.cc
	$(CC) $(CFLAGS) $(CONFF) -std=c++11 randtrack.cc -o randtrack

randtrack_tm: list.h hash.h defs.h randtrack_tm.cc
	$(CC) $(CFLAGS) $(CONFF) -std=c++11 randtrack_tm.cc -o randtrack

randtrack_global_lock: list.h hash.h defs.h randtrack_global_lock.cc
	$(CC) $(CFLAGS) $(CONFF) -std=c++11 randtrack_global_lock.cc -o randtrack

randtrack_list_lock: list.h hash.h defs.h randtrack_list_lock.cc
	$(CC) $(CFLAGS) $(CONFF) -std=c++11 -DLIST_LEVEL randtrack_list_lock.cc -o randtrack

randtrack_element_lock: list.h hash.h defs.h randtrack_element_lock.cc
	$(CC) $(CFLAGS) $(CONFF) -std=c++11 -DLIST_LEVEL randtrack_element_lock.cc -o randtrack

randtrack_reduction: list.h hash.h defs.h randtrack_reduction.cc
	$(CC) $(CFLAGS) $(CONFF) -std=c++11 randtrack_reduction.cc -o randtrack

clean:
	rm -f *.o randtrack randtrack_global_lock randtrack_tm randtrack_list_lock

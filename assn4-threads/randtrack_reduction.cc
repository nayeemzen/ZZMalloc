
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>    /* POSIX Threads */

#include "defs.h"
#include "hash.h"


#define SAMPLES_TO_COLLECT   10000000
#define RAND_NUM_UPPER_BOUND   100000
#define NUM_SEED_STREAMS            4

// allow configuring debug via commandline -DDBG
#ifndef DBG
#define DBG_PRINT(...)       (void)NULL;
#define DBG_ASSERT(expr)     (void)NULL;
#else
#define DBG_PRINT(...)       printf(__VA_ARGS__);
#define DBG_ASSERT(expr)     assert(expr);
#endif


/* 
 * ECE454 Students: 
 * Please fill in the following team struct 
 */
team_t team = {
    "0xdefec8ed",                  /* Team name */

    "Zaid Al-Khishman",                    /* First member full name */
    "999280665",                 /* First member student number */
    "zaid.al.khishman@mail.utoronto.ca",                 /* First member email address */

    "Nayeem Husain Zen",                           /* Second member full name */
    "998927924",                           /* Second member student number */
    "nayeem.zen@mail.utoronto.ca"                            /* Second member email address */
};

unsigned num_threads;
unsigned samples_to_skip;

class sample;

class sample {
  unsigned my_key;
 public:
  sample *next;
  unsigned count;

  sample(unsigned the_key){my_key = the_key; count = 0;};
  unsigned key(){return my_key;}
  void print(FILE *f){printf("%d %d\n",my_key,count);}
};

// This instantiates an empty hash table
// it is a C++ template, which means we define the types for
// the element and key value here: element is "class sample" and
// key value is "unsigned".  
hash<sample,unsigned> h[NUM_SEED_STREAMS];

class tdata{
public:
  int table_idx;
  int begin;
  int end;
};

extern pthread_mutex_t* list_lock_to_release;

void* func(void *ptr){
  tdata* data = (tdata*) ptr;
  int i,j,k;
  int rnum;
  unsigned key;
  int idx = data->table_idx;
  sample *s;

  // process streams starting with different initial numbers
 DBG_PRINT("This thread is working from %d to %d\n",data->begin, data->end);
 for (i= data->begin ; i < data->end; i++){
    rnum = i;

    // collect a number of samples
    for (j=0; j<SAMPLES_TO_COLLECT; j++){

      // skip a number of samples
      for (k=0; k<samples_to_skip; k++){
         rnum = rand_r((unsigned int*)&rnum);
      }

      // force the sample to be within the range of 0..RAND_NUM_UPPER_BOUND-1
      key = rnum % RAND_NUM_UPPER_BOUND;
      // if this sample has not been counted before
      if (!(s = h[idx].lookup(key, NULL))){
        // insert a new element for it into the hash table
        s = new sample(key);
        h[idx].insert(s);
        // increment the count for the sample
        
      }

      s->count++;
    }
  }

}

void reduce_tables(int num_threads, hash<sample, unsigned> *result) {
   for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < (1 << h[i].size()); j++) {
            auto list = h[i].get_list(j);
            while (list->num_ele() > 0) {
                sample *s;
                auto elem = list->pop();
                unsigned key = elem->key();
                if (!(s = result->lookup(key, NULL))) {
                    s = new sample(key);
                    result->insert(s);
                    s->count = elem->count;
                } else {
                    s->count += elem->count;
                }
           }
       }
   }
}

int  
main (int argc, char* argv[]){
  int t;
  pthread_t threads[4] = { 0 };

  // Print out team information
  printf( "Team Name: %s\n", team.team );
  printf( "\n" );
  printf( "Student 1 Name: %s\n", team.name1 );
  printf( "Student 1 Student Number: %s\n", team.number1 );
  printf( "Student 1 Email: %s\n", team.email1 );
  printf( "\n" );
  printf( "Student 2 Name: %s\n", team.name2 );
  printf( "Student 2 Student Number: %s\n", team.number2 );
  printf( "Student 2 Email: %s\n", team.email2 );
  printf( "\n" );

  // Parse program arguments
  if (argc != 3){
    printf("Usage: %s <num_threads> <samples_to_skip>\n", argv[0]);
    exit(1);  
  }
  sscanf(argv[1], " %d", &num_threads); // not used in this single-threaded version
  sscanf(argv[2], " %d", &samples_to_skip);

  tdata* data = new tdata[num_threads];
  // initialize a 16K-entry (2**14) hash of empty lists
  for (int i = 0; i < NUM_SEED_STREAMS; i++) {
        h[i].setup(14);
  }

  /* create threads 1 and 2 */
/*
  1 -> 4 / 1 = 4 (0, 0+4)
  2 -> 4 / 2 = 2 (0, 0+2), (2, 2+2)
  4 -> 4 / 4 = 1 (0, 0+1), (1, 1+1), (2, 2+1), (3, 3+1)

*/
  int i = 0;
  for (t=0; i < num_threads; t += 4/num_threads,i++){
      DBG_PRINT("start,end::%d, %d\n", t, t+(4/num_threads));
      data[i].table_idx = i;
      data[i].begin=t;
      data[i].end=t+(4/num_threads);
      pthread_create (&threads[i], NULL, func, (void *) &data[i]);
  }


  for (t=0; t < num_threads; t++){
     pthread_join(threads[t], NULL);
  }
    
  hash<sample,unsigned> reduced_table;
  reduced_table.setup(14);
  reduce_tables(num_threads, &reduced_table);
  // print a list of the frequency of all samples
  reduced_table.print();
}

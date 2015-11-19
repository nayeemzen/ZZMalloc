
#ifndef HASH_H
#define HASH_H

#include <stdio.h>
#include "list.h"
// allow configuring debug via commandline -DDBG
#ifndef DBG
#define DBG_PRINT(...)       (void)NULL;
#define DBG_ASSERT(expr)     (void)NULL;
#else
#define DBG_PRINT(...)       printf(__VA_ARGS__);
#define DBG_ASSERT(expr)     assert(expr);
#endif

#define HASH_INDEX(_addr,_size_mask) (((_addr) >> 2) & (_size_mask))

template<class Ele, class Keytype> class hash;

template<class Ele, class Keytype> class hash {
 private:
  unsigned my_size_log;
  unsigned my_size;
  unsigned my_size_mask;
  list<Ele,Keytype> *entries;

 public:
  void setup(unsigned the_size_log=5);
  void insert(Ele *e);

  list<Ele,Keytype> *get_list(unsigned the_idx);
  unsigned size() { return my_size_log; };
  //ugly but minimalistic and a classic
  Ele *lookup(Keytype the_key, pthread_mutex_t**);
  void print(FILE *f=stdout);
  void reset();
  void cleanup();
};

template<class Ele, class Keytype> 
void 
hash<Ele,Keytype>::setup(unsigned the_size_log){
  my_size_log = the_size_log;
  my_size = 1 << my_size_log;
  my_size_mask = (1 << my_size_log) - 1;
  entries = new list<Ele,Keytype>[my_size];
}

template<class Ele, class Keytype> 
list<Ele,Keytype> *
hash<Ele,Keytype>::get_list(unsigned the_idx){
  if (the_idx >= my_size){
    fprintf(stderr,"hash<Ele,Keytype>::list() public idx out of range!\n");
    exit (1);
  }
  return &entries[the_idx];
}


template<class Ele, class Keytype> 
Ele *                                      //ugly but minimalistic and a classic
hash<Ele,Keytype>::lookup(Keytype the_key, pthread_mutex_t** list_lock_to_release){
  list<Ele,Keytype> *l;

  l = &entries[HASH_INDEX(the_key,my_size_mask)];

  // lock the specific list here, unlock after increment
  // ugly but yet minimalistic and a classic 
  #ifdef LIST_LEVEL
  pthread_mutex_lock(&(l->list_lock));
  *list_lock_to_release = (&(l->list_lock));
  #endif

  return l->lookup(the_key);
}  

template<class Ele, class Keytype> 
void 
hash<Ele,Keytype>::print(FILE *f){
  unsigned i;

  for (i=0;i<my_size;i++){
    entries[i].print(f);
  }
}

template<class Ele, class Keytype> 
void 
hash<Ele,Keytype>::reset(){
  unsigned i;
  for (i=0;i<my_size;i++){
    entries[i].cleanup();
  }
}

template<class Ele, class Keytype> 
void 
hash<Ele,Keytype>::cleanup(){
  unsigned i;
  reset();
  delete [] entries;
}

template<class Ele, class Keytype> 
void 
hash<Ele,Keytype>::insert(Ele *e){
  list<Ele,Keytype>* l; 
  l = &entries[HASH_INDEX(e->key(),my_size_mask)];
  l->push(e);

}


#endif

#include "mymalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


//block wears two hats:
// 1. Allocated - only size is used
// 2. Free - all three fields are used

struct block {
  unsigned int size;
  struct block* next;
};

static struct block *free_head = NULL;

void *my_malloc(size_t size){
  if(size <= 0)
    return NULL;

  if(size % 8 != 0 ){
    for(int i = 0; i < 7; i++){
      size++;
      if(size % 8 == 0)
        break;
    }
  }

  //add 8 bytes for bookkeeping
  size += 8;

  struct block *free_ptr = free_head;
  struct block *freedom_ptr = NULL;

  int freedom;

  if(free_ptr == NULL){
    freedom_ptr = (size < 8192) ? sbrk(8192) : sbrk(size);
    freedom_ptr->size = (size < 8192) ? 8192 : size;

    freedom = freedom_ptr->size - size; 
  }

  // 1. check to see if anything is free
  while(free_ptr != NULL){

    // we want to see how much is free and how much is being used
    freedom = free_ptr->size - size;

    // enough space for user break
    if(free_ptr->next == NULL && freedom >= 0){
      freedom_ptr = free_ptr;
      break; 
    }

    // ran through the list and noticed that nothing is free
    // did this so the chain is not broken
    if(free_ptr->next == NULL && freedom < 0){
      freedom_ptr = (size < 8192) ? sbrk(8192) : sbrk(size);
      freedom_ptr->size = (size < 8192) ? 8192 : size;

      // freedom now holds the amount of free bytes available
      freedom = freedom_ptr->size - size; 
      break;
    }

    if(free_ptr->next == NULL)
      break;

    free_ptr = free_ptr->next;
  }

  struct block *alloc_ptr = freedom_ptr;
  alloc_ptr->size = size;


  if(freedom > 0){
    // the head is free add to free_list
    if(free_head == NULL){
      free_head = (void*)(alloc_ptr) + size;
      free_head->size = freedom;
      free_head->next = NULL;
    }
    // free_head is the only node on the list
    // we either update the existing free_head or
    // we add more nodes to free list
    else if(free_head->next == NULL){
      if(free_head->size < size){
        free_head->next = (void*)(alloc_ptr) + size;
        free_head->next->size = freedom;
        free_head->next->next = NULL;
      }
      else{
        free_head = (void*)(alloc_ptr) + size;
        free_head->size = freedom;
        free_head->next = NULL;
      }
    } 
    // more than one node on the free list
    // go to end of free list and either add to the free list
    // or update the node accordingly
    else{
      free_ptr = free_head;
      while(free_ptr->next->next != NULL){
        free_ptr = free_ptr->next;
      }
      if(free_ptr->next->size < size)
        free_ptr=free_ptr->next;
      
      free_ptr->next = (void*)(alloc_ptr) + size;
      free_ptr->next->size = freedom;
      free_ptr->next->next = NULL;
    }
  }
  // you are using the whole node so delete the node from free_list
  else{
    // this means only one node we can just make it null
    if(free_head == freedom_ptr){
      free_head = NULL;
    }
    // more than one node so go to the end of the list
    else if(free_head != NULL){
      free_ptr = free_head;
      while(free_ptr->next != NULL){
        free_ptr = free_ptr->next;
        if(free_ptr == freedom_ptr){
          free_ptr->next = free_ptr->next->next;
        } 
      }
    }
  }

  return (void*)(alloc_ptr) + 8;
}

// free node to the free_list
void my_free(void *ptr){
  if(ptr == NULL)
    return;

  struct block *tmp = free_head;

  free_head = (ptr-8);
  free_head->next = tmp;
}

// returns the begining of the list
void *free_list_begin(){
  return (void*)free_head;
}

// returns the next node on the free_list
void *free_list_next(void *node){
  if(node == NULL)
    return NULL;

  return ((struct block*)node)->next;
}

void coalesce_free_list(){
  struct block *ptr = free_head;

  ptr = free_head;
  unsigned int sorted_size = 0, i ,j;
  while(ptr != NULL){
    sorted_size++;
    ptr = ptr->next;
  }
  
  // need to reset ptr to the head
  ptr = free_head;
  
  unsigned long *sorted = malloc(sizeof(unsigned long) * sorted_size);
  unsigned long tmp;
  
  //struct block free[sorted_size], tmp;
  for(i=0; i < sorted_size; i++){
    sorted[i] = (unsigned long)ptr;
    //free[i] = *ptr;
    ptr = ptr->next;
  }

  //qsort(sorted_size, sorted_size, sizeof(sorted[0]),cmpfunc);
  for(i = 0; i < sorted_size; ++i){
    for(j = i + 1; j < sorted_size; ++j){
      if(sorted[i] > sorted[j]){
          tmp = sorted[i];
          sorted[i] = sorted[j];
          sorted[j] = tmp;
      }
    }
  }
 
  free_head = (struct block*)sorted[0];
  ptr = free_head;

  // i build up a new linked list and ignore the old one
  // i do that by calling malloc on *sorted[] and freeing *sorted[]
  // i for contgious nodes if they are then change ptr->size
  // if they are not then add the the free_list and increment ptr
  for(i=0; i<sorted_size-1; i++) { 
    if(sorted[i] + ((struct block*)sorted[i])->size == sorted[i+1]) 
	    ptr->size += ((struct block*)sorted[i+1])->size;

	  else {
	    ptr->next = (struct block*)sorted[i+1];
	    ptr = ptr->next;
  	}
  }
  ptr->next = NULL;
 
  free(sorted); 
}

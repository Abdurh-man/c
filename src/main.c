#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <signal.h> 
#include <pthread.h> 

#include "jrb.h"
#include "dllist.h"
#include "sockettome.h"

typedef struct room{
  char *name;
  Dllist clients;
  Dllist messages;
  // reserves timestamp for rooms
  pthread_mutex_t *lock;
  // condition for working room up
  pthread_cond_t *cond;
}ROOM;

typedef struct client{
  char *name;
  // room they occupy
  ROOM *room;
  // socket clients communicates to and from
  FILE *read;
  FILE *write;
//  Dllist list;
}CLIENT;

JRB room_list;

void leave(CLIENT *C){
  char *message;
  Dllist ptr;
  ROOM *R;

  R = C->room;

  pthread_mutex_lock(R->lock);
  
  dll_traverse(ptr, C->room->clients){
    CLIENT *tmp = (CLIENT*)ptr->val.v;

    if(strcmp(tmp->name, C->name) == 0){
      dll_delete_node(ptr);
      break;
    }
  }
  
  message = (char*) malloc(sizeof(char) * (strlen(C->name) + 16));
  sprintf(message, "%s has left\n", C->name);

  dll_append(R->messages, new_jval_s(message));

  // singal room to wake up unlock the room
  pthread_cond_signal(R->cond);
  pthread_mutex_unlock(R->lock);

  fclose(C->read);
  fclose(C->write);
  free(C->name);

  free(C);
}

int append_message(CLIENT *C, char *message){
  fputs(message, C->write);

  return fflush(C->write);
}

int client_setup(CLIENT *C){
  char buf[1000];
  char *new_message;

  ROOM *R;

  JRB jrb_tmp;
  Dllist dllist_tmp;

  // prints rooms
  if(fprintf(C->write, "Chat Rooms:\n") < 0){
    return 2;
  }

  jrb_traverse(jrb_tmp,room_list){
    R = (ROOM*)jrb_tmp->val.v;
    if(fprintf(C->write, "\n%s:", R->name) < 0){
      return 2;
    }

    pthread_mutex_lock(R->lock);

    //prints the users in the current room
    dll_traverse(dllist_tmp, R->clients){
      
      if(fprintf(C->write, " %s", ((CLIENT*)dllist_tmp->val.v)->name) < 0){
        return 2;
      }
    }

    pthread_mutex_unlock(R->lock);
  }

  //prompt user for name
  if(fprintf(C->write, "\n\nEnter your chat name (no spaces):\n") < 0){
    return 2;
  }

  if(fflush(C->write) == EOF || feof(C->write)){
    return 2;
  }

  // save info typed from user
  fgets(buf, 1000, C->read);
  if((feof(C->read) != 0) || buf == NULL){
    return 2;
  }

  buf[strlen(buf)-1] = '\0';
  C->name = strdup(buf);

  if(fprintf(C->write, "Enter chat room:\n") < 0){
    return 3;
  }

  if(fflush(C->write) == EOF || feof(C->write)){
    return 3;
  }

  // save info typed from user
  fgets(buf, 1000, C->read);
  if((feof(C->read) != 0) || buf == NULL){
    return 3;
  }

  //search for the room in the room_list
  buf[strlen(buf)-1] = '\0';
  jrb_tmp = jrb_find_str(room_list,buf);

  // loop until we have the room the user wants
  while(jrb_tmp == NULL){
    if(fprintf(C->write, "\nNo chat room %s\n\nEnter chat room:\n",buf) < 0){
      return 3;
    }

    if(fflush(C->write) == EOF || feof(C->write)){
      return 3;
    }

    fgets(buf, 1000, C->read);
    if((feof(C->read) != 0) || buf == NULL){
      return 3;
    }

    buf[strlen(buf)-1] = '\0';
    jrb_tmp = jrb_find_str(room_list,buf);

  }

  new_message = (char*)malloc(sizeof(char) * (strlen(C->name)+16));
  sprintf(new_message, "%s has joined\n", C->name);
  
  C->room = (ROOM*)jrb_tmp->val.v;

  // lock append unlock
  pthread_mutex_lock(C->room->lock);
  
  dll_append(C->room->clients, new_jval_v(C));
  dll_append(C->room->messages, new_jval_v(new_message));
  
  pthread_cond_signal(C->room->cond);
  pthread_mutex_unlock(C->room->lock);

  // add the user to the correct room and print off
  // that they have joined the room

  new_message = (char*)malloc(sizeof(char) * (strlen(C->name)+1004));
  
  while(fgets(buf, 1000, C->read) != NULL){
    if((feof(C->read) != 0) || buf == NULL){
      return 3;
    }
  
    sprintf(new_message, "%s: %s",C->name, buf);

    pthread_mutex_lock(C->room->lock);

    // add the user to client list and add there message with them
    dll_append(C->room->messages, new_jval_s(new_message));

    //singal room to wake up and unlock it
    pthread_cond_signal(C->room->cond);
    pthread_mutex_unlock(C->room->lock);
  
    new_message = (char*)malloc(sizeof(char) * (strlen(C->name)+1004));
  }

  return 1;
}

void *room_func(void *vroom){
  ROOM *R;
  R = (ROOM*)(vroom); 
  Dllist m_dllist, c_dllist;
    
  while(1){
    // lock the room
    pthread_mutex_lock(R->lock);
    
    while(dll_empty(R->messages)){
      pthread_cond_wait(R->cond, R->lock);
    }   
    
    // handle messages pushing by writing all messages on dlist to not be empty
    while(!dll_empty(R->messages)){
      m_dllist = dll_first(R->messages);
      printf("%s",jval_s(m_dllist->val));

      // pthread_cond_wait called on the room clients in room){
      dll_traverse(c_dllist, R->clients){
        if(append_message( (CLIENT*)jval_v(c_dllist->val), jval_s(m_dllist->val)) != 0){
          // user has left room clean
          fclose( ((CLIENT*)jval_v(c_dllist->val))->read);
          fclose( ((CLIENT*)jval_v(c_dllist->val))->write);
          
          free( ((CLIENT*)jval_v(c_dllist->val))->name);
          
          dll_delete_node(c_dllist); 

          free((CLIENT*)jval_v(c_dllist->val));
        }
      }

      // clear out room messages(using a message list)
      
      free(m_dllist->val.s);
      dll_delete_node(m_dllist); 
      
    }
    
    // unlock the room
    // pthread_cond_signal(R->cond);
    pthread_mutex_unlock(R->lock);

  }
}

void *client_func(void *vclient){
  CLIENT *C;

  //pthread_detach(pthread_self()); 

  C = (CLIENT*)(vclient);

  // how far did we get into client set up 1 is success 2 client left before
  // providing name 3 client left before char room
  int how_far = client_setup(C);

  if(how_far == 1){
    // client left
    leave(C);
  }

  else if(how_far == 2){
    fprintf(stdout,"Client left before specifying name\n");
    fclose(C->read);
    fclose(C->write);
    free(C);
  }

  else if(how_far == 3){
    fprintf(stdout,"Clients %s left before specifying chat room\n",C->name);
    fclose(C->read);
    fclose(C->write);
    free(C->name);
    
    free(C);
  }
    // exit thread
    //pthread_exit(NULL);
    return NULL;
}

int main(int argc, char** argv) {
  pthread_t room_t;
  pthread_t client_t;
  int fd;
  int port, rsize, sock;
  ROOM *R;
  CLIENT *C;

  if (argc < 3) {
    fprintf(stderr, "usage: ./chat-server port Chat-Room_names ...\n");
    exit(1);
  }

  port = atoi(argv[1]);

  if(port < 50000){
    fprintf(stderr, "port must be > 50000\n");
    exit(1);
  }

  rsize = argc-2;
  room_list = make_jrb();

  for(int i = 0; i < rsize; i++){
    R = (ROOM*)malloc(sizeof(ROOM));
    R->messages = new_dllist();
    R->clients = new_dllist();
    R->name = argv[i+2];
    jrb_insert_str(room_list, R->name, new_jval_v((void *)R));
  //}
  //
  //jrb_traverse(ptr, room_list){
  //  R = (ROOM*)ptr->val.v;
    R->lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    R->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));

    pthread_mutex_init(R->lock, NULL);
    pthread_cond_init(R->cond, NULL);

    pthread_create(&room_t, NULL, room_func, (void*)R);
  }

  sock = serve_socket(port);

  while(1){
    fprintf(stdout,"Waiting for client to connect... \n");
    fd = accept_connection(sock);
    fprintf(stdout,"Client connected!\n"); 

    C = (CLIENT*)malloc(sizeof(CLIENT));  
    C->read = fdopen(fd,"r");
    C->write = fdopen(fd,"w");

    pthread_create(&client_t, NULL, client_func, C);
  }

  return 0;
}

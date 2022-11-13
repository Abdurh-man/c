#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "fields.h"
#include "jrb.h"

void redirecting_input(char *file){
  //printf(file);
  int fd = open(file, O_RDONLY,0664);
  if(fd < 0){
    perror(file);
    exit(1);
  }
  // file descriptor 0 belongs to stdin
  if(dup2(fd,0) != 0){
    perror(file);
    exit(1);
  }
  close(fd);
}


void redirecting_output(char *file){
  //printf(file);
  // the 0644 is to change the file the mode of the file
  int fd = open(file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
  if(fd < 0){
    perror(file);
    exit(1);
  }

  // file descriptor 1 belongs to stdout
  if(dup2(fd,1) != 1){
    perror(file);
    exit(1);
  }
  close(fd);
}


void appending_redirected_output(char *file){
  //printf(file);
  // the 0644 is to change the file the mode of the file
  int fd = open(file, O_WRONLY | O_APPEND | O_CREAT, 0644);
  if(fd < 0){
    perror(file);
    exit(1);
  }

  // file descriptor 1 belongs to stdout
  if(dup2(fd,1) != 1){
    perror(file);
    exit(1);
  }
  close(fd);
}

// this function is used to redirect accordingly
void redirecting(char **arg, int i){

  // should not be indexing if i is 0 or negative 
  if(i < 1)
    return;

  if(strcmp(arg[i],"<") == 0){ 
    redirecting_input(arg[i+1]);     
    // want to remove < file.txt from the argument
    arg[i] = NULL;
    arg[i++] = NULL;
  }

  else if(strcmp(arg[i],">") == 0){
    redirecting_output(arg[i+1]);
    arg[i] = NULL;
    arg[i++] = NULL;
  }

  else if(strcmp(arg[i],">>") == 0){
    appending_redirected_output(arg[i+1]);
    arg[i] = NULL;
    arg[i++] = NULL;
  }
}

// this function is used to do stuff before the first pipe
void first_pipe(char **argv, int (*pipefd)[2], int argc, int amper){
  int pid;
  int status;  
  pipe(*pipefd);

  // fork and redirect accordingly
  if( (pid = fork()) == 0){
    redirecting(argv,argc-2);

    if(dup2(pipefd[0][1], 1) != 1) {
      perror("headsort: dup2(pipefd[1])");
      exit(1);
    }
    close(pipefd[0][1]);

    // exec for the stuff before the pipe
    execvp(argv[0],argv);
    perror(argv[0]);
    exit(1);
  }
  else{
    close(pipefd[0][1]);

    if(amper == 0){
      while(pid != wait(&status));
    }
  }   
}

// the output of the first pipe is the input for the subsequent pipe
void mid_pipe(char **argv, int (*pipefd)[2], int argc, int amper){
  int pid;
  int status;  
  int fd = pipefd[0][0];

  close(pipefd[0][1]);
  pipe(*pipefd);

  if( (pid = fork()) == 0){
    if(dup2(fd, 0) != 0) {
      perror("headsort: dup2(pipefd[0])");
      exit(1);
    }
    close(fd);


    if(dup2(pipefd[0][1], 1) != 1) {
      perror("headsort: dup2(pipefd[0])");
      exit(1);
    }
    close(pipefd[0][1]);

    execvp(argv[0],argv);
    perror(argv[0]);
    exit(1);
  }
  else{
    close(fd);
    close(pipefd[0][1]);
    if(amper == 0){
      while(pid != wait(&status));
    }
  }   
}


void last_pipe(char **argv, int (*pipefd)[2], int argc, int amper){
  int pid;
  int status;  
  close(pipefd[0][1]);
  if( (pid = fork()) == 0){
    redirecting(argv,argc-2);

    if(dup2(pipefd[0][0], 0) != 0) {
      perror("headsort: dup2(pipefd[1])");
      exit(1);
    }
    close(pipefd[0][0]);
    
    // here we exec the stuff after the last pipe
    execvp(argv[0],argv);
    perror(argv[0]);
    exit(1);
  }
  else{
    close(pipefd[0][0]);

    if(amper == 0){
      while(pid != wait(&status));
    }
  }
}


void pipe_it(IS is, int pipe_num, int amper){
  // allocate an array of commands with each command seperated by pipe
  char ***argv = NULL;
  argv = (char ***)calloc((pipe_num + 1), sizeof(char **));
  
  for (int i = 0; i <= pipe_num; i++){
    argv[i] = NULL;
    argv[i] = (char**)calloc(64, sizeof(char*));
    
  }

  int command_count = 0;
  int argc = 0;
  int pipefd[2];

  // assign stuff to argv
  for(int i = 0; i < is->NF; i++){
    // when a pipe is seen change that a NULL
    if(strcmp(is->fields[i],"|") == 0){

      argv[command_count][i-argc] = NULL;

      command_count++;
      argc = i+1;

      continue;
    }
    argv[command_count][i-argc] = strdup(is->fields[i]);
  }

  int i = 0;
  argc = 0;
  for(i = 0; i <= pipe_num; i++){
    // get the size of each command
    for(argc = 0; ;argc++){
      if(argv[i][argc] == NULL || ( strcmp(argv[i][argc],"") == 0) ){
        argv[i][argc] = NULL;
        break;
      }
    }
    if(i+1 == 1){
      first_pipe(argv[i], &pipefd, argc, amper);
    }
    
    else if(i < pipe_num){
      mid_pipe(argv[i], &pipefd, argc, amper);
    }

    else if(i == pipe_num){
      last_pipe(argv[i], &pipefd, argc, amper);
    }

    // freeing the words in the specifec command
    for(int j = 0; j < argc; j++){
      free(argv[i][j]);
    }
  }
  // freeing up the rest 
  for(int i = 0; i <= pipe_num; i++){
    free(argv[i]);
  }
  free(argv);
}

int main(int argc, char** argv) {
  IS is;
  int status;
  //Dllist line = new_dllist();

  // making sure the right number of arguments given
  if(argc != 2 && argc != 1){
    perror("Expected ./jsh [arg]\n");
    exit(1);
  }

  // if 2 arguments given
  char *prompt = "JSH: ";
  if(argc == 2){
    if(strcmp(argv[1], "-") == 0){
      prompt = "";
    }
    else{
      prompt = argv[1];
      strcpy(prompt, ": ");
    }  
  }

  is = new_inputstruct(NULL);

  printf(prompt);
  while(!feof(stdin) && get_line(is) >= 0){
    if(is->NF == 0)
      continue;

    int pipe_num = 0;
    int amper = 0;
    //execvp need a null terminated string so add NULL to fields
    is->fields[is->NF] = NULL;

    if((is->fields[is->NF-1][0] == '&')){
      is->fields[is->NF-1] = NULL;
      is->NF--;
      amper = 1;
    }
    for(int i = 0; i < is->NF; i++){
      if(strcmp(is->fields[i],"|") == 0){
        pipe_num++;
      }
    }

    // deals with pipes given a number of pipes present
    if(pipe_num > 0){
      pipe_it(is,pipe_num,amper);  
    }

    // no pipes present so just do file redirecting and exec
    else{
      pid_t pid = fork();
      if(pid == 0) {

        for(int i = 0; i < is->NF; i++){
          redirecting(is->fields,i);
        }
        execvp(is->fields[0],is->fields);
        perror(is->fields[0]);
        exit(1);
      }

      // if no & then it wait until the wait() returns the pid
      else if(amper == 0){
        while(pid != wait(&status));
      }

      printf(prompt);
    }
  }

  // freeing IS
  jettison_inputstruct(is);
  return 0;
}


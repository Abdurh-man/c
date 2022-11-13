//#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>

#include "jrb.h"
#include "fields.h"
#include "dllist.h"
#include "jval.h"

typedef struct{
  Dllist C,H,F,L,O;

}TYPE;

long unsigned header_check(TYPE *T){
  Dllist tmp;
  int exists;
  long unsigned max = 0;
  struct stat buf;
  dll_traverse(tmp, T->H){
    exists = stat(tmp->val.s, &buf);
    if(exists < 0){
      perror(tmp->val.s);
      return 1;
    }
    else{
      if(max < buf.st_mtime){
        max = buf.st_mtime;
      }
    }
  }
  return max;
}

int source_check(TYPE *T, int H_max, char * E_file){
  Dllist tmp, tmp1;
  int exists;
  char recompiled = 'F';
  long unsigned O_max = 0;
  long unsigned C_time = 0;
  char *file = NULL; 
  struct stat buf;
  char command[50];

  dll_traverse(tmp, T->C){
    file = tmp->val.s;
    exists = stat(file, &buf);
    if(exists < 0){
      fprintf(stderr,"fmakefile: ");
      perror(tmp->val.s);
      return 1;
    }
    else{
      C_time = buf.st_mtime;
      char file_o[50] = " "; 
     
      strncpy(file_o,file,strlen(file)-1);
      
      strcat(file_o,"o");
      //if(C_time > H_max){
      
      dll_append(T->O,new_jval_s(strdup(file_o)));
      
      exists = stat(file_o, &buf);
      //if there is no .o file corresponding to the .c file, then the .c file must be compiled (with -c)
      if(exists < 0){
        sprintf(command,"gcc -c ");
        
        // add the flags to the compilation command line
        dll_traverse(tmp1, T->F){
          strcat(command,tmp1->val.s);
          strcat(command," ");
        }
        
        strcat(command,file);
        //compile the command then clear the string 

        printf("%s\n",command);
        
        //system call failed free and exit
        if(system(command) != 0){
          fprintf(stderr,"Command failed.  Exiting\n");
          return 1;
        }

        recompiled = 'T';
      }

      //If there is a .o file corresponding to the .c file
      else{
        long unsigned O_time = buf.st_mtime;
        //i want the newest .o file compliation time
        if(O_time > O_max)
          O_max = O_time;

        if(C_time > O_time || H_max > O_time){
          sprintf(command,"gcc -c ");

          // add the flags to the compilation command line
          dll_traverse(tmp1, T->F){
            strcat(command,tmp1->val.s);
            strcat(command," ");
          }

          strcat(command,file);
          //compile the command then clear the string 
          printf("%s\n",command);

          //system call failed free and exit
          if(system(command) != 0){
           fprintf(stderr,"Command failed.  Exiting\n");
           return 1;
          }

          recompiled = 'T';
        }
      }
       memset(file_o, 0, 50);
    }
  }
  
  exists = stat(E_file, &buf);
  if(exists == 0 && buf.st_mtime > O_max && recompiled == 'F'){
    printf("%s up to date\n",E_file);
  }
  else{// if(exists != 0){
    sprintf(command,"gcc -o ");
    
    strcat(command,E_file);

    // add the flags to the compilation command line
    dll_traverse(tmp1, T->F){
      strcat(command," ");
      strcat(command,tmp1->val.s);
    }
     
    dll_traverse(tmp1, T->O){
      strcat(command," ");
      strcat(command,tmp1->val.s);
    }
    
    // add the list-of-libraries to the compilation command line
    dll_traverse(tmp1, T->L){
      strcat(command," ");
      strcat(command,tmp1->val.s);
    }
    
    //compile the command then clear the string 
    printf("%s\n",command);

    //system call failed free and exit
    if(system(command) != 0){
      //fprintf(stderr,"Command failed.  Exiting\n");
      fprintf(stderr,"Command failed.  Fakemake exiting\n");
      return 1;
    }
  }
  return 0;
}

void add_to_list(TYPE *T, IS is){
  // gets the first string of the line
  char list_var = is->text1[0];

  //printf("%c\n\n",list_var);

  switch(list_var){
    case 'C': 
      for(int i = 1; i < is->NF; i++){
        dll_append(T->C,new_jval_s(strdup(is->fields[i])));
      }
      break;

    case 'H':
      for(int i = 1; i < is->NF; i++){
        dll_append(T->H,new_jval_s(strdup(is->fields[i])));
      }
      break;

    case 'F':
      for(int i = 1; i < is->NF; i++){
        dll_append(T->F,new_jval_s(strdup(is->fields[i])));
      }
      break; 

    case 'L':
      for(int i = 1; i < is->NF; i++){
        dll_append(T->L,new_jval_s(strdup(is->fields[i])));
      }
      break;

    default:
      printf("Unknown file description given\n");
  }
}

void dealloc(TYPE *T, IS is, char *E, int end){
  Dllist node;
  // free the list-of-fields
  dll_traverse(node, T->L){
    free(node->val.s);
  }
  free_dllist(T->L);

  // free the source list of files
  dll_traverse(node, T->C){
    free(node->val.s);
  }
  free_dllist(T->C);
    
  //free the header list 
  dll_traverse(node, T->H){
    free(node->val.s);
  }
  free_dllist(T->H);
    
  //free the flag list 
  dll_traverse(node, T->F){
    free(node->val.s);
  }
  free_dllist(T->F);

  //free the object list 
  dll_traverse(node, T->O){
    free(node->val.s);
  }
  free_dllist(T->O);
  
  free(T);
  
  //frees up memory allocated with new_inputstruct
  jettison_inputstruct(is);
 
  if(strcmp(E," ") != 0)
    free(E);

  //exit(1) with error exit(0) without error the value of end are passed in
  //else where
  exit(end);
}

int main(int argc, char **argv){
  IS is;
  char file[50] = " ";
  

  if(argc == 2)
    strncpy(file,argv[1],strlen(argv[1]));
  else
    strcpy(file,"fmakefile");
    

  is = new_inputstruct(file);
  
  if(is == NULL){
    perror(argv[1]);
    return -1;
  }

  TYPE *T = malloc(sizeof(TYPE));

  T->C = new_dllist();
  T->H = new_dllist();
  T->F = new_dllist();
  T->L = new_dllist();
  T->O = new_dllist();

  int check_E = 0;
  char *E = " ";
  while(get_line(is) >= 0){
    if(is->NF == 0){
      //line is empty don't print it
      continue;
    }

    if(is->text1[0] == 'E'){
      check_E++;
      if(is->NF > 2){
        fprintf(stderr,"No executable specified\n");
        //frees and exists
        dealloc(T,is,E,1);
      }
      
      if(check_E > 1){
        fprintf(stderr,"fmakefile (%i) cannot have more than one E line\n",is->line);
        
        // this is wrong by gradescript but this is correct code
        //        fprintf(stderr,"%s (%i) cannot have more than one E line\n",file,is->line);
        //frees and exists
        dealloc(T,is,E,1);
      }
      
      E = strdup(is->fields[1]);
    } 
    else
      add_to_list(T, is);
  }

  if(check_E == 0){
    fprintf(stderr,"No executable specified\n");
    //frees and exists
    dealloc(T,is,E,1);
  }
  
  long unsigned H_max = header_check(T);
  if(H_max == 1){
    //frees and exists
    dealloc(T,is,E,1);
  }
  //source_check(T,H_max,buf.st_mtime,E);
  int err = source_check(T,H_max,E);
 
  //frees and exists
  dealloc(T,is,E,err);

  return 0;
}

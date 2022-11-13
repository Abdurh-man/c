#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<dirent.h>

#include <fcntl.h>
#include <unistd.h>

#include <stdio_ext.h>

#include <time.h>
#include <utime.h>

#include "jrb.h"
#include "fields.h"
#include "dllist.h"
#include "jval.h"

// dealloc frees the dll_list and the jrb_tree and exists
void dealloc(JRB inode_list, Dllist directories){
  Dllist tmp;
  dll_rtraverse(tmp, directories) {
    free(tmp->val.s);
    tmp = tmp->blink;
    tmp = tmp->blink;
  }
  free_dllist(directories);

  JRB tmp_node;
  jrb_traverse(tmp_node, inode_list){
    free(tmp_node->val.v);
  }
  jrb_free_tree(inode_list);

  exit(1);
}

int main(int argc, char** argv){
  JRB inode_list, JRB_tmp;
  inode_list = make_jrb();

  if(argc != 1){  
    fprintf(stderr, "%s [directory]\n",argv[1]);
  }

  Dllist directories, tmp;

  directories = new_dllist();

  char *fn_name;
  int size, mode;
  long inode,f_size,mtime;
  //char *fn_mode, *fn_mtime, *fn_name, *fn_inode; 

    // parses the filename size then the neccessary stuff in the look then the
    // filename size again 
    while(fread(&size,4,1,stdin) == 1){ 
      // need the malloc for the null terminating character
      fn_name = malloc(size + 1);
      
      // gets in the name and if it does read it print out an error message and
      // free all the necessary stuff and exit
      // dealloc frees the dll_list and the jrb_tree and exists
      if(fread(fn_name,size,1,stdin) != 1){
        fprintf(stderr, "Bad tarc file at byte %d.  Tried to read filename, but only got 1 bytes\n" ,size);
        free(fn_name);
        dealloc(inode_list,directories);
      }
      fn_name[size] = '\0';

      if(fread(&inode,8,1,stdin) != 1){
        fprintf(stderr, "Bad tarc file for %s.  Couldn't read inode\n" , fn_name);
        free(fn_name);
        dealloc(inode_list,directories);
      }  

      if(jrb_find_int(inode_list,inode) == NULL){
        jrb_insert_int( inode_list, inode,new_jval_s(strdup(fn_name)) );
      }

      else{

        JRB_tmp = jrb_find_int(inode_list,inode);
        link(JRB_tmp->val.s,fn_name);
        // need to free fn_name since in doing continue and it wont free since
        // free is at the bottom of the loop 
        free(fn_name);
        continue;  
      }

      if(fread(&mode,4,1,stdin) != 1){
        fprintf(stderr, "Bad tarc file for %s.  Couldn't read mode\n", fn_name);
        free(fn_name);
        dealloc(inode_list,directories);
      }

      if(fread(&mtime,8,1,stdin) != 1){
        fprintf(stderr, "Bad tarc file for %s.  Couldn't read modification time\n", fn_name);
        free(fn_name);
        dealloc(inode_list,directories);
      }
      
      // i add all the necessary thing needed to change the modifcation time
      // and acces to the file 
      if(S_ISDIR(mode)){
        dll_append(directories, new_jval_i(mode));
        dll_append(directories, new_jval_l(mtime));
        dll_append(directories, new_jval_s(strdup(fn_name)));


        mkdir(fn_name,0775);
      }
      else{

        if(fread(&f_size,8,1,stdin) != 1){
          fprintf(stderr, "Bad tarc file for %s.  Couldn't read size\n", fn_name);
          free(fn_name);
          dealloc(inode_list,directories);
        }
        
        char *f_bytes = malloc(f_size);

        if(fread(f_bytes,f_size,1,stdin) != 1){
          fprintf(stderr, "Bad tarc file at byte %ld.  Tried to read filename, but only got 6 bytes\n",f_size);
          free(fn_name);
          free(f_bytes);
          dealloc(inode_list,directories);
        }
        //printf("%ld\n",f_size);
        FILE *file;

        file = fopen(fn_name,"w+");

        if(file == NULL){
          free(fn_name);
          free(f_bytes);
          dealloc(inode_list,directories);
        }

        if(fwrite(f_bytes,f_size,1,file) != 1){
          free(fn_name);
          free(f_bytes);
          dealloc(inode_list,directories);
        }
        


        // close the file and changed the modification time and the mode
        fclose(file);
        struct utimbuf ubuf;
        ubuf.modtime = mtime;
        ubuf.actime = mtime;

        utime(fn_name,&ubuf);
        chmod(fn_name,mode);

        free(f_bytes);
        //printf("%ld\n",f_size);
      }
      free(fn_name);
    }
  
  // travers backwards to make sure change the info for subdirectories before 
  dll_rtraverse(tmp, directories) {
    // i malloc file so i can delete the current value of the directories list 
    char *file = strdup(tmp->val.s); 
    free(tmp->val.s);

    tmp = tmp->blink;
    long modification_time = tmp->val.l;

    tmp = tmp->blink;
    int mode = tmp->val.i;
    
    // utime takes a you ubuf pointer
    // assigned ubuf to have the correct modification_time
    struct utimbuf ubuf;
    ubuf.modtime = modification_time;
    ubuf.actime = modification_time;
    
    // changed the modification time to the correct one
    utime(file,&ubuf);
    // changed the directory mode to the right one after changing it to write access
    chmod(file,mode);

    free(file); 
  }

  free_dllist(directories);

  JRB tmp_node;
  jrb_traverse(tmp_node, inode_list){
    free(tmp_node->val.v);
  }
  jrb_free_tree(inode_list);

  return 0;
}

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<dirent.h>

#include <fcntl.h>
#include <unistd.h>

#include <stdio_ext.h>

#include "jrb.h"
#include "fields.h"
#include "dllist.h"
#include "jval.h"


//gets the last boundary
void strip(char *fn, char boundary, char **save){
  //char stripped_fn[strlen(fn)];
  char *stripped_fn = malloc(sizeof(fn)+1);
  int c_count = 0;
  for(int i = 0; i <= strlen(fn); i++){
    c_count++;
    if(fn[i] == boundary)
      c_count = 0;
  }
  //i dont want the last / so decrement c_count
  c_count--;

  int relative_dir = strlen(fn) - c_count;
  for(int i = 0; i < c_count; i++)
    stripped_fn[i] = fn[relative_dir + i];

  strcpy(*save,stripped_fn);

  free(stripped_fn);
}


void get_size(char *fn, char *rn, JRB inodes){
  DIR *d;
  struct dirent *de;
  struct stat buf;
  int exists;
  char *s, *rs;
  int fn_size = 0;
  Dllist directories, tmp;

  s = (char *) malloc(sizeof(char)*(strlen(fn)+258));
  // the absoulte path since fn gets the first directory and the rn gets the
  // subdirectories the get the full path you need to combine the two
  sprintf(s, "%s%s",fn,rn);


  d = opendir(s);
  if (d == NULL) {
    perror(s);
    //exit(1);
    return;
  }

  //s = (char *) malloc(sizeof(char)*(strlen(fn)+258));
  rs = (char *) malloc(sizeof(char)*(strlen(rn)+258));

  directories = new_dllist();
  for (de = readdir(d); de != NULL; de = readdir(d)) {

    if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
      continue;
    }

    /* Look for fn/de->d_name */
    // gets the full path by addding the first directory, the subdirectory, and the respective file
    sprintf(s, "%s%s/%s",fn, rn, de->d_name);
    // gets the relative path
    sprintf(rs, "%s/%s",rn, de->d_name);

    exists = lstat(s, &buf);
    if (exists < 0) {
      fprintf(stderr, "Couldn't stat %s\n", s);
      return;
      //exit(1);
    } 
    else{ 

      fn_size = strlen(rs);
      // print Filename size in little endian
      fwrite(&fn_size,4,1,stdout);

      // print Filename
      fwrite(rs,fn_size,1,stdout);

      //print Inode
      fwrite(&buf.st_ino,8,1,stdout);

      if(jrb_find_int(inodes, buf.st_ino) == NULL){
        fwrite(&buf.st_mode,4,1,stdout);
        fwrite(&buf.st_mtime,8,1,stdout);

        jrb_insert_int(inodes, buf.st_ino, JNULL);
      }
      else
        continue;
    }

    // directory if directroy add the to the directory list
    // which traversed to get the subdirectories 
    if(S_ISDIR(buf.st_mode)){
      // passes in the realtive path and concatenates to the first directory to
      // get the absoultre
      dll_append(directories, new_jval_s(strdup(rs)));
    }
    
    // file
    else{
      FILE *file;

      file = fopen(s,"r");

      if(file == NULL){
        // the file doesnt exist so print with error
        perror(s);
        exit(EXIT_FAILURE);
      }
      else{
        char *file_info;
        
        const unsigned long size = buf.st_size;
        
        file_info = malloc(size);

        if(fread(file_info,1,size,file) != size){
          perror(s);
          exit(EXIT_FAILURE);
        }

        fwrite(&size,8,1,stdout);

        //fwrite(file_info,buf.st_size,1,stdout);
        if(fwrite(file_info,1,size,stdout) != size){
          perror(s);
          exit(EXIT_FAILURE);
        }

        fclose(file);
        free(file_info);
      }
    }
  }

  closedir(d);
  dll_traverse(tmp, directories) {
    //goes into the subdirectories
    get_size(fn, tmp->val.s, inodes);
    /* This keeps the program from overgrowing its memory */
    free(tmp->val.s);
  }

  /* As does this */
  free_dllist(directories);
  free(s);
  free(rs);
}


int main(int argc, char** argv){

  JRB inodes;
  inodes = make_jrb();

  if(argc != 2){  
    fprintf(stderr, "%s [directory]\n",argv[1]);
  }

  // alot of this stuff is done to avoid corrupting information
  char fn[strlen(argv[1])];
  memcpy(fn, argv[1], strlen(argv[1])+1);
  int fn_size = 0;
  //rn = relative name
  char *rn = malloc(sizeof(fn));



  // this stuff is done to print the first directory given and its 
  // respective information
  DIR *d;
  d = opendir(fn);
  if(d == NULL) {
    perror(fn);
    exit(1);
  }

  struct stat buf;
  int exists;

  exists = lstat(fn, &buf);
  if (exists < 0) {
    fprintf(stderr, "Couldn't stat %s\n", fn);
  } 
  else{ 
    //strips the thing after the last / and sets it to rn

    strip(fn,'/',&rn);
    fn_size = strlen(rn);  
    
    // print Filename size in little endian
    fwrite(&fn_size,4,1,stdout);

    // print Filename
    fwrite(rn,fn_size,1,stdout);

    fwrite(&buf.st_ino,8,1,stdout);
    fwrite(&buf.st_mode,4,1,stdout);
    fwrite(&buf.st_mtime,8,1,stdout);
  }

  closedir(d);
  
  
  memset(fn,0,strlen(fn));
  strncpy(fn,argv[1], strlen(argv[1]) - fn_size);

  // gives the recursive function the absoulte and realative path and the
  // jrb_tree(inodes)
  get_size(fn,rn,inodes);


  JRB tmp_node; 
  jrb_traverse(tmp_node, inodes){
    free(tmp_node->val.v);
  }
  jrb_free_tree(inodes);

  free(rn);

  return 0; 
}

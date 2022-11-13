#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include "jrb.h"
#include "fields.h"
#include "dllist.h"
#include "jval.h"

#define byte 1
//add a struct for ip
typedef struct
{
  unsigned char address[4];
  Dllist names;
} IP;

int main()
{
  int file_int;
  unsigned char *buff;
  //int read;
  JRB machines, tmp_node;
  IP *ip;
  char *Nbuff;

  machines = make_jrb();
  if ((file_int = open("converted", O_RDONLY)) < 0)
  {
    perror("converted");
    return -1;
  }

  buff = (unsigned char *)malloc(256);

  ip = (IP *)malloc(sizeof(IP));
  ip->names = new_dllist();

  unsigned int num_names = 0;
  // read in the ip address and the number of names a byte at a time
  // then read in the names assocaited with the machine
  for (int bcount = 0; bcount < 8 && (read(file_int, buff, 1) == 1); bcount++)
  {
    //reads in ip addres
    if (bcount < 4)
    {
      ip->address[bcount] = (unsigned char)(*buff);
    }

    //reads in the number of names
    if (bcount >= 4)
    {
      num_names |= ((unsigned int)(*buff)) << (8 * (7 - bcount));
    }

    // has already read 8 bytes everything else should be a name
    if (bcount == 7)
    {
      char name[256];
      char absoulte = 'F';
      for (int i = 0; num_names != 0 && (read(file_int, buff, 1) == 1); i++)
      {
        // absoulte is given so strip
        if (*buff == '.')
        {
          absoulte = 'T';
        }
        name[i] = *buff;

        if (*buff == '\0')
        {
           //printf("\n%s   %d.%d.%d.%d    %d", name, ip->address[0],ip->address[1],ip->address[2],ip->address[3], num_names);


          Nbuff = strdup(name);
          dll_append(ip->names, new_jval_v(Nbuff));

          // the name is an absoulte name strip the local name and add it to the list
          if (absoulte == 'T')
          {
            strtok(name, ".");
            Nbuff = strdup(name);
            dll_append(ip->names, new_jval_v(Nbuff));
            //gotta reassign the value of absoulte
            absoulte = 'F';
          }
          num_names--;
          // i need to resert i to -1 because it will increment to 0
          // need to clear name so memset is called
          i = -1;
          memset(name, 0, 256);
        }
      }

      // everything of one machines had been read reset bcount to 0; num_names to 0;
      // and insert the info to tree

      jrb_insert_str(machines, dll_first(ip->names)->val.s, new_jval_v(ip));
      
      // the for loop increments an extra time at the end of the loop
      bcount = -1;
      num_names = 0;
      ip = (IP *)malloc(sizeof(IP));
      ip->names = new_dllist();
    }
  }
  // an extra malloc is done at the end of the lopp so free it
  free_dllist(ip->names);
  free(ip);

  close(file_int);

  free(buff);

  printf("Hosts all read in\n\nEnter host name: ");

  char str[50];
  while (!feof(stdin))
  {
    if(scanf("%s", str) != 1){
      break;
    }

    //tmp_node = jrb_find_str(machines, str);

    Dllist p;
    char boo = 'F';
    jrb_traverse(tmp_node,machines)
    {
      
      dll_traverse(p, ((IP*)(tmp_node->val.v))->names)
      {
        if (strcmp(str, p->val.s) == 0)
        {
          boo = 'T';
          IP *p = tmp_node->val.v;

          printf("%d.%d.%d.%d: ", p->address[0], p->address[1], p->address[2], p->address[3]);
          Dllist pp;
          dll_traverse(pp, p->names)
          {
            printf("%s ", pp->val.s);
          }
          printf("\n\n");
        }
      }
    }

    if (boo == 'F')
    {
      printf("no key %s\n", str);
      printf("\nEnter host name: ");
      continue;
    }
    printf("Enter host name: ");
  }

  //freeing the memory allocated by traversing the list. Notice this is done right before
  //the end of the program that is to avoid deleting information that is needed
 
  Dllist node;
  jrb_traverse(tmp_node, machines)
  {
    ip = tmp_node->val.v;
    dll_traverse(node, ip->names)
    {
      free(node->val.s);
      //dll_delete_node(node);
    }
    free_dllist(ip->names);
    //freed all the stuff in ip now free ip
    free(ip);
  }
  jrb_free_tree(machines);  
  return 0;
}

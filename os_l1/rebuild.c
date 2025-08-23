#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define FILENAME "foodep.txt"
#define ARRAY "done.txt"

void procnode(int Rnode) {
  FILE * fp;
  int Tnode, node;
  char name[3], vis[20], cname[32], nline[256], line[256], * cp, * pcp;
  pid_t mypid, cpid;

  fp = (FILE * ) fopen(FILENAME, "r");
  if (fp == NULL) {
    fprintf(stderr, "Unable to open data file\n");
    exit(2);
  }

  mypid = getpid();
  fgets(line, 256, fp);
  cp = line;
  sscanf(cp, "%d", & Tnode);
  while (Tnode--) {
    fgets(line, 256, fp);
    cp = line;
    sscanf(cp, "%d", & node);
    sscanf(cp, "%s", name);
    cp += strlen(name) + 1;
    pcp = cp;
    if (node == Rnode) {
      FILE * rfp = (FILE *) fopen(ARRAY, "r");
      fgets(nline, 256, rfp);
      sscanf(nline, "%s", vis);
      fclose(rfp);
      if (vis[node - 1] == '1') break;
      vis[node - 1] = '1';
      rfp = (FILE * ) fopen(ARRAY, "w");
      fputs(vis, rfp);
      fclose(rfp);
      while (sscanf(cp, "%s", cname) == 1) {
        if (cpid = fork()) {
          waitpid(cpid, NULL, 0);
        } else {
          execlp("./rebuild", "./rebuild", cname, "adithya", NULL);
        }
        cp += strlen(cname) + 1;
      }
      int j = 0;
      printf("foo%d rebuilt ", node);
      while (sscanf(pcp, "%s", cname) == 1) {
        if (!j) {
          printf("from foo%s ", cname);
          j = 1;
        } else {
          printf(",foo%s", cname);
        }
        pcp += strlen(cname) + 1;
      }
      printf("\n");
    }
  }
}

int main(int argc, char * argv[]) {
  if (argc == 1) {
    procnode(10);
  } else if (argc == 2) {
    FILE * fp1 = (FILE * ) fopen(FILENAME, "r");
    if (fp1 == NULL) {
      fprintf(stderr, "Unable to open data file\n");
      exit(2);
    }
    char line[256];
    char * cp;
    fgets(line, 256, fp1);
    cp = line;
    int k;
    sscanf(cp, "%d", & k);
    fclose(fp1);
    FILE * file = fopen(ARRAY, "w");
    if (file == NULL) {
      // Check for errors opening the file
      printf("Could not open the file for writing.\n");
      return 1;
    }
    for (int i = 0; i < k; i++) {
      fprintf(file, "%d", 0); // Write each element on a new line
    }
    fclose(file);
    procnode(atoi(argv[1]));
  } else {
    procnode(atoi(argv[1]));
  }

  exit(0);
}
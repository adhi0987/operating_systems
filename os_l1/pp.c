#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILENAME "foodep.txt"

void foo (int myNum)
{
   FILE *fp;
   int i,num;
   char name[32],visited[32], cNum[32],line[256],line1[256], *cp,*pcp;
   pid_t cpid;

   fp = (FILE *)fopen(FILENAME, "r");
   if (fp == NULL) 
   {
      fprintf(stderr, "Unable to open data file\n");
      exit(2);
   }
   fgets(line, 256, fp);
   cp = line;
   sscanf(cp, "%d", &num);
   // printf("%d\n",num);
   while (num--) 
   {
      fgets(line, 256, fp);
      cp = line;
      sscanf(cp, "%d", &i);
      sscanf(cp, "%s", name);
      cp += strlen(name) + 1;
      pcp = cp;
      // printf("%d\n",i);
      if (i == myNum) 
      {
         // printf("%d\n",i);
         FILE *file = fopen("done.txt", "r");  // Open the file for writing

         if (file == NULL) {
            perror("Error opening file");
            exit(3) ;
         }

         fgets(line1, 256, file);
         sscanf(line1, "%s", visited);
         fclose(file);

         // printf("%s %d pra ",visited,i);
         // printf("%c bro",visited[i-1]);

         if(visited[i-1]=='1') break;


         visited[i-1]='1';
         file = fopen("done.txt", "w");  // Open the file for writing

         if (file == NULL) {
            perror("Error opening file");
            exit(3) ;
         }

         fputs(visited,file);
         fclose(file);

         while(sscanf(cp, "%s", cNum)==1)
         {
            // printf("%s prabhash\n",cNum);
            cp += strlen(cNum) + 1;
            if ((cpid = fork())) 
            {
               waitpid(cpid, NULL, 0);
            } 
            else 
            {
               execlp("./rebuild", "./rebuild", cNum,"1",NULL);
            }
         }
         printf("foo%d rebuilt ", myNum);
         int j=0;
         while(sscanf(pcp, "%s", cNum)==1)
         {
            if(j==0) printf("from foo%s",cNum);
            else printf(",foo%s",cNum);
            j=1;
            pcp += strlen(cNum) + 1;
         }
         printf("\n");
         
         break;
      }
   }

   fclose(fp);
   return;
}

int main ( int argc, char *argv[] )
{
   if (argc == 1) 
   {
      FILE *fp;
      fp = (FILE *)fopen(FILENAME, "r");

      if (fp == NULL) 
      {
         fprintf(stderr, "Unable to open data file\n");
         exit(2);
      }

      char line[10];
      int k;

      fgets(line,10,fp);
      sscanf(line, "%d", &k);
      // printf("%d\n",k);

      fclose(fp);

      FILE *file = fopen("done.txt", "w");  // Open the file for writing

      if (file == NULL) {
         perror("Error opening file");
         return 1;
      }

      for (int i = 0; i < k; i++) {
         fputc('0', file);  // Write a '0' character to the file
      }

      fclose(file);
      
      foo(10);
   }
   else if(argc==2)
   {
      FILE *fp;
      fp = (FILE *)fopen(FILENAME, "r");

      if (fp == NULL) 
      {
         fprintf(stderr, "Unable to open data file\n");
         exit(2);
      }

      char line[10];
      int k;

      fgets(line,10,fp);
      sscanf(line, "%d", &k);
      // printf("%d\n",k);

      fclose(fp);

      FILE *file = fopen("done.txt", "w");  // Open the file for writing

      if (file == NULL) {
         perror("Error opening file");
         return 1;
      }

      for (int i = 0; i < k; i++) {
         fputc('0', file);  // Write a '0' character to the file
      }

      fclose(file);

      foo(atoi(argv[1]));
      // printf("done\n");
   }
   else if(argc==3)
   {
      foo(atoi(argv[1]));
   }
   exit(0);
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "t2bc.h"

int main(int argc, const char *argv[])
{
   const char *inName, *outName;
   FILE *inFile, *outFile;
   char str[256], *begin, *end,
        *type, *string, *line, *op;

   if (argc != 3)
   {
      printf("\nUsage:  TOK2TOK <input.toks> <output.inc>\n");
      return 1;
   }
   inName = argv[1],
   outName = argv[2];

   inFile = fopen (inName, "r");
   if (inFile == NULL)
   {
      printf("Unable to open input file '%s': %s", inName, strerror (errno));
      return 1;
   }
   outFile = fopen (outName, "w");
   if (outFile == NULL)
   {
      printf("Unable to open output file '%s': %s", outName, strerror (errno));
      return 1;
   }

   while (fgets(str, sizeof(str), inFile) != NULL)
   {
      if (str[0] == ' ')
      {
         begin = strchr(str, ':');
         begin = strchr(begin, '\'') + 1;
         end   = strchr(begin, '\'');
         type = begin;
         *end = 0;

         begin = strchr(end+1, ':');
         begin = strchr(begin, '\'') + 1;
         end   = strchr(begin, '\'');
         string = begin;
         *end = 0;

         begin = strchr(end+1, '[') + 1;
         end   = strchr(begin, ',');
         line = begin;
         *end = 0;

         begin = strchr(end+1, ':');
         begin = strchr(begin+1, ':');
         begin = strchr(begin, '\'') + 1;
         if (begin[0] == '\'')
            op = "NONE";
         else
         {
            end = strchr(begin, '\'');
            op = begin;
            *end = 0;
         }

         fprintf (outFile, "{%-10s, \"%s\", %s, %s},\n", type, string, line, op);
      }
   }

   fclose(inFile);
   fclose(outFile);

	return 0;
}


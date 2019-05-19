/*
Sourse Python text:

class Point:
    def __init__(self, x=0, y=0):
        self.x = x
        self.y = y
    def right(self):
        self.x = self.x+1

scuter=Point()
.....

scuter.right()

*/

typedef struct LEX_
{
   char tag;
   union {
      char string[50];
      int  number;
      char symbol;
   } v;
} LEX;

/* My input: */
LEX inText[];

///////////////////////////

typedef enum OP_CODE_ {
   LOAD_GLOBAL,
   LOAD_ATTR,
   CALL_FUNCTION,
   POP_TOP,
//   ...
} OP_CODE;

typedef struct INSTRUCTION_
{
   OP_CODE  op_code;
   unsigned line;
   unsigned index;
   union {
      char string[50];
      int  number;
   } argument;
} INSTRUCTION;

/* My output: */
INSTRUCTION outText[];


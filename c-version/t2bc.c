
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "t2bc.h"

LEX in[] = {
   #include "token2.inc"
};

static PrintRPN (void);

STR buf1[128], buf2[128], buf3[128];
STR_LIST constList = {buf1, 0};
STR_LIST globalList = {buf2, 0};
STR_LIST localList = {buf3, 0};

LEX_EX buf4[128], buf5[128];
STACK rpn = {buf4, 0};
STACK stack = {buf5, 0};

static unsigned DefHendler (unsigned i)
{
   unsigned rv, indentCount;

   i++;

   printf ("%u\t", in[i].line);
   printf ("LOAD_CONST         \t %u ('%s')\n", constList.n, in[i].string);
   printf ("LOAD_CONST         \t %u ('%s')\n", constList.n+1, in[i].string);
   printf ("MAKE_FUNCTION      \t 0\n");
   printf ("STORE_NAME         \t %u (%s)\n", constList.n, in[i].string);

   Add2List(constList, in[i].string);
   Add2List(constList, in[i].string);
   Add2List(globalList, in[i].string);

   /* formal parameters */
   for (; in[i].type != OP || in[i].op != RPAR; i++)
      if (in[i].type == NAME)
         Add2List(localList, in[i].string);
   for (; in[i].type != INDENT; i++);
   indentCount = 1;

   rv = i;

   for (i++; indentCount != 0; i++)
   {
      if (in[i].type == INDENT)
         indentCount++;
      else if (in[i].type == DEDENT)
         indentCount--;
      else if (in[i].type == NAME)
      {
         if (in[i+1].type == OP)
            switch (in[i+1].op)
            {
               case LPAR:  /* go to next line */
                  for (i++; in[i].type != NEWLINE; i++);
                  break;
               case LSQB:  /* skip array item */
               {
                  unsigned sqbCount = 1;
                  for (i+=2; sqbCount; i++)
                     if (in[i].type == OP)
                     {
                        if (in[i].op == RSQB)
                           sqbCount--;
                        else if (in[i].op == LSQB)
                           sqbCount++;
                     }
                  break;
               }
               case RPAR:
               case COMMA: /* Save name */
                  Add2List(localList, in[i].string);
                  break;
               case EQUAL: /* Save name and go to next line */
                  Add2List(localList, in[i].string);
                  for (i++; in[i].type != NEWLINE; i++);
                  break;
            }
      }
   }

   return rv;
}

static void Step1 (void)
{
   unsigned i;
   unsigned indentCount;
   bool local, leftPart = true;
   LEX_EX lexEx;
   int flags = 0;

   for (i=0; true; i++)
   {
      lexEx.l = &in[i];

      lexEx.flags = leftPart ? L_PART : R_PART;
      /* %= &= **= *= += -= //= /= */
      if (SLASHEQUAL >= in[i+1].op && in[i+1].op >= PERCENTEQUAL)
         lexEx.flags |= R_PART;

      if (in[i].type == OP)
      {
         lexEx.priority = in[i].op;
         if ((in[i].op == PLUS || in[i].op == MINUS) && i > 1 &&
             in[i-1].type != NAME && in[i-1].type != NUMBER && in[i-1].type != STRING)
            lexEx.priority = TILDE;
      }
      switch (in[i].type)
      {
         case NAME:
            if (strcmp (in[i].string, "def") == 0)
            {
               i = DefHendler (i);
               local = true;
               break;
            }
            //...
            /* usual NAME */


         case NUMBER:
         case STRING:
            Push(rpn, &lexEx);
            break;
         case OP:
            /* %= &= **= *= += -= //= /= = */
            if (EQUAL >= in[i].op && in[i].op >= PERCENTEQUAL)
            {
               leftPart = false;
            }

            while (stack.n && TOS(stack)->type == OP &&
                   TOS(stack)->op < lexEx.priority)
               if (in[i].op != LPAR)
                  Push(rpn, Pop(stack));
               else
                  Pop(stack);

            if (in[i].op > RPAR)
               Push(stack, &lexEx);
            break;
         case NEWLINE:
            while (stack.n)
               Push(rpn, Pop(stack));
            Push(rpn, &lexEx);
            leftPart = true;
            break;
         case ENDMARKER:
            Push(rpn, &lexEx);
            return;
      }
   }
}

static char prefix[30];
static bool newLine;

static int GetNameIndex (STR str, bool def, bool *local)
{
   unsigned i;

   *local = false;

   if (def)
   {
      for (i=0; i<localList.n; i++)
         if (strcmp (localList.s[i], str) == 0)
         {
            *local = true;
            return i;
         }
   }
   for (i=0; i<globalList.n; i++)
      if (strcmp (globalList.s[i], str) == 0)
         return i;
   strcpy (globalList.s[i], str);
   globalList.n++;
   return i;
}

static int GetConstIndex (STR_LIST *list, STR str)
{
   unsigned i;

   for (i=0; i<list->n; i++)
      if (strcmp (list->s[i], str) == 0)
         return i;
   strcpy (list->s[i], str);
   list->n++;
   return i;
}

static void PrintPrefix (LEX *lex)
{
   if (newLine)
   {
      printf ("%3u%10s", lex->line, " ");
      newLine = false;
   }
   else
      printf ("%13s", " ");
}
static void Load (LEX *lex, bool def)
{
   bool local;
   char *nameType;
   unsigned j;

   if (lex->type == RESULT)
      return;

   PrintPrefix (lex);
   switch (lex->type)
   {
      case NUMBER: case STRING:
         j = GetConstIndex (&constList, lex->string);
         printf ("LOAD_CONST          %u (%s)\n",
                  j, lex->string);
         break;
      case NAME:
         j = GetNameIndex (lex->string, def, &local);
         nameType = def ? (local ? "FAST  " : "GLOBAL") : ("NAME  ");
         printf ("LOAD_%s         %u (%s)\n",
                  nameType, j, lex->string);
         break;
      default:
         assert(0);
   }
}

static void Store (LEX *lex, bool def)
{
   bool local;
   char *nameType;
   unsigned j;

   assert (lex->type == NAME);
   j = GetNameIndex (lex->string, def, &local);
   nameType = def ? "FAST" : "NAME";
   PrintPrefix (lex);
   printf ("STORE_%s          %u (%s)\n",
            nameType, j, lex->string);
}

static LEX resLex = {RESULT, "result", 0, NONE};
static const LEX_EX result = {&resLex, 0};

static void Step2(void)
{
   unsigned i;
   int j;
   bool def = false;
   bool local;
   char *nameType;

   newLine = true;
   for (i=0; i<rpn.n; i++)
   {
      switch (rpn.lex[i].l->type)
      {
         case NAME:
         case NUMBER:
         case STRING:
            if (rpn.lex[i].flags & L_PART)
               Push(stack, &rpn.lex[i]);
            if (rpn.lex[i].flags & R_PART)
               Load(rpn.lex[i].l, def);
            break;
         case OP:
            /* %= &= **= *= += -= //= /= = */
            if (EQUAL >= rpn.lex[i].l->op && rpn.lex[i].l->op >= PERCENTEQUAL)
            {
               if (rpn.lex[i].l->op != EQUAL)
               {
                  PrintPrefix (rpn.lex[i].l);
                  switch (rpn.lex[i].l->op)
                  {
                     case PERCENTEQUAL:    /* %=   */
                           printf ("INPLACE_MODULO\n"); break;
                     case AMPEREQUAL:      /* &=   */
                           printf ("INPLACE_AND\n"); break;
                     case DOUBLESTAREQUAL: /* **=  */
                           printf ("INPLACE_POWER\n"); break;
                     case STAREQUAL:       /* *=   */
                           printf ("INPLACE_MULTIPLY\n"); break;
                     case PLUSEQUAL:       /* +=   */
                           printf ("INPLACE_ADD\n"); break;
                     case MINEQUAL:        /* -=   */
                           printf ("INPLACE_SUBTRACT\n"); break;
                     case DOUBLESLASHEQUAL:/* //=  */
                           printf ("INPLACE_FLOOR_DIVIDE\n"); break;
                     case SLASHEQUAL:      /* /=   */
                           printf ("INPLACE_TRUE_DIVIDE\n"); break;
                  }
               }
               assert (stack.n > 0);
               Store (TOS(stack), def);
               Pop(stack);
            }
            else if ((EQEQUAL >= rpn.lex[i].l->op && rpn.lex[i].l->op >= PERCENT)
                      || rpn.lex[i].l->op == DOUBLESTAR)
            {
               unsigned opNum;
               char     *opName;
               PrintPrefix (rpn.lex[i].l);
               switch (rpn.lex[i].l->op)
               {
                  case PLUS: /* + */
                     if (rpn.lex[i].priority == TILDE)
                        printf ("UNARY_POSITIVE\n");
                     else
                        printf ("BINARY_ADD\n");
                     break;
                  case MINUS: /* - */
                     if (rpn.lex[i].priority == TILDE)
                        printf ("UNARY_NEGATIVE\n");
                     else
                     printf ("BINARY_SUBTRACT\n"); break;
                  case STAR: /* * */
                     printf ("BINARY_MULTIPLY\n"); break;
                  case SLASH: /* / */
                     printf ("BINARY_TRUE_DIVIDE\n"); break;
                  case DOUBLESLASH: /* // */
                     printf ("BINARY_FLOOR_DIVIDE\n"); break;
                  case PERCENT: /* % */
                     printf ("BINARY_MODULO\n"); break;

                  case LESS: /* < */
                     opNum = 0; opName = "<"; goto any_compare;
                  case LESSEQUAL: /* <= */
                     opNum = 1; opName = "<="; goto any_compare;
                  case GREATER: /* > */
                     opNum = 4; opName = ">"; goto any_compare;
                  case GREATEREQUAL: /* >= */
                     opNum = 5; opName = ">="; goto any_compare;
                  case NOTEQUAL: /* != */
                     opNum = 3; opName = "!="; goto any_compare;
                  case EQEQUAL: /* == */
                     opNum = 2; opName = "==";
               any_compare:
                     printf ("COMPARE_OP          %u (%s)\n", opNum, opName);
                     break;

                  default:
                     return;
               }
            }
            break;

         case NEWLINE:
            printf ("\n");
            newLine = true;
            break;

         case ENDMARKER:
         {
            unsigned j;
            newLine = false;
            j = GetConstIndex (&constList, "None");
            PrintPrefix (rpn.lex[i].l);
            printf ("LOAD_CONST          %u (None)\n", j);
            PrintPrefix (rpn.lex[i].l);
            printf ("RETURN_VALUE\n");
            return;
         }
      }
   }
}

int main(void)
{
   unsigned i;

   Step1();
//   PrintRPN();
   Step2();

   printf ("   consts\n");
   for (i=0; i<constList.n; i++)
      printf ("      %s\n", constList.s[i]);

   printf ("   names (");
   if (globalList.n)
   {
      for (i=0; i<globalList.n-1; i++)
         printf ("'%s', ", globalList.s[i]);
      printf ("'%s'", globalList.s[i]);
   }
   printf (")\n");

   return 0;
}

/* for debugging */
static PrintRPN (void)
{
   unsigned i;

   for (i=0; i<rpn.n; i++)
      printf ("%3u  type: %3u  '%15s'   op: %3u;  flags: %u\n",
       rpn.lex[i].l->line, rpn.lex[i].l->type, rpn.lex[i].l->string, rpn.lex[i].l->op,
       rpn.lex[i].flags);
}

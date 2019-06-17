
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "t2bc.h"

LEX in[] = {
   #include "token2.inc"
};

BYTECODE *out;

char *buf1[128], *buf2[128], *buf3[128];
STR_LIST constList = {buf1, 0};
STR_LIST globalList = {buf2, 0};
STR_LIST localList = {buf3, 0};

LEX_EX buf4[128], buf5[128];
LEX_STACK rpn = {buf4, 0};
LEX_STACK stack = {buf5, 0};

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
            Push(rpn, lexEx);
            break;
         case OP:
            /* %= &= **= *= += -= //= /= = */
            if (EQUAL >= in[i].op && in[i].op >= PERCENTEQUAL)
            {
               leftPart = false;
            }

            if (in[i].op == LPAR)
               Push(stack, lexEx);
            else if (in[i].op == RPAR)
            {
               while (stack.n && Tos(stack).l->type == OP &&
                      Tos(stack).l->op != LPAR)
                  Push(rpn, Pop(stack));
               Pop(stack);
            }
            else
            {
               while (stack.n && Tos(stack).l->type == OP &&
                      Tos(stack).l->op <= lexEx.priority)
                  if (Tos(stack).l->op != LPAR)
                     Push(rpn, Pop(stack));
                  else
                     break;
               Push(stack, lexEx);
            }
            break;
         case NEWLINE:
            while (stack.n)
               Push(rpn, Pop(stack));
            Push(rpn, lexEx);
            leftPart = true;
            break;
         case ENDMARKER:
            Push(rpn, lexEx);
            return;
      }
   }
}

static bool newLine;
static int pc = -1;/* program counter */

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
   globalList.s[i] = str;
   globalList.n++;
   return i;
}

static int GetConstIndex (STR_LIST *list, STR str)
{
   unsigned i;

   for (i=0; i<list->n; i++)
      if (strcmp (list->s[i], str) == 0)
         return i;
   list->s[i] = str;
   list->n++;
   return i;
}

static void PrintPrefix (const LEX *lex)
{
   pc++;
   if (newLine)
   {
      out[pc].line = lex->line;
      newLine = false;
   }
   else
      out[pc].line = -1;
   out[pc].pc = pc;
   out[pc].param = -1;
   out[pc].comment = NULL;
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
         out[pc].comm = "LOAD_CONST";
         out[pc].param = j;
         out[pc].comment = lex->string;
         break;
      case NAME:
         j = GetNameIndex (lex->string, def, &local);
         out[pc].comm = def ? (local ? "LOAD_FAST" : "LOAD_GLOBAL") : ("LOAD_NAME");
         out[pc].param = j;
         out[pc].comment = lex->string;
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
   PrintPrefix (lex);
   out[pc].comm = def ? "STORE_FAST" : "STORE_NAME";
   out[pc].param = j;
   out[pc].comment = lex->string;
}

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
      switch (rpn.slot[i].l->type)
      {
         case NAME:
         case NUMBER:
         case STRING:
            if (rpn.slot[i].flags & L_PART)
               Push(stack, rpn.slot[i]);
            if (rpn.slot[i].flags & R_PART)
               Load(rpn.slot[i].l, def);
            break;
         case OP:
            /* %= &= **= *= += -= //= /= = */
            if (EQUAL >= rpn.slot[i].l->op && rpn.slot[i].l->op >= PERCENTEQUAL)
            {
               if (rpn.slot[i].l->op != EQUAL)
               {
                  PrintPrefix (rpn.slot[i].l);
                  switch (rpn.slot[i].l->op)
                  {
                     case PERCENTEQUAL:    /* %=   */
                           out[pc].comm = "INPLACE_MODULO"; break;
                     case AMPEREQUAL:      /* &=   */
                           out[pc].comm = "INPLACE_AMD"; break;
                     case DOUBLESTAREQUAL: /* **=  */
                           out[pc].comm = "INPLACE_POWER"; break;
                     case STAREQUAL:       /* *=   */
                           out[pc].comm = "INPLACE_MULTIPLY"; break;
                     case PLUSEQUAL:       /* +=   */
                           out[pc].comm = "INPLACE_ADD"; break;
                     case MINEQUAL:        /* -=   */
                           out[pc].comm = "INPLACE_SUBTRACT"; break;
                     case DOUBLESLASHEQUAL:/* //=  */
                           out[pc].comm = "INPLACE_FLOOR_DIVIDE"; break;
                     case SLASHEQUAL:      /* /=   */
                           out[pc].comm = "INPLACE_TRUE_DIVIDE"; break;
                  }
               }
               assert (stack.n > 0);
               Store (Tos(stack).l, def);
               Pop(stack);
            }
            else if ((EQEQUAL >= rpn.slot[i].l->op && rpn.slot[i].l->op >= PERCENT)
                      || rpn.slot[i].l->op == DOUBLESTAR)
            {
               unsigned opNum;
               char     *opName;
               PrintPrefix (rpn.slot[i].l);
               switch (rpn.slot[i].l->op)
               {
                  case PLUS: /* + */
                     if (rpn.slot[i].priority == TILDE)
                        out[pc].comm = "UNARY_POSITIVE";
                     else
                        out[pc].comm = "BINARY_ADD";
                     break;
                  case MINUS: /* - */
                     if (rpn.slot[i].priority == TILDE)
                        out[pc].comm = "UNARY_NEGATIVE";
                     else
                        out[pc].comm = "BINARY_SUBTRACT";
                     break;
                  case STAR: /* * */
                     out[pc].comm = "BINARY_MULTIPLY"; break;
                  case SLASH: /* / */
                     out[pc].comm = "BINARY_TRUE_DIVIDE"; break;
                  case DOUBLESLASH: /* // */
                     out[pc].comm = "BINARY_FLOOR_DIVIDE"; break;
                  case PERCENT: /* % */
                     out[pc].comm = "BINARY_MODULO"; break;

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
                     out[pc].comm = "COMPARE_OP";
                     out[pc].param = opNum;
                     out[pc].comment = opName;
                     break;

                  default:
                     return;
               }
            }
            break;

         case NEWLINE:
            newLine = true;
            break;

         case ENDMARKER:
         {
            unsigned j;
            newLine = false;
            j = GetConstIndex (&constList, "None");
            PrintPrefix (rpn.slot[i].l);
            out[pc].comm = "LOAD_CONST";
            out[pc].param = j;
            out[pc].comment = "None";
            PrintPrefix (rpn.slot[i].l);
            out[pc].comm = "RETURN_VALUE";
            out[pc].param = -1;
            out[pc].comment = NULL;
            return;
         }
      }
   }
}

static void PrintRPN (void);
static void PrintResult(void);
int main(void)
{
   unsigned i;

   Step1();
//   PrintRPN();

   out = malloc (1024*sizeof(BYTECODE));
   assert(out);
   Step2();
   PrintResult();
   free (out);

   return 0;
}

/* for debugging */
static void PrintRPN (void)
{
   unsigned i;

   for (i=0; i<rpn.n; i++)
      printf ("%3u  type: %3u  '%15s'   op: %3u;  flags: %u\n",
       rpn.slot[i].l->line, rpn.slot[i].l->type, rpn.slot[i].l->string, rpn.slot[i].l->op,
       rpn.slot[i].flags);
}

static void PrintResult(void)
{
   unsigned i;

   for (i=0; i<=pc; i++)
   {
      if (out[i].line != -1)
         printf ("\n%3d", out[i].line);
      else
         printf ("   ");

      printf ("%9u %-18s", 2*out[i].pc, out[i].comm);

      if (out[i].param != -1)
      {
         printf ("%3u", out[i].param);
         if (out[i].comment)
            printf (" (%s)", out[i].comment);
      }
      printf ("\n");
   }

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
}


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "t2bc.h"

LEX in[] = {
//   #include "code_test.inc"
//   #include "function1.inc"
   #include "token1.inc"
//   #include "token2.inc"
//   #include "token2.inc"
//   #include "token3.inc"
//   #include "token4.inc"
//   #include "token5.inc"
//   #include "token6.inc"
//   #include "token9.inc"
};

BYTECODE *out;

static FUNCTIONS_TABLE ft;
static unsigned cf = 0;

static void InitFunTab(FUNCTIONS_TABLE *functionsTable)
{
   functionsTable->entry[0].name = "Main";
   functionsTable->entry[0].local.s = malloc (sizeof(char *) * MAX_OF_LOCAL);
   functionsTable->entry[0].local.n = 0;
   assert (functionsTable->entry[0].local.s);

   functionsTable->entry[0].global.s = malloc (sizeof(char *) * MAX_OF_GLOBAL);
   functionsTable->entry[0].global.n = 0;
   assert (functionsTable->entry[0].global.s);

   functionsTable->consts.s = malloc (sizeof(char *) * MAX_OF_CONST);
   functionsTable->consts.n = 0;
   assert (functionsTable->consts.s);

   functionsTable->n = 1;
}
static void FreeFunTab(FUNCTIONS_TABLE *functionsTable)
{
   unsigned i;

   for (i=0; i<functionsTable->n; i++)
   {
      free (functionsTable->entry[i].local.s);
      free (functionsTable->entry[i].global.s);
   }
   free (functionsTable->consts.s);
}

LEX_EX buf4[512], buf5[128];
LEX_STACK rpn = {buf4, 0};
LEX_STACK stack = {buf5, 0};

unsigned buf6[32];
ARG_COUNT_STACK argCountStack = {buf6, 0};

PC_SLOT buf7[32];
PC_STACK pcStack = {buf7, 0};

KEYWORD buf8[32];
INDENT_STACK indentStack = {buf8, 0};


#define RecodeAndPush(s,key) \
do {                         \
   in[i].type = KWORD;       \
   in[i].op = key;           \
   Push(s, lexEx);           \
} while(0)

static void Step1 (void)
{
   unsigned i;
   unsigned indentCount;
   bool leftPart = true, forLoop = false, whileLoop = false, ifFlag = false,
        elseFlag = false, defFlag = false;
   LEX_EX lexEx;
   int flags = 0;

   for (i=0; true; i++)
   {
      lexEx.l = &in[i];
      lexEx.flags = 0;

      if (in[i+1].type == OP)
      {
         if (in[i+1].op == LPAR)
         {
            leftPart = false;
            lexEx.flags = R_PART;
         }
         /* %= &= **= *= += -= //= /= */
         else if (SLASHEQUAL >= in[i+1].op && in[i+1].op >= PERCENTEQUAL)
            lexEx.flags = L_PART | R_PART;
      }
      lexEx.flags |= leftPart ? L_PART : R_PART;

      if (in[i].type == OP)
      {
         lexEx.priority = in[i].op;
         if ((in[i].op == PLUS || in[i].op == MINUS) && i > 1 &&
             in[i-1].type != NAME && in[i-1].type != NUMBER && in[i-1].type != STRING)
         {
            if (in[i+1].type == NUMBER)
            {
               char buf[50];
               if (in[i].op == MINUS)
                  strcpy (buf, "-");
               else
                  buf[0] = 0;
               strcat (buf, in[i+1].string);
               strcpy (in[i+1].string, buf);
               lexEx.l = &in[i+1];
               i++;
            }
            else
               lexEx.priority = TILDE;
         }
      }
      switch (in[i].type)
      {
         case NAME:
            if (strcmp (in[i].string, "def") == 0)
            {
               RecodeAndPush(rpn, DEF);
               defFlag = true;
               break;
            }
            if (strcmp (in[i].string, "return") == 0)
            {
               if (in[i+1].type != ENDMARKER)
               {
                  RecodeAndPush(stack, RETURN);
                  leftPart = false;
               }
               break;
            }
            else if (strcmp (in[i].string, "for") == 0)
            {
               RecodeAndPush(rpn, FOR);
               forLoop = true;
               break;
            }
            else if (strcmp (in[i].string, "in") == 0)
            {
               RecodeAndPush(stack, IN);
               leftPart = false;
               break;
            }
            else if (strcmp (in[i].string, "while") == 0)
            {
               RecodeAndPush(rpn, WHILE);
               whileLoop = true;
               leftPart = false;
               break;
            }
            else if (strcmp (in[i].string, "if") == 0)
            {
               RecodeAndPush(rpn, IF);
               ifFlag = true;
               leftPart = false;
               break;
            }
            else if (strcmp (in[i].string, "elif") == 0)
            {
               RecodeAndPush(rpn, ELIF);
               ifFlag = true;
               elseFlag = true;
               leftPart = false;
               break;
            }
            else if (strcmp (in[i].string, "else") == 0)
            {
               RecodeAndPush(rpn, ELSE);
               elseFlag = true;
               leftPart = false;
               break;
            }
            else if (strcmp (in[i].string, "True") == 0)
            {
               in[i].type = KWORD;
               in[i].op = TRUE;
               if (i && (in[i-1].type != KWORD || in[i-1].op != WHILE))
                  Push(rpn, lexEx);
               break;
            }
            else if (strcmp (in[i].string, "break") == 0)
            {
               RecodeAndPush(rpn, BREAK);
               leftPart = false;
               break;
            }
            //...
            /* usual NAME */

         case NUMBER:
         case STRING:
            Push(rpn, lexEx);
            break;
         case OP:
            if (in[i].op == COLON) // :
               break;

            /* %= &= **= *= += -= //= /= = */
            if (EQUAL >= in[i].op && in[i].op >= PERCENTEQUAL)
            {
               leftPart = false;
            }

            if (in[i].op == LPAR)
            {
               if (i && in[i-1].type == NAME)
               {
                  lexEx.flags |= F_FLAG;
                  Push(argCountStack, in[i-1].type == OP ? 0 : 1);
               }
               Push(stack, lexEx);
            }
            else if (in[i].op == RPAR)
            {
               while (stack.n && Tos(stack).l->type == OP &&
                      Tos(stack).l->op != LPAR)
                  Push(rpn, Pop(stack));
               if (stack.n && (Tos(stack).flags & F_FLAG))
               {
                  Tos(stack).l->type = FUNCTION;
                  Tos(stack).l->op = Pop(argCountStack);
                  Push(rpn, Pop(stack));
               }
               else
                  Pop(stack);
            }
            else if (in[i].op == COMMA)
            {
               while (stack.n && Tos(stack).l->type == OP &&
                      Tos(stack).l->op != LPAR)
                  Push(rpn, Pop(stack));
               if (stack.n && (Tos(stack).flags & F_FLAG))
                  Tos(argCountStack)++;
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

         case INDENT:
            if (forLoop)
            {
               lexEx.flags |= F_FOR;
               forLoop = false;
            }
            else if (whileLoop)
            {
               lexEx.flags |= F_WHILE;
               whileLoop = false;
            }
            else if (ifFlag)
            {
               lexEx.flags |= F_IF;
               ifFlag = false;
            }
            else if (elseFlag)
            {
               lexEx.flags |= F_ELSE;
               elseFlag = false;
            }
            else if (defFlag)
            {
               lexEx.flags |= F_DEF;
               defFlag = false;
            }
         case DEDENT:
            Push(rpn, lexEx);
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

static bool newLine, label = false;
static int pc = -1;/* program counter */

static int GetNameIndex (STR str, bool def, bool *local)
{
   unsigned i;

   *local = false;

   if (def)
   {
      for (i=0; i<ft.entry[cf].local.n; i++)
         if (strcmp (ft.entry[cf].local.s[i], str) == 0)
         {
            *local = true;
            return i;
         }
   }
   for (i=0; i<ft.entry[cf].global.n; i++)
      if (strcmp (ft.entry[cf].global.s[i], str) == 0)
         return i;
   ft.entry[cf].global.s[i] = str;
   ft.entry[cf].global.n++;
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

#define InitComm(lex)   InitCommEx(lex,NULL)
static void InitCommEx (const LEX *lex, char *comm)
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
   out[pc].comm = comm;
   out[pc].param = -1;
   out[pc].comment[0] = 0;
   out[pc].label = label;
   label = false;
}
static void Load (LEX *lex, bool def)
{
   bool local;
   char *nameType;
   unsigned j;

   if (lex->type == RESULT)
      return;

   InitComm (lex);
   switch (lex->type)
   {
      case NUMBER: case STRING: case KWORD:
         j = GetConstIndex (&ft.consts, lex->string);
         out[pc].comm = "LOAD_CONST";
         out[pc].param = j;
         strcpy (out[pc].comment, lex->string);
         break;
      case NAME:
         j = GetNameIndex (lex->string, def, &local);
         out[pc].comm = def ? (local ? "LOAD_FAST" : "LOAD_GLOBAL") : ("LOAD_NAME");
         out[pc].param = j;
         strcpy (out[pc].comment, lex->string);
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
   InitComm (lex);
   out[pc].comm = def ? "STORE_FAST" : "STORE_NAME";
   out[pc].param = j;
   strcpy (out[pc].comment, lex->string);
}

#define MakeEndOfLoop(lable_)  \
do {                                                               \
   InitCommEx (&lex, "JUMP_ABSOLUTE");   /* goto loop begin */     \
   InitCommEx (&lex, "POP_BLOCK");       /* end of loop */         \
   out[pc].label = lable_;                                           \
} while(0)

static unsigned DefHendler (unsigned i)
{
   unsigned rv, indentCount;

   label = false;

   i++;

   assert (rpn.slot[i].l->type == NAME);
   InitCommEx (rpn.slot[i].l, "LOAD_CONST");
   out[pc].param = ft.consts.n;
   strcpy (out[pc].comment, rpn.slot[i].l->string);

   InitCommEx (rpn.slot[i].l, "LOAD_CONST");
   out[pc].param = ft.consts.n+1;
   strcpy (out[pc].comment, rpn.slot[i].l->string);

   InitCommEx (rpn.slot[i].l, "MAKE_FUNCTION");

   InitCommEx (rpn.slot[i].l, "STORE_NAME");
   out[pc].param = ft.entry[0].global.n;
   strcpy (out[pc].comment, rpn.slot[i].l->string);

   Add2List(ft.consts, rpn.slot[i].l->string);
   Add2List(ft.consts, rpn.slot[i].l->string);
   Add2List(ft.entry[0].global, rpn.slot[i].l->string);

   /* start body function */
   cf = ft.n;
   ft.entry[cf].name = rpn.slot[i].l->string;
   i++;

   ft.entry[cf].local.s = malloc (sizeof(char *) * MAX_OF_LOCAL);
   ft.entry[cf].local.n = 0;
   assert (ft.entry[cf].local.s);

   ft.entry[cf].global.s = malloc (sizeof(char *) * MAX_OF_GLOBAL);
   ft.entry[cf].global.n = 0;
   assert (ft.entry[cf].global.s);

   /* formal parameters */
   for (; rpn.slot[i].l->type != FUNCTION; i++)
      if (rpn.slot[i].l->type == NAME)
         Add2List(ft.entry[cf].local, rpn.slot[i].l->string);
   for (; rpn.slot[i].l->type != NEWLINE; i++);
   newLine = true;
   indentCount = 0;

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
                  Add2List(ft.entry[cf].local, in[i].string);
                  break;
               case EQUAL: /* Save name and go to next line */
                  Add2List(ft.entry[cf].local, in[i].string);
                  for (i++; in[i].type != NEWLINE; i++);
                  break;
            }
      }
   }
   ft.n++;

   return rv;
}

static void Step2(void)
{
   unsigned i;
   int j;
   bool def = false;
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
                  InitComm (rpn.slot[i].l);
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
               InitComm (rpn.slot[i].l);
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
                     strcpy (out[pc].comment, opName);
                     if (Tos(indentStack) == WHILE || Tos(indentStack) == IF
                         || Tos(indentStack) == ELIF)
                     {
                        PC_SLOT slot;

                        Pop(indentStack);
                        InitCommEx (rpn.slot[i].l, "POP_JUMP_IF_FALSE");
                        out[pc].param = opNum;

                        slot.pc = pc;
                        slot.type = PC_POP_JUMP_IF_FALSE;
                        Push(pcStack, slot);
                     }
                     break;

                  default:
                     return;
               }
            }
            break;

         case KWORD:
            switch (rpn.slot[i].l->op)
            {
               case FOR:
                  InitCommEx (rpn.slot[i].l, "SETUP_LOOP");
                  {
                     PC_SLOT slot = {pc, PC_SETUP_LOOP};
                     Push(pcStack, slot);
                  }
                  break;
               case IN:
                  InitCommEx (rpn.slot[i].l, "GET_ITER");

                  InitCommEx (rpn.slot[i].l, "FOR_ITER");
                  out[pc].label = true;
                  {
                     PC_SLOT slot = {pc, PC_FOR_ITER};
                     Push(pcStack, slot);
                  }
                  assert (stack.n > 0);
                  Store (Tos(stack).l, def);
                  Pop(stack);
                  break;

               case WHILE:
                  InitCommEx (rpn.slot[i].l, "SETUP_LOOP");
                  {
                     PC_SLOT slot1 = {pc, PC_SETUP_LOOP},
                             slot2 = {pc+1, PC_AFTER_WHILE};
                     Push(pcStack, slot1);
                     Push(pcStack, slot2);
                     Push(indentStack, WHILE);
                  }
                  label = true;         /* set label for next command */
                  break;
               case IF:
                  Push(indentStack, IF);
                  break;
               case ELIF:
                  Push(indentStack, ELIF);
                  break;
               case BREAK:
                  InitCommEx (rpn.slot[i].l, "BREAK_LOOP");
                  break;
               case DEF:
                  i = DefHendler (i);
                  Push(indentStack, DEF);
                  def = true;
                  break;
               case RETURN:
                  InitCommEx (rpn.slot[i].l, "RETURN_VALUE");
                  break;
               case TRUE:
                  Load(rpn.slot[i].l, def);
                  break;
            }
            break;

         case INDENT:
            if (rpn.slot[i].flags & F_FOR)
               Push(indentStack, FOR);
            else if (rpn.slot[i].flags & F_WHILE)
               Push(indentStack, WHILE);
            else if (rpn.slot[i].flags & F_IF)
               Push(indentStack, IF);
            else if (rpn.slot[i].flags & F_ELSE)
               Push(indentStack, ELSE);
            else if (rpn.slot[i].flags & F_DEF)
               Push(indentStack, DEF);
            else
               Push(indentStack, USUALLY);
            break;
         case DEDENT:
         {
            LEX lex = {DEDENT, "", -1, NONE};

            assert(indentStack.n);

            if (rpn.slot[i+1].l->type == KWORD &&
                (rpn.slot[i+1].l->op == ELSE || rpn.slot[i+1].l->op == ELIF))
            {
               PC_SLOT slot;

               InitCommEx (&lex, "JUMP_FORWARD");
            }

            if (Tos(indentStack) == FOR)             /* end of 'for' loop */
            {
               MakeEndOfLoop (true);

               assert (Tos(pcStack).type == PC_FOR_ITER);
               out[pc-1].param = Tos(pcStack).pc;    /* link to begin */

               out[Tos(pcStack).pc].param = pc - Tos(pcStack).pc - 1;
               sprintf (out[Tos(pcStack).pc].comment, "to %u", pc);
               Pop(pcStack);

               assert (Tos(pcStack).type == PC_SETUP_LOOP);
               out[Tos(pcStack).pc].param = pc - Tos(pcStack).pc;
               sprintf (out[Tos(pcStack).pc].comment, "to %u", pc + 1);
               Pop(pcStack);
            }
            else if (Tos(indentStack) == WHILE)      /* end of 'while' loop */
            {
               assert (pcStack.n);
               if (Tos(pcStack).type == PC_POP_JUMP_IF_FALSE)
               {
                  MakeEndOfLoop (true);
                  out[Tos(pcStack).pc].param = pc;          /* link to end of loop */
                  Pop(pcStack);
               }
               else
                  MakeEndOfLoop (false);

               assert (Tos(pcStack).type == PC_AFTER_WHILE);
               out[pc-1].param = Tos(pcStack).pc;           /* link to begin of loop */
               Pop(pcStack);

               assert (Tos(pcStack).type == PC_SETUP_LOOP);
               out[Tos(pcStack).pc].param = pc - Tos(pcStack).pc;  /* link after loop block */
               sprintf (out[Tos(pcStack).pc].comment, "to %u", pc + 1);
               Pop(pcStack);
            }
            else if (Tos(indentStack) == IF)
            {
               unsigned link;

               if (rpn.slot[i+1].l->type == DEDENT &&
                   indentStack.slot[indentStack.n - 2] == WHILE)
                  link = pcStack.slot[pcStack.n - 3].type == PC_AFTER_WHILE ?
                         pcStack.slot[pcStack.n - 3].pc : pcStack.slot[pcStack.n - 4].pc;
               else
                  link = pc + 1;

               if (Tos(pcStack).type == PC_POP_JUMP_IF_FALSE)
               {
                  out[Tos(pcStack).pc].param = link;
                  Pop(pcStack);
               }
            }
            else if (Tos(indentStack) == ELSE)
            {
               char comment[16];

               assert(pcStack.n);

               sprintf (comment, "to %u", pc + 1);
               while (pcStack.n)
               {
                  assert(Tos(pcStack).type == PC_JUMP_FORWARD);
                  out[Tos(pcStack).pc].param = pc - Tos(pcStack).pc;
                  strcpy (out[Tos(pcStack).pc].comment, comment);
                  Pop(pcStack);
               }
            }

            else if (Tos(indentStack) == DEF)             /* end of function body */
            {
               if (rpn.slot[i-1].l->type == KWORD && rpn.slot[i-1].l->op == RETURN)
               {
                  unsigned j;
                  newLine = false;
                  j = GetConstIndex (&ft.consts, "None");
                  InitCommEx (rpn.slot[i].l, "LOAD_CONST");
                  out[pc].param = j;
                  strcpy (out[pc].comment, "None");

                  InitCommEx (rpn.slot[i].l, "RETURN_VALUE");
               }
               def = false;

               InitCommEx (rpn.slot[i].l, "END_OF_FUNCTION_BODY");

               cf = 0;
            }

            newLine = true;
            label = true;         /* set label for next command */
            Pop(indentStack);

            if (strcmp (out[pc].comm, "JUMP_FORWARD") == 0)
            {
               PC_SLOT slot;

               slot.pc = pc;
               slot.type = PC_JUMP_FORWARD;
               Push(pcStack, slot);
            }

            break;
         }
         case NEWLINE:
            newLine = true;
            break;

         case FUNCTION:
            InitCommEx (rpn.slot[i].l, "CALL_FUNCTION");
            out[pc].param = rpn.slot[i].l->op;
            if ((rpn.slot[i+1].l->type != OP || rpn.slot[i+1].l->op != EQUAL) &&
                (rpn.slot[i+1].l->type != KWORD || rpn.slot[i+1].l->op != IN))
            {
               InitCommEx (rpn.slot[i].l, "POP_TOP");
            }
            break;

         case ENDMARKER:
         {
            unsigned j;

            newLine = false;
            j = GetConstIndex (&ft.consts, "None");
            InitCommEx (rpn.slot[i].l, "LOAD_CONST");
            out[pc].param = j;
            strcpy (out[pc].comment, "None");

            InitCommEx (rpn.slot[i].l, "RETURN_VALUE");
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

   InitFunTab(&ft);

   Step1();
//   PrintRPN();
//  return 0;

   out = malloc (1024*sizeof(BYTECODE));
   assert(out);
   Step2();
   PrintResult();
   free (out);
   FreeFunTab(&ft);

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

   cf = 0;

   for (i=0; i<=pc; i++)
   {
      if (strcmp (out[i].comm, "END_OF_FUNCTION_BODY") == 0)
      {
         unsigned j;

         cf++;

         printf ("   name '%s'\n", ft.entry[cf].name);

         printf ("   names (");
         if (ft.entry[cf].global.n)
         {
            for (j=0; j<ft.entry[cf].global.n-1; j++)
               printf ("'%s', ", ft.entry[cf].global.s[j]);
            printf ("'%s'", ft.entry[cf].global.s[j]);
         }
         printf (")\n");

         printf ("   varnames (");
         if (ft.entry[cf].local.n)
         {
            for (j=0; j<ft.entry[cf].local.n-1; j++)
               printf ("'%s', ", ft.entry[cf].local.s[j]);
            printf ("'%s'", ft.entry[cf].local.s[j]);
         }
         printf (")\n");

         printf ("\n**************** END_OF_FUNCTION_BODY ****************\n\n");
      }
      else
      {
         if (out[i].line != -1)
            printf ("\n%3d", out[i].line);
         else
            printf ("   ");

         if (out[i].label)
            printf ("%7s", ">>");
         else
            printf ("%7s", " ");

         printf ("%5u %-23s", 2*out[i].pc, out[i].comm);

         if (out[i].param != -1)
         {
            printf ("%3u", out[i].param);
            if (out[i].comment[0])
               printf (" (%s)", out[i].comment);
         }
         printf ("\n");
      }
   }

   cf = 0;

   printf ("   name '%s'\n", ft.entry[cf].name);

   printf ("   consts\n");
   for (i=0; i<ft.consts.n; i++)
      printf ("      %s\n", ft.consts.s[i]);

   printf ("   names (");
   if (ft.entry[cf].global.n)
   {
      for (i=0; i<ft.entry[cf].global.n-1; i++)
         printf ("'%s', ", ft.entry[cf].global.s[i]);
      printf ("'%s'", ft.entry[cf].global.s[i]);
   }
   printf (")\n");
}

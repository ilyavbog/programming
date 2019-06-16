#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof ((a)) / sizeof ((a)[0]))
#endif

typedef enum bool_ {
   false,
   true
} bool;

typedef enum TYPE_ {
   NAME,
   NUMBER,
   STRING,
   OP,
   NEWLINE,     // = C ';'
   INDENT,      // = C '{'
   NL,          // ignore
   DEDENT,      // = C '}'
   ENDMARKER,
   RESULT,
   FUNCTION,
   KWORD
} TYPE;

typedef char STR[50];

typedef enum  OPERATOR_ {
   LPAR = 0,               // (
   RPAR = 1,               // )

   DOUBLESTAR = 10,        // **

   TILDE = 20,             // ~

   PERCENT = 30,           // %
   STAR,                   // *
   SLASH,                  // /
   DOUBLESLASH,            // //

   PLUS = 40,              // +
   MINUS,                  // -

   LEFTSHIFT = 50,         // <<
   RIGHTSHIFT,             // >>

   AMPER = 60,             // &
   CIRCUMFLEX,             // ^
   VBAR,                   // |

   LESS = 70,              // <
   LESSEQUAL,              // <=
   GREATER,                // >
   GREATEREQUAL,           // >=

   NOTEQUAL = 80,          // !=
   EQEQUAL,                // ==

   PERCENTEQUAL = 90,      // %=
   AMPEREQUAL,             // &=
   DOUBLESTAREQUAL,        // **=
   STAREQUAL,              // *=
   PLUSEQUAL,              // +=
   MINEQUAL,               // -=
   DOUBLESLASHEQUAL,       // //=
   SLASHEQUAL,             // /=
   EQUAL,                  // =

   COMMA,             // ,
   RARROW,            // ->
   DOT,               // .
   ELLIPSIS,          // ...
   COLON,             // :
   COLONEQUAL,        // :=
   SEMI,              // ;
   LEFTSHIFTEQUAL,    // <<=
   RIGHTSHIFTEQUAL,   // >>=
   AT,                // @
   ATEQUAL,           // @=
   LSQB,              // [
   RSQB,              // ]
   CIRCUMFLEXEQUAL,   // ^=
   LBRACE,            // {
   VBAREQUAL,         // |=
   RBRACE,            // }

   NONE = 200
} OPERATOR;

typedef enum KEYWORD_ {
   FOR,
   IN,
   USUALLY
} KEYWORD;

typedef struct LEX_
{
   TYPE     type;
   STR      string;
   unsigned line;
   OPERATOR op;
} LEX;

typedef struct STR_LIST_
{
   char     **s;
   unsigned n;
} STR_LIST;

#define L_PART 0x01
#define R_PART 0x02
#define F_FLAG 0x04
#define F_FOR  0x08
typedef struct LEX_EX_
{
   LEX      *l;
   int      flags;
   unsigned priority;
} LEX_EX;

typedef struct LEX_STACK_
{
   LEX_EX   *slot;
   unsigned n;
} LEX_STACK;

typedef struct ARG_COUNT_STACK_
{
   unsigned *slot;
   unsigned n;
} ARG_COUNT_STACK;

typedef enum PC_TYPE_
{
   PC_SETUP_LOOP,
   PC_FOR_ITER,
   PC_END_OF_LOOP  //???
} PC_TYPE;

typedef struct PC_SLOT_
{
   unsigned pc;
   PC_TYPE  type;
} PC_SLOT;

typedef struct PC_STACK_
{
   PC_SLOT  *slot;
   unsigned n;
} PC_STACK;

typedef struct INDENT_STACK_
{
   KEYWORD  *slot;
   unsigned n;
} INDENT_STACK;

/* Top of stack */
#define Tos(s)   s.slot[s.n-1]

#define Add2List(list,str) \
do {                             \
   strcpy (list.s[list.n], str); \
   list.n++;                     \
} while (0)

#define Push(stack_, slot_) \
do {                              \
   stack_.slot[stack_.n] = slot_; \
   stack_.n++;                    \
} while (0)

#define Pop(stack_) \
   (stack_.n--, stack_.slot[stack_.n])

/*-------------------------------------------*/

typedef struct BYTECODE_ {
   int  line; /* source line number or -1 */
   int  pc;
   char *comm;
   int  param;
   char comment[32];
   bool label;
} BYTECODE;

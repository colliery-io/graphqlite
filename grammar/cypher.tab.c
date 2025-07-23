/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "cypher.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ast.h"

// Forward declarations
int yylex(void);
void yyerror(const char *s);

// Global result storage for parsed AST
cypher_ast_node_t *parse_result = NULL;

#line 85 "cypher.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "cypher.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_IDENTIFIER = 3,                 /* IDENTIFIER  */
  YYSYMBOL_STRING_LITERAL = 4,             /* STRING_LITERAL  */
  YYSYMBOL_INTEGER_LITERAL = 5,            /* INTEGER_LITERAL  */
  YYSYMBOL_FLOAT_LITERAL = 6,              /* FLOAT_LITERAL  */
  YYSYMBOL_CREATE = 7,                     /* CREATE  */
  YYSYMBOL_MATCH = 8,                      /* MATCH  */
  YYSYMBOL_RETURN = 9,                     /* RETURN  */
  YYSYMBOL_WHERE = 10,                     /* WHERE  */
  YYSYMBOL_TRUE = 11,                      /* TRUE  */
  YYSYMBOL_FALSE = 12,                     /* FALSE  */
  YYSYMBOL_AND = 13,                       /* AND  */
  YYSYMBOL_OR = 14,                        /* OR  */
  YYSYMBOL_NOT = 15,                       /* NOT  */
  YYSYMBOL_IS = 16,                        /* IS  */
  YYSYMBOL_NULL_TOKEN = 17,                /* NULL_TOKEN  */
  YYSYMBOL_LPAREN = 18,                    /* LPAREN  */
  YYSYMBOL_RPAREN = 19,                    /* RPAREN  */
  YYSYMBOL_LBRACE = 20,                    /* LBRACE  */
  YYSYMBOL_RBRACE = 21,                    /* RBRACE  */
  YYSYMBOL_LBRACKET = 22,                  /* LBRACKET  */
  YYSYMBOL_RBRACKET = 23,                  /* RBRACKET  */
  YYSYMBOL_DOT = 24,                       /* DOT  */
  YYSYMBOL_COMMA = 25,                     /* COMMA  */
  YYSYMBOL_COLON = 26,                     /* COLON  */
  YYSYMBOL_SEMICOLON = 27,                 /* SEMICOLON  */
  YYSYMBOL_ARROW_RIGHT = 28,               /* ARROW_RIGHT  */
  YYSYMBOL_ARROW_LEFT = 29,                /* ARROW_LEFT  */
  YYSYMBOL_DASH = 30,                      /* DASH  */
  YYSYMBOL_EQ = 31,                        /* EQ  */
  YYSYMBOL_NEQ = 32,                       /* NEQ  */
  YYSYMBOL_LT = 33,                        /* LT  */
  YYSYMBOL_GT = 34,                        /* GT  */
  YYSYMBOL_LE = 35,                        /* LE  */
  YYSYMBOL_GE = 36,                        /* GE  */
  YYSYMBOL_YYACCEPT = 37,                  /* $accept  */
  YYSYMBOL_program = 38,                   /* program  */
  YYSYMBOL_statement = 39,                 /* statement  */
  YYSYMBOL_create_statement = 40,          /* create_statement  */
  YYSYMBOL_match_statement = 41,           /* match_statement  */
  YYSYMBOL_return_statement = 42,          /* return_statement  */
  YYSYMBOL_node_pattern = 43,              /* node_pattern  */
  YYSYMBOL_relationship_pattern = 44,      /* relationship_pattern  */
  YYSYMBOL_property_list = 45,             /* property_list  */
  YYSYMBOL_variable = 46,                  /* variable  */
  YYSYMBOL_label = 47,                     /* label  */
  YYSYMBOL_property = 48,                  /* property  */
  YYSYMBOL_literal = 49,                   /* literal  */
  YYSYMBOL_string_literal = 50,            /* string_literal  */
  YYSYMBOL_integer_literal = 51,           /* integer_literal  */
  YYSYMBOL_float_literal = 52,             /* float_literal  */
  YYSYMBOL_boolean_literal = 53,           /* boolean_literal  */
  YYSYMBOL_where_clause = 54,              /* where_clause  */
  YYSYMBOL_expression = 55,                /* expression  */
  YYSYMBOL_comparison_expression = 56,     /* comparison_expression  */
  YYSYMBOL_property_expression = 57        /* property_expression  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  12
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   161

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  37
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  21
/* YYNRULES -- Number of rules.  */
#define YYNRULES  62
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  160

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   291


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    54,    54,    58,    59,    63,    67,    76,    79,    86,
      89,    96,   103,   106,   109,   112,   119,   123,   127,   131,
     135,   139,   143,   147,   151,   155,   159,   163,   167,   171,
     175,   179,   200,   204,   211,   219,   227,   235,   236,   237,
     238,   243,   250,   257,   264,   267,   274,   281,   282,   285,
     288,   291,   298,   301,   304,   307,   310,   313,   316,   319,
     326,   330,   335
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  static const char *const yy_sname[] =
  {
  "end of file", "error", "invalid token", "IDENTIFIER", "STRING_LITERAL",
  "INTEGER_LITERAL", "FLOAT_LITERAL", "CREATE", "MATCH", "RETURN", "WHERE",
  "TRUE", "FALSE", "AND", "OR", "NOT", "IS", "NULL_TOKEN", "LPAREN",
  "RPAREN", "LBRACE", "RBRACE", "LBRACKET", "RBRACKET", "DOT", "COMMA",
  "COLON", "SEMICOLON", "ARROW_RIGHT", "ARROW_LEFT", "DASH", "EQ", "NEQ",
  "LT", "GT", "LE", "GE", "$accept", "program", "statement",
  "create_statement", "match_statement", "return_statement",
  "node_pattern", "relationship_pattern", "property_list", "variable",
  "label", "property", "literal", "string_literal", "integer_literal",
  "float_literal", "boolean_literal", "where_clause", "expression",
  "comparison_expression", "property_expression", YY_NULLPTR
  };
  return yy_sname[yysymbol];
}
#endif

#define YYPACT_NINF (-88)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      67,   -10,   -10,    29,   -88,     8,    81,    33,    94,   -88,
      94,   -88,   -88,    33,   -88,    51,   -88,     8,   -88,    39,
      16,    19,   -88,    36,   -88,   -88,   -88,   -88,   -88,    51,
      51,   -88,   -88,   -88,   -88,   -88,   112,   -88,    17,   -88,
     -88,    99,   105,     0,   -10,     2,   -10,   116,   112,    54,
      51,    51,    28,     9,     9,     9,     9,     9,     9,    55,
      63,   -88,   -88,   108,    91,   105,    56,   -88,   101,   105,
      60,   -88,   -88,   -88,   112,   112,   113,   -88,   -88,   -88,
     -88,   -88,   -88,   -88,    66,   114,    99,   -88,    99,   -10,
      -1,    99,   104,   105,   -10,    41,    99,   103,   105,   -88,
     -88,   -88,   -88,    64,   -88,    99,   106,    78,   -10,    95,
     -88,    99,   107,    79,   -10,    97,   119,    80,   -10,   117,
     -88,    99,   111,    86,   -10,   121,   -88,    99,   120,   -88,
     122,   -88,   124,    88,   -10,   127,   -88,   123,    89,   -10,
     126,   -10,   129,   -88,   130,   -10,   134,   -88,   -10,   -88,
     131,   -10,   -88,   132,   -88,   -10,   -88,   -10,   -88,   -88
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     0,     0,     0,     2,     3,     0,     0,     7,     8,
       9,    10,     1,     0,     4,     0,     5,     0,    34,     0,
       0,     0,    11,    60,    41,    42,    43,    44,    45,     0,
       0,    62,    37,    38,    39,    40,    46,    47,     0,     6,
      14,     0,     0,     0,     0,     0,     0,     0,    50,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    32,    35,     0,     0,     0,     0,    31,     0,     0,
       0,    23,    61,    51,    48,    49,     0,    58,    52,    53,
      54,    55,    56,    57,     0,     0,     0,    12,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    59,
      36,    15,    33,     0,    30,     0,     0,     0,     0,     0,
      22,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      28,     0,     0,     0,     0,     0,    20,     0,     0,    13,
       0,    24,     0,     0,     0,     0,    16,     0,     0,     0,
       0,     0,     0,    26,     0,     0,     0,    18,     0,    29,
       0,     0,    21,     0,    25,     0,    17,     0,    27,    19
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -88,   -88,   -88,   -88,   -88,    10,    -2,   140,   -87,    -6,
     -63,    61,    75,   -88,   -88,   -88,   -88,   -88,   -19,   -88,
      40
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,     3,     4,     5,     6,    14,     8,     9,    60,    19,
      63,    61,    31,    32,    33,    34,    35,    17,    36,    37,
      38
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      10,   103,    90,    18,   107,    18,    95,    22,     7,   113,
      48,    49,    23,    24,    25,    26,    16,    13,   117,   105,
      27,    28,   106,    64,   123,    68,    65,    39,    69,    12,
     109,    74,    75,    52,   133,   115,    18,    66,    43,    70,
     138,    45,    67,    76,    71,    77,    44,    46,    53,    54,
      55,    56,    57,    58,    23,    24,    25,    26,    40,    41,
      47,   111,    27,    28,   112,    42,    29,    50,    51,    30,
      24,    25,    26,    73,     1,     2,    91,    27,    28,    92,
      96,    84,    93,    97,    85,   116,    98,   104,    86,    86,
      13,    15,   110,    78,    79,    80,    81,    82,    83,   119,
     125,   130,    59,    86,    86,    86,   120,   135,    62,   142,
     146,    86,   126,    86,    86,   121,   131,   127,   122,    72,
     128,    89,   136,    20,    21,    50,    51,    87,    88,    94,
      99,   114,   143,   101,   108,   124,   118,   147,   129,   149,
     132,   134,    11,   152,   137,   140,   154,   102,   139,   156,
     144,   145,   150,   158,   141,   159,   148,   153,   151,   100,
     157,   155
};

static const yytype_uint8 yycheck[] =
{
       2,    88,    65,     3,    91,     3,    69,    13,    18,    96,
      29,    30,     3,     4,     5,     6,     6,     9,   105,    20,
      11,    12,    23,    23,   111,    23,    26,    17,    26,     0,
      93,    50,    51,    16,   121,    98,     3,    43,    22,    45,
     127,    22,    44,    15,    46,    17,    30,    28,    31,    32,
      33,    34,    35,    36,     3,     4,     5,     6,    19,    20,
      24,    20,    11,    12,    23,    26,    15,    13,    14,    18,
       4,     5,     6,    19,     7,     8,    20,    11,    12,    23,
      20,    26,    26,    23,    21,    21,    26,    89,    25,    25,
       9,    10,    94,    53,    54,    55,    56,    57,    58,    21,
      21,    21,     3,    25,    25,    25,   108,    21,     3,    21,
      21,    25,   114,    25,    25,    20,   118,    20,    23,     3,
      23,    30,   124,    29,    30,    13,    14,    19,    20,    28,
      17,    28,   134,    19,    30,    28,    30,   139,    19,   141,
      23,    30,     2,   145,    23,    23,   148,    86,    28,   151,
      23,    28,    23,   155,    30,   157,    30,    23,    28,    84,
      28,    30
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     7,     8,    38,    39,    40,    41,    18,    43,    44,
      43,    44,     0,     9,    42,    10,    42,    54,     3,    46,
      29,    30,    46,     3,     4,     5,     6,    11,    12,    15,
      18,    49,    50,    51,    52,    53,    55,    56,    57,    42,
      19,    20,    26,    22,    30,    22,    28,    24,    55,    55,
      13,    14,    16,    31,    32,    33,    34,    35,    36,     3,
      45,    48,     3,    47,    23,    26,    46,    43,    23,    26,
      46,    43,     3,    19,    55,    55,    15,    17,    57,    57,
      57,    57,    57,    57,    26,    21,    25,    19,    20,    30,
      47,    20,    23,    26,    28,    47,    20,    23,    26,    17,
      49,    19,    48,    45,    43,    20,    23,    45,    30,    47,
      43,    20,    23,    45,    28,    47,    21,    45,    30,    21,
      43,    20,    23,    45,    28,    21,    43,    20,    23,    19,
      21,    43,    23,    45,    30,    21,    43,    23,    45,    28,
      23,    30,    21,    43,    23,    28,    21,    43,    30,    43,
      23,    28,    43,    23,    43,    30,    43,    28,    43,    43
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    37,    38,    39,    39,    39,    39,    40,    40,    41,
      41,    42,    43,    43,    43,    43,    44,    44,    44,    44,
      44,    44,    44,    44,    44,    44,    44,    44,    44,    44,
      44,    44,    45,    45,    46,    47,    48,    49,    49,    49,
      49,    50,    51,    52,    53,    53,    54,    55,    55,    55,
      55,    55,    56,    56,    56,    56,    56,    56,    56,    56,
      57,    57,    57
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     2,     2,     3,     2,     2,     2,
       2,     2,     5,     8,     3,     6,     8,    11,     9,    12,
       7,    10,     6,     4,     8,    11,     9,    12,     7,    10,
       6,     4,     1,     3,     1,     1,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     1,     3,     3,
       2,     3,     3,     3,     3,     3,     3,     3,     3,     4,
       1,     3,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif



static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yystrlen (yysymbol_name (yyarg[yyi]));
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp = yystpcpy (yyp, yysymbol_name (yyarg[yyi++]));
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* program: statement  */
#line 54 "cypher.y"
              { parse_result = (yyvsp[0].statement); }
#line 1445 "cypher.tab.c"
    break;

  case 3: /* statement: create_statement  */
#line 58 "cypher.y"
                     { (yyval.statement) = (yyvsp[0].create_statement); }
#line 1451 "cypher.tab.c"
    break;

  case 4: /* statement: create_statement return_statement  */
#line 59 "cypher.y"
                                        { 
        // Create a compound statement with CREATE and RETURN
        (yyval.statement) = ast_create_compound_statement((yyvsp[-1].create_statement), (yyvsp[0].return_statement));
    }
#line 1460 "cypher.tab.c"
    break;

  case 5: /* statement: match_statement return_statement  */
#line 63 "cypher.y"
                                       { 
        // Create a compound statement with MATCH and RETURN
        (yyval.statement) = ast_create_compound_statement((yyvsp[-1].match_statement), (yyvsp[0].return_statement));
    }
#line 1469 "cypher.tab.c"
    break;

  case 6: /* statement: match_statement where_clause return_statement  */
#line 67 "cypher.y"
                                                    {
        // Create a compound statement with MATCH, WHERE and RETURN
        cypher_ast_node_t *match_with_where = ast_attach_where_clause((yyvsp[-2].match_statement), (yyvsp[-1].where_clause));
        (yyval.statement) = ast_create_compound_statement(match_with_where, (yyvsp[0].return_statement));
    }
#line 1479 "cypher.tab.c"
    break;

  case 7: /* create_statement: CREATE node_pattern  */
#line 76 "cypher.y"
                        {
        (yyval.create_statement) = ast_create_create_statement((yyvsp[0].node_pattern));
    }
#line 1487 "cypher.tab.c"
    break;

  case 8: /* create_statement: CREATE relationship_pattern  */
#line 79 "cypher.y"
                                  {
        (yyval.create_statement) = ast_create_create_statement((yyvsp[0].relationship_pattern));
    }
#line 1495 "cypher.tab.c"
    break;

  case 9: /* match_statement: MATCH node_pattern  */
#line 86 "cypher.y"
                       {
        (yyval.match_statement) = ast_create_match_statement((yyvsp[0].node_pattern));
    }
#line 1503 "cypher.tab.c"
    break;

  case 10: /* match_statement: MATCH relationship_pattern  */
#line 89 "cypher.y"
                                 {
        (yyval.match_statement) = ast_create_match_statement((yyvsp[0].relationship_pattern));
    }
#line 1511 "cypher.tab.c"
    break;

  case 11: /* return_statement: RETURN variable  */
#line 96 "cypher.y"
                    {
        (yyval.return_statement) = ast_create_return_statement((yyvsp[0].variable));
    }
#line 1519 "cypher.tab.c"
    break;

  case 12: /* node_pattern: LPAREN variable COLON label RPAREN  */
#line 103 "cypher.y"
                                       {
        (yyval.node_pattern) = ast_create_node_pattern((yyvsp[-3].variable), (yyvsp[-1].label), NULL);
    }
#line 1527 "cypher.tab.c"
    break;

  case 13: /* node_pattern: LPAREN variable COLON label LBRACE property_list RBRACE RPAREN  */
#line 106 "cypher.y"
                                                                     {
        (yyval.node_pattern) = ast_create_node_pattern((yyvsp[-6].variable), (yyvsp[-4].label), (yyvsp[-2].property_list));
    }
#line 1535 "cypher.tab.c"
    break;

  case 14: /* node_pattern: LPAREN variable RPAREN  */
#line 109 "cypher.y"
                             {
        (yyval.node_pattern) = ast_create_node_pattern((yyvsp[-1].variable), NULL, NULL);
    }
#line 1543 "cypher.tab.c"
    break;

  case 15: /* node_pattern: LPAREN variable LBRACE property_list RBRACE RPAREN  */
#line 112 "cypher.y"
                                                         {
        (yyval.node_pattern) = ast_create_node_pattern((yyvsp[-4].variable), NULL, (yyvsp[-2].property_list));
    }
#line 1551 "cypher.tab.c"
    break;

  case 16: /* relationship_pattern: node_pattern DASH LBRACKET COLON label RBRACKET ARROW_RIGHT node_pattern  */
#line 119 "cypher.y"
                                                                             {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, (yyvsp[-3].label), NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[-7].node_pattern), edge, (yyvsp[0].node_pattern), 1);  // 1 = right direction
    }
#line 1560 "cypher.tab.c"
    break;

  case 17: /* relationship_pattern: node_pattern DASH LBRACKET COLON label LBRACE property_list RBRACE RBRACKET ARROW_RIGHT node_pattern  */
#line 123 "cypher.y"
                                                                                                           {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, (yyvsp[-6].label), (yyvsp[-4].property_list));
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[-10].node_pattern), edge, (yyvsp[0].node_pattern), 1);  // 1 = right direction, type with properties
    }
#line 1569 "cypher.tab.c"
    break;

  case 18: /* relationship_pattern: node_pattern DASH LBRACKET variable COLON label RBRACKET ARROW_RIGHT node_pattern  */
#line 127 "cypher.y"
                                                                                        {
        cypher_ast_node_t *edge = ast_create_edge_pattern((yyvsp[-5].variable), (yyvsp[-3].label), NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[-8].node_pattern), edge, (yyvsp[0].node_pattern), 1);  // 1 = right direction
    }
#line 1578 "cypher.tab.c"
    break;

  case 19: /* relationship_pattern: node_pattern DASH LBRACKET variable COLON label LBRACE property_list RBRACE RBRACKET ARROW_RIGHT node_pattern  */
#line 131 "cypher.y"
                                                                                                                    {
        cypher_ast_node_t *edge = ast_create_edge_pattern((yyvsp[-8].variable), (yyvsp[-6].label), (yyvsp[-4].property_list));
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[-11].node_pattern), edge, (yyvsp[0].node_pattern), 1);  // 1 = right direction
    }
#line 1587 "cypher.tab.c"
    break;

  case 20: /* relationship_pattern: node_pattern DASH LBRACKET variable RBRACKET ARROW_RIGHT node_pattern  */
#line 135 "cypher.y"
                                                                            {
        cypher_ast_node_t *edge = ast_create_edge_pattern((yyvsp[-3].variable), NULL, NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[-6].node_pattern), edge, (yyvsp[0].node_pattern), 1);  // 1 = right direction, no type
    }
#line 1596 "cypher.tab.c"
    break;

  case 21: /* relationship_pattern: node_pattern DASH LBRACKET variable LBRACE property_list RBRACE RBRACKET ARROW_RIGHT node_pattern  */
#line 139 "cypher.y"
                                                                                                        {
        cypher_ast_node_t *edge = ast_create_edge_pattern((yyvsp[-6].variable), NULL, (yyvsp[-4].property_list));
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[-9].node_pattern), edge, (yyvsp[0].node_pattern), 1);  // 1 = right direction, no type but with properties
    }
#line 1605 "cypher.tab.c"
    break;

  case 22: /* relationship_pattern: node_pattern DASH LBRACKET RBRACKET ARROW_RIGHT node_pattern  */
#line 143 "cypher.y"
                                                                   {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, NULL, NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[-5].node_pattern), edge, (yyvsp[0].node_pattern), 1);  // 1 = right direction, no variable or type
    }
#line 1614 "cypher.tab.c"
    break;

  case 23: /* relationship_pattern: node_pattern DASH ARROW_RIGHT node_pattern  */
#line 147 "cypher.y"
                                                 {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, NULL, NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[-3].node_pattern), edge, (yyvsp[0].node_pattern), 1);  // 1 = right direction, no brackets
    }
#line 1623 "cypher.tab.c"
    break;

  case 24: /* relationship_pattern: node_pattern ARROW_LEFT LBRACKET COLON label RBRACKET DASH node_pattern  */
#line 151 "cypher.y"
                                                                              {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, (yyvsp[-3].label), NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[0].node_pattern), edge, (yyvsp[-7].node_pattern), -1);  // -1 = left direction
    }
#line 1632 "cypher.tab.c"
    break;

  case 25: /* relationship_pattern: node_pattern ARROW_LEFT LBRACKET COLON label LBRACE property_list RBRACE RBRACKET DASH node_pattern  */
#line 155 "cypher.y"
                                                                                                          {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, (yyvsp[-6].label), (yyvsp[-4].property_list));
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[0].node_pattern), edge, (yyvsp[-10].node_pattern), -1);  // -1 = left direction, type with properties
    }
#line 1641 "cypher.tab.c"
    break;

  case 26: /* relationship_pattern: node_pattern ARROW_LEFT LBRACKET variable COLON label RBRACKET DASH node_pattern  */
#line 159 "cypher.y"
                                                                                       {
        cypher_ast_node_t *edge = ast_create_edge_pattern((yyvsp[-5].variable), (yyvsp[-3].label), NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[0].node_pattern), edge, (yyvsp[-8].node_pattern), -1);  // -1 = left direction  
    }
#line 1650 "cypher.tab.c"
    break;

  case 27: /* relationship_pattern: node_pattern ARROW_LEFT LBRACKET variable COLON label LBRACE property_list RBRACE RBRACKET DASH node_pattern  */
#line 163 "cypher.y"
                                                                                                                   {
        cypher_ast_node_t *edge = ast_create_edge_pattern((yyvsp[-8].variable), (yyvsp[-6].label), (yyvsp[-4].property_list));
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[0].node_pattern), edge, (yyvsp[-11].node_pattern), -1);  // -1 = left direction
    }
#line 1659 "cypher.tab.c"
    break;

  case 28: /* relationship_pattern: node_pattern ARROW_LEFT LBRACKET variable RBRACKET DASH node_pattern  */
#line 167 "cypher.y"
                                                                           {
        cypher_ast_node_t *edge = ast_create_edge_pattern((yyvsp[-3].variable), NULL, NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[0].node_pattern), edge, (yyvsp[-6].node_pattern), -1);  // -1 = left direction, no type
    }
#line 1668 "cypher.tab.c"
    break;

  case 29: /* relationship_pattern: node_pattern ARROW_LEFT LBRACKET variable LBRACE property_list RBRACE RBRACKET DASH node_pattern  */
#line 171 "cypher.y"
                                                                                                       {
        cypher_ast_node_t *edge = ast_create_edge_pattern((yyvsp[-6].variable), NULL, (yyvsp[-4].property_list));
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[0].node_pattern), edge, (yyvsp[-9].node_pattern), -1);  // -1 = left direction, no type but with properties
    }
#line 1677 "cypher.tab.c"
    break;

  case 30: /* relationship_pattern: node_pattern ARROW_LEFT LBRACKET RBRACKET DASH node_pattern  */
#line 175 "cypher.y"
                                                                  {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, NULL, NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[0].node_pattern), edge, (yyvsp[-5].node_pattern), -1);  // -1 = left direction, no variable or type
    }
#line 1686 "cypher.tab.c"
    break;

  case 31: /* relationship_pattern: node_pattern ARROW_LEFT DASH node_pattern  */
#line 179 "cypher.y"
                                                {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, NULL, NULL);
        (yyval.relationship_pattern) = ast_create_relationship_pattern((yyvsp[0].node_pattern), edge, (yyvsp[-3].node_pattern), -1);  // -1 = left direction, no brackets
    }
#line 1695 "cypher.tab.c"
    break;

  case 32: /* property_list: property  */
#line 200 "cypher.y"
             {
        cypher_ast_node_t *list = ast_create_property_list();
        (yyval.property_list) = ast_add_property_to_list(list, (yyvsp[0].property));
    }
#line 1704 "cypher.tab.c"
    break;

  case 33: /* property_list: property_list COMMA property  */
#line 204 "cypher.y"
                                   {
        (yyval.property_list) = ast_add_property_to_list((yyvsp[-2].property_list), (yyvsp[0].property));
    }
#line 1712 "cypher.tab.c"
    break;

  case 34: /* variable: IDENTIFIER  */
#line 211 "cypher.y"
               {
        (yyval.variable) = ast_create_variable((yyvsp[0].IDENTIFIER));
        free((yyvsp[0].IDENTIFIER));  // Free the token string
    }
#line 1721 "cypher.tab.c"
    break;

  case 35: /* label: IDENTIFIER  */
#line 219 "cypher.y"
               {
        (yyval.label) = ast_create_label((yyvsp[0].IDENTIFIER));
        free((yyvsp[0].IDENTIFIER));  // Free the token string
    }
#line 1730 "cypher.tab.c"
    break;

  case 36: /* property: IDENTIFIER COLON literal  */
#line 227 "cypher.y"
                             {
        (yyval.property) = ast_create_property((yyvsp[-2].IDENTIFIER), (yyvsp[0].literal));
        free((yyvsp[-2].IDENTIFIER));  // Free the key string
    }
#line 1739 "cypher.tab.c"
    break;

  case 37: /* literal: string_literal  */
#line 235 "cypher.y"
                   { (yyval.literal) = (yyvsp[0].string_literal); }
#line 1745 "cypher.tab.c"
    break;

  case 38: /* literal: integer_literal  */
#line 236 "cypher.y"
                      { (yyval.literal) = (yyvsp[0].integer_literal); }
#line 1751 "cypher.tab.c"
    break;

  case 39: /* literal: float_literal  */
#line 237 "cypher.y"
                    { (yyval.literal) = (yyvsp[0].float_literal); }
#line 1757 "cypher.tab.c"
    break;

  case 40: /* literal: boolean_literal  */
#line 238 "cypher.y"
                      { (yyval.literal) = (yyvsp[0].boolean_literal); }
#line 1763 "cypher.tab.c"
    break;

  case 41: /* string_literal: STRING_LITERAL  */
#line 243 "cypher.y"
                   {
        (yyval.string_literal) = ast_create_string_literal((yyvsp[0].STRING_LITERAL));
        free((yyvsp[0].STRING_LITERAL));  // Free the token string
    }
#line 1772 "cypher.tab.c"
    break;

  case 42: /* integer_literal: INTEGER_LITERAL  */
#line 250 "cypher.y"
                    {
        (yyval.integer_literal) = ast_create_integer_literal((yyvsp[0].INTEGER_LITERAL));
        free((yyvsp[0].INTEGER_LITERAL));  // Free the token string
    }
#line 1781 "cypher.tab.c"
    break;

  case 43: /* float_literal: FLOAT_LITERAL  */
#line 257 "cypher.y"
                  {
        (yyval.float_literal) = ast_create_float_literal((yyvsp[0].FLOAT_LITERAL));
        free((yyvsp[0].FLOAT_LITERAL));  // Free the token string
    }
#line 1790 "cypher.tab.c"
    break;

  case 44: /* boolean_literal: TRUE  */
#line 264 "cypher.y"
         {
        (yyval.boolean_literal) = ast_create_boolean_literal(1);
    }
#line 1798 "cypher.tab.c"
    break;

  case 45: /* boolean_literal: FALSE  */
#line 267 "cypher.y"
            {
        (yyval.boolean_literal) = ast_create_boolean_literal(0);
    }
#line 1806 "cypher.tab.c"
    break;

  case 46: /* where_clause: WHERE expression  */
#line 274 "cypher.y"
                     {
        (yyval.where_clause) = ast_create_where_clause((yyvsp[0].expression));
    }
#line 1814 "cypher.tab.c"
    break;

  case 47: /* expression: comparison_expression  */
#line 281 "cypher.y"
                          { (yyval.expression) = (yyvsp[0].comparison_expression); }
#line 1820 "cypher.tab.c"
    break;

  case 48: /* expression: expression AND expression  */
#line 282 "cypher.y"
                                {
        (yyval.expression) = ast_create_binary_expr((yyvsp[-2].expression), AST_OP_AND, (yyvsp[0].expression));
    }
#line 1828 "cypher.tab.c"
    break;

  case 49: /* expression: expression OR expression  */
#line 285 "cypher.y"
                               {
        (yyval.expression) = ast_create_binary_expr((yyvsp[-2].expression), AST_OP_OR, (yyvsp[0].expression));
    }
#line 1836 "cypher.tab.c"
    break;

  case 50: /* expression: NOT expression  */
#line 288 "cypher.y"
                     {
        (yyval.expression) = ast_create_unary_expr(AST_OP_NOT, (yyvsp[0].expression));
    }
#line 1844 "cypher.tab.c"
    break;

  case 51: /* expression: LPAREN expression RPAREN  */
#line 291 "cypher.y"
                               {
        (yyval.expression) = (yyvsp[-1].expression);  // Parentheses just return the inner expression
    }
#line 1852 "cypher.tab.c"
    break;

  case 52: /* comparison_expression: property_expression EQ property_expression  */
#line 298 "cypher.y"
                                               {
        (yyval.comparison_expression) = ast_create_binary_expr((yyvsp[-2].property_expression), AST_OP_EQ, (yyvsp[0].property_expression));
    }
#line 1860 "cypher.tab.c"
    break;

  case 53: /* comparison_expression: property_expression NEQ property_expression  */
#line 301 "cypher.y"
                                                  {
        (yyval.comparison_expression) = ast_create_binary_expr((yyvsp[-2].property_expression), AST_OP_NEQ, (yyvsp[0].property_expression));
    }
#line 1868 "cypher.tab.c"
    break;

  case 54: /* comparison_expression: property_expression LT property_expression  */
#line 304 "cypher.y"
                                                 {
        (yyval.comparison_expression) = ast_create_binary_expr((yyvsp[-2].property_expression), AST_OP_LT, (yyvsp[0].property_expression));
    }
#line 1876 "cypher.tab.c"
    break;

  case 55: /* comparison_expression: property_expression GT property_expression  */
#line 307 "cypher.y"
                                                 {
        (yyval.comparison_expression) = ast_create_binary_expr((yyvsp[-2].property_expression), AST_OP_GT, (yyvsp[0].property_expression));
    }
#line 1884 "cypher.tab.c"
    break;

  case 56: /* comparison_expression: property_expression LE property_expression  */
#line 310 "cypher.y"
                                                 {
        (yyval.comparison_expression) = ast_create_binary_expr((yyvsp[-2].property_expression), AST_OP_LE, (yyvsp[0].property_expression));
    }
#line 1892 "cypher.tab.c"
    break;

  case 57: /* comparison_expression: property_expression GE property_expression  */
#line 313 "cypher.y"
                                                 {
        (yyval.comparison_expression) = ast_create_binary_expr((yyvsp[-2].property_expression), AST_OP_GE, (yyvsp[0].property_expression));
    }
#line 1900 "cypher.tab.c"
    break;

  case 58: /* comparison_expression: property_expression IS NULL_TOKEN  */
#line 316 "cypher.y"
                                        {
        (yyval.comparison_expression) = ast_create_is_null_expr((yyvsp[-2].property_expression), 1);  // IS NULL
    }
#line 1908 "cypher.tab.c"
    break;

  case 59: /* comparison_expression: property_expression IS NOT NULL_TOKEN  */
#line 319 "cypher.y"
                                            {
        (yyval.comparison_expression) = ast_create_is_null_expr((yyvsp[-3].property_expression), 0);  // IS NOT NULL
    }
#line 1916 "cypher.tab.c"
    break;

  case 60: /* property_expression: IDENTIFIER  */
#line 326 "cypher.y"
               {
        (yyval.property_expression) = ast_create_identifier((yyvsp[0].IDENTIFIER));
        free((yyvsp[0].IDENTIFIER));
    }
#line 1925 "cypher.tab.c"
    break;

  case 61: /* property_expression: IDENTIFIER DOT IDENTIFIER  */
#line 330 "cypher.y"
                                {
        (yyval.property_expression) = ast_create_property_access((yyvsp[-2].IDENTIFIER), (yyvsp[0].IDENTIFIER));
        free((yyvsp[-2].IDENTIFIER));
        free((yyvsp[0].IDENTIFIER));
    }
#line 1935 "cypher.tab.c"
    break;

  case 62: /* property_expression: literal  */
#line 335 "cypher.y"
              { (yyval.property_expression) = (yyvsp[0].literal); }
#line 1941 "cypher.tab.c"
    break;


#line 1945 "cypher.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 338 "cypher.y"


void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}

// Forward declarations for lexer functions
extern void init_lexer(const char *input);
extern void cleanup_lexer(void);

// Main parse function
cypher_ast_node_t* parse_cypher_query(const char *query) {
    parse_result = NULL;
    
    // Initialize lexer with input string
    init_lexer(query);
    
    // Parse the input
    int result = yyparse();
    
    // Clean up lexer
    cleanup_lexer();
    
    if (result != 0) {
        // Parse failed - clean up any partial result
        if (parse_result) {
            ast_free(parse_result);
            parse_result = NULL;
        }
        return NULL;
    }
    
    return parse_result;
}

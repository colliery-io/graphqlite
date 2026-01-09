/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Skeleton implementation for Bison GLR parsers in C

   Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

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

/* C GLR parser skeleton written by Paul Hilfinger.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "glr.c"

/* Pure parsers.  */
#define YYPURE 1



/* Substitute the type names.  */
#define YYSTYPE CYPHER_YYSTYPE
#define YYLTYPE CYPHER_YYLTYPE
/* Substitute the variable and function names.  */
#define yyparse cypher_yyparse
#define yylex   cypher_yylex
#define yyerror cypher_yyerror
#define yydebug cypher_yydebug

/* First part of user prologue.  */
#line 1 "src/backend/parser/cypher_gram.y"

/*
 * Cypher Grammar for GraphQLite
 * Simplified version based on Apache AGE grammar for SQLite compatibility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser/cypher_ast.h"
#include "parser/cypher_parser.h"

/* Forward declarations */
void cypher_yyerror(CYPHER_YYLTYPE *yylloc, cypher_parser_context *context, const char *msg);
int cypher_yylex(CYPHER_YYSTYPE *yylval, CYPHER_YYLTYPE *yylloc, cypher_parser_context *context);


#line 83 "build/parser/cypher_gram.tab.c"

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

#include "cypher_gram.tab.h"

/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_INTEGER = 3,                    /* INTEGER  */
  YYSYMBOL_DECIMAL = 4,                    /* DECIMAL  */
  YYSYMBOL_STRING = 5,                     /* STRING  */
  YYSYMBOL_IDENTIFIER = 6,                 /* IDENTIFIER  */
  YYSYMBOL_PARAMETER = 7,                  /* PARAMETER  */
  YYSYMBOL_BQIDENT = 8,                    /* BQIDENT  */
  YYSYMBOL_NOT_EQ = 9,                     /* NOT_EQ  */
  YYSYMBOL_LT_EQ = 10,                     /* LT_EQ  */
  YYSYMBOL_GT_EQ = 11,                     /* GT_EQ  */
  YYSYMBOL_DOT_DOT = 12,                   /* DOT_DOT  */
  YYSYMBOL_TYPECAST = 13,                  /* TYPECAST  */
  YYSYMBOL_PLUS_EQ = 14,                   /* PLUS_EQ  */
  YYSYMBOL_REGEX_MATCH = 15,               /* REGEX_MATCH  */
  YYSYMBOL_MATCH = 16,                     /* MATCH  */
  YYSYMBOL_RETURN = 17,                    /* RETURN  */
  YYSYMBOL_CREATE = 18,                    /* CREATE  */
  YYSYMBOL_WHERE = 19,                     /* WHERE  */
  YYSYMBOL_WITH = 20,                      /* WITH  */
  YYSYMBOL_SET = 21,                       /* SET  */
  YYSYMBOL_DELETE = 22,                    /* DELETE  */
  YYSYMBOL_REMOVE = 23,                    /* REMOVE  */
  YYSYMBOL_MERGE = 24,                     /* MERGE  */
  YYSYMBOL_UNWIND = 25,                    /* UNWIND  */
  YYSYMBOL_DETACH = 26,                    /* DETACH  */
  YYSYMBOL_FOREACH = 27,                   /* FOREACH  */
  YYSYMBOL_OPTIONAL = 28,                  /* OPTIONAL  */
  YYSYMBOL_DISTINCT = 29,                  /* DISTINCT  */
  YYSYMBOL_ORDER = 30,                     /* ORDER  */
  YYSYMBOL_BY = 31,                        /* BY  */
  YYSYMBOL_SKIP = 32,                      /* SKIP  */
  YYSYMBOL_LIMIT = 33,                     /* LIMIT  */
  YYSYMBOL_AS = 34,                        /* AS  */
  YYSYMBOL_ASC = 35,                       /* ASC  */
  YYSYMBOL_DESC = 36,                      /* DESC  */
  YYSYMBOL_AND = 37,                       /* AND  */
  YYSYMBOL_OR = 38,                        /* OR  */
  YYSYMBOL_XOR = 39,                       /* XOR  */
  YYSYMBOL_NOT = 40,                       /* NOT  */
  YYSYMBOL_IN = 41,                        /* IN  */
  YYSYMBOL_IS = 42,                        /* IS  */
  YYSYMBOL_NULL_P = 43,                    /* NULL_P  */
  YYSYMBOL_TRUE_P = 44,                    /* TRUE_P  */
  YYSYMBOL_FALSE_P = 45,                   /* FALSE_P  */
  YYSYMBOL_EXISTS = 46,                    /* EXISTS  */
  YYSYMBOL_ANY = 47,                       /* ANY  */
  YYSYMBOL_NONE = 48,                      /* NONE  */
  YYSYMBOL_SINGLE = 49,                    /* SINGLE  */
  YYSYMBOL_REDUCE = 50,                    /* REDUCE  */
  YYSYMBOL_UNION = 51,                     /* UNION  */
  YYSYMBOL_ALL = 52,                       /* ALL  */
  YYSYMBOL_CASE = 53,                      /* CASE  */
  YYSYMBOL_WHEN = 54,                      /* WHEN  */
  YYSYMBOL_THEN = 55,                      /* THEN  */
  YYSYMBOL_ELSE = 56,                      /* ELSE  */
  YYSYMBOL_END_P = 57,                     /* END_P  */
  YYSYMBOL_ON = 58,                        /* ON  */
  YYSYMBOL_SHORTESTPATH = 59,              /* SHORTESTPATH  */
  YYSYMBOL_ALLSHORTESTPATHS = 60,          /* ALLSHORTESTPATHS  */
  YYSYMBOL_PATTERN = 61,                   /* PATTERN  */
  YYSYMBOL_EXPLAIN = 62,                   /* EXPLAIN  */
  YYSYMBOL_LOAD = 63,                      /* LOAD  */
  YYSYMBOL_CSV = 64,                       /* CSV  */
  YYSYMBOL_FROM = 65,                      /* FROM  */
  YYSYMBOL_HEADERS = 66,                   /* HEADERS  */
  YYSYMBOL_FIELDTERMINATOR = 67,           /* FIELDTERMINATOR  */
  YYSYMBOL_STARTS = 68,                    /* STARTS  */
  YYSYMBOL_ENDS = 69,                      /* ENDS  */
  YYSYMBOL_CONTAINS = 70,                  /* CONTAINS  */
  YYSYMBOL_71_ = 71,                       /* '='  */
  YYSYMBOL_72_ = 72,                       /* '<'  */
  YYSYMBOL_73_ = 73,                       /* '>'  */
  YYSYMBOL_74_ = 74,                       /* '+'  */
  YYSYMBOL_75_ = 75,                       /* '-'  */
  YYSYMBOL_76_ = 76,                       /* '*'  */
  YYSYMBOL_77_ = 77,                       /* '/'  */
  YYSYMBOL_78_ = 78,                       /* '%'  */
  YYSYMBOL_79_ = 79,                       /* '^'  */
  YYSYMBOL_80_ = 80,                       /* '.'  */
  YYSYMBOL_UNARY_MINUS = 81,               /* UNARY_MINUS  */
  YYSYMBOL_UNARY_PLUS = 82,                /* UNARY_PLUS  */
  YYSYMBOL_83_ = 83,                       /* '('  */
  YYSYMBOL_84_ = 84,                       /* '|'  */
  YYSYMBOL_85_ = 85,                       /* ')'  */
  YYSYMBOL_86_ = 86,                       /* ','  */
  YYSYMBOL_87_ = 87,                       /* ':'  */
  YYSYMBOL_88_ = 88,                       /* '['  */
  YYSYMBOL_89_ = 89,                       /* ']'  */
  YYSYMBOL_90_ = 90,                       /* '{'  */
  YYSYMBOL_91_ = 91,                       /* '}'  */
  YYSYMBOL_YYACCEPT = 92,                  /* $accept  */
  YYSYMBOL_stmt = 93,                      /* stmt  */
  YYSYMBOL_union_query = 94,               /* union_query  */
  YYSYMBOL_single_query = 95,              /* single_query  */
  YYSYMBOL_clause_list = 96,               /* clause_list  */
  YYSYMBOL_clause = 97,                    /* clause  */
  YYSYMBOL_match_clause = 98,              /* match_clause  */
  YYSYMBOL_from_graph_opt = 99,            /* from_graph_opt  */
  YYSYMBOL_optional_opt = 100,             /* optional_opt  */
  YYSYMBOL_return_clause = 101,            /* return_clause  */
  YYSYMBOL_with_clause = 102,              /* with_clause  */
  YYSYMBOL_unwind_clause = 103,            /* unwind_clause  */
  YYSYMBOL_foreach_clause = 104,           /* foreach_clause  */
  YYSYMBOL_load_csv_clause = 105,          /* load_csv_clause  */
  YYSYMBOL_foreach_update_list = 106,      /* foreach_update_list  */
  YYSYMBOL_distinct_opt = 107,             /* distinct_opt  */
  YYSYMBOL_order_by_opt = 108,             /* order_by_opt  */
  YYSYMBOL_skip_opt = 109,                 /* skip_opt  */
  YYSYMBOL_limit_opt = 110,                /* limit_opt  */
  YYSYMBOL_where_opt = 111,                /* where_opt  */
  YYSYMBOL_order_by_list = 112,            /* order_by_list  */
  YYSYMBOL_order_by_item = 113,            /* order_by_item  */
  YYSYMBOL_return_item_list = 114,         /* return_item_list  */
  YYSYMBOL_return_item = 115,              /* return_item  */
  YYSYMBOL_set_item_list = 116,            /* set_item_list  */
  YYSYMBOL_set_item = 117,                 /* set_item  */
  YYSYMBOL_create_clause = 118,            /* create_clause  */
  YYSYMBOL_merge_clause = 119,             /* merge_clause  */
  YYSYMBOL_on_create_clause = 120,         /* on_create_clause  */
  YYSYMBOL_on_match_clause = 121,          /* on_match_clause  */
  YYSYMBOL_set_clause = 122,               /* set_clause  */
  YYSYMBOL_delete_clause = 123,            /* delete_clause  */
  YYSYMBOL_delete_item_list = 124,         /* delete_item_list  */
  YYSYMBOL_delete_item = 125,              /* delete_item  */
  YYSYMBOL_remove_clause = 126,            /* remove_clause  */
  YYSYMBOL_remove_item_list = 127,         /* remove_item_list  */
  YYSYMBOL_remove_item = 128,              /* remove_item  */
  YYSYMBOL_detach_opt = 129,               /* detach_opt  */
  YYSYMBOL_pattern_list = 130,             /* pattern_list  */
  YYSYMBOL_simple_path = 131,              /* simple_path  */
  YYSYMBOL_path = 132,                     /* path  */
  YYSYMBOL_node_pattern = 133,             /* node_pattern  */
  YYSYMBOL_rel_pattern = 134,              /* rel_pattern  */
  YYSYMBOL_variable_opt = 135,             /* variable_opt  */
  YYSYMBOL_varlen_range_opt = 136,         /* varlen_range_opt  */
  YYSYMBOL_label_opt = 137,                /* label_opt  */
  YYSYMBOL_label_list = 138,               /* label_list  */
  YYSYMBOL_rel_type_list = 139,            /* rel_type_list  */
  YYSYMBOL_expr = 140,                     /* expr  */
  YYSYMBOL_primary_expr = 141,             /* primary_expr  */
  YYSYMBOL_literal_expr = 142,             /* literal_expr  */
  YYSYMBOL_function_call = 143,            /* function_call  */
  YYSYMBOL_list_predicate = 144,           /* list_predicate  */
  YYSYMBOL_reduce_expr = 145,              /* reduce_expr  */
  YYSYMBOL_argument_list = 146,            /* argument_list  */
  YYSYMBOL_list_literal = 147,             /* list_literal  */
  YYSYMBOL_list_comprehension = 148,       /* list_comprehension  */
  YYSYMBOL_pattern_comprehension = 149,    /* pattern_comprehension  */
  YYSYMBOL_map_literal = 150,              /* map_literal  */
  YYSYMBOL_map_projection = 151,           /* map_projection  */
  YYSYMBOL_map_projection_list = 152,      /* map_projection_list  */
  YYSYMBOL_map_projection_item = 153,      /* map_projection_item  */
  YYSYMBOL_case_expression = 154,          /* case_expression  */
  YYSYMBOL_when_clause_list = 155,         /* when_clause_list  */
  YYSYMBOL_when_clause = 156,              /* when_clause  */
  YYSYMBOL_literal = 157,                  /* literal  */
  YYSYMBOL_identifier = 158,               /* identifier  */
  YYSYMBOL_parameter = 159,                /* parameter  */
  YYSYMBOL_properties_opt = 160,           /* properties_opt  */
  YYSYMBOL_map_pair_list = 161,            /* map_pair_list  */
  YYSYMBOL_map_pair = 162                  /* map_pair  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;


/* Default (constant) value used for initialization for null
   right-hand sides.  Unlike the standard yacc.c template, here we set
   the default value of $$ to a zeroed-out value.  Since the default
   value is undefined, this behavior is technically correct.  */
static YYSTYPE yyval_default;
static YYLTYPE yyloc_default
# if defined CYPHER_YYLTYPE_IS_TRIVIAL && CYPHER_YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;



#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


#ifndef YYFREE
# define YYFREE free
#endif
#ifndef YYMALLOC
# define YYMALLOC malloc
#endif
#ifndef YYREALLOC
# define YYREALLOC realloc
#endif

#ifdef __cplusplus
  typedef bool yybool;
# define yytrue true
# define yyfalse false
#else
  /* When we move to stdbool, get rid of the various casts to yybool.  */
  typedef signed char yybool;
# define yytrue 1
# define yyfalse 0
#endif

#ifndef YYSETJMP
# include <setjmp.h>
# define YYJMP_BUF jmp_buf
# define YYSETJMP(Env) setjmp (Env)
/* Pacify Clang and ICC.  */
# define YYLONGJMP(Env, Val)                    \
 do {                                           \
   longjmp (Env, Val);                          \
   YY_ASSERT (0);                               \
 } while (yyfalse)
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

/* The _Noreturn keyword of C11.  */
#ifndef _Noreturn
# if (defined __cplusplus \
      && ((201103 <= __cplusplus && !(__GNUC__ == 4 && __GNUC_MINOR__ == 7)) \
          || (defined _MSC_VER && 1900 <= _MSC_VER)))
#  define _Noreturn [[noreturn]]
# elif ((!defined __cplusplus || defined __clang__) \
        && (201112 <= (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) \
            || (!defined __STRICT_ANSI__ \
                && (4 < __GNUC__ + (7 <= __GNUC_MINOR__) \
                    || (defined __apple_build_version__ \
                        ? 6000000 <= __apple_build_version__ \
                        : 3 < __clang_major__ + (5 <= __clang_minor__))))))
   /* _Noreturn works as-is.  */
# elif (2 < __GNUC__ + (8 <= __GNUC_MINOR__) || defined __clang__ \
        || 0x5110 <= __SUNPRO_C)
#  define _Noreturn __attribute__ ((__noreturn__))
# elif 1200 <= (defined _MSC_VER ? _MSC_VER : 0)
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn
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

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  94
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   2077

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  92
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  71
/* YYNRULES -- Number of rules.  */
#define YYNRULES  246
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  537
/* YYMAXRHS -- Maximum number of symbols on right-hand side of rule.  */
#define YYMAXRHS 13
/* YYMAXLEFT -- Maximum number of symbols to the left of a handle
   accessed by $0, $-1, etc., in any rule.  */
#define YYMAXLEFT 0

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   327

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
       2,     2,     2,     2,     2,     2,     2,    78,     2,     2,
      83,    85,    76,    74,    86,    75,    80,    77,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    87,     2,
      72,    71,    73,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    88,     2,    89,    79,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    90,    84,    91,     2,     2,     2,     2,
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
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    81,    82
};

#if CYPHER_YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   160,   160,   165,   177,   181,   185,   192,   199,   204,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   221,
     222,   227,   234,   235,   242,   243,   248,   256,   264,   273,
     282,   288,   294,   301,   312,   317,   322,   327,   332,   337,
     342,   347,   352,   357,   362,   367,   375,   376,   380,   381,
     385,   386,   390,   391,   395,   396,   400,   405,   413,   414,
     415,   420,   425,   433,   437,   445,   450,   458,   462,   475,
     483,   487,   491,   495,   499,   506,   513,   521,   529,   536,
     541,   549,   558,   565,   570,   578,   587,   599,   604,   611,
     616,   625,   631,   650,   654,   660,   667,   672,   679,   687,
     702,   706,   711,   716,   723,   727,   732,   737,   744,   748,
     753,   758,   767,   768,   769,   770,   776,   777,   779,   781,
     783,   785,   790,   791,   796,   803,   810,   817,   827,   837,
     848,   858,   869,   879,   890,   900,   911,   918,   925,   932,
     943,   944,   945,   964,   965,   966,   967,   968,   969,   970,
     971,   972,   973,   974,   975,   976,   977,   978,   979,   980,
     981,   982,   983,   984,   985,   986,   990,   991,   992,   993,
     994,   995,   996,   997,   998,   999,  1000,  1001,  1002,  1009,
    1016,  1023,  1030,  1035,  1043,  1047,  1062,  1088,  1095,  1103,
    1108,  1118,  1122,  1127,  1136,  1141,  1146,  1151,  1160,  1171,
    1176,  1185,  1189,  1206,  1211,  1216,  1221,  1238,  1251,  1268,
    1272,  1280,  1289,  1294,  1302,  1308,  1313,  1320,  1334,  1338,
    1343,  1347,  1354,  1359,  1367,  1374,  1378,  1382,  1387,  1391,
    1395,  1402,  1407,  1412,  1420,  1429,  1430,  1431,  1438,  1443,
    1451,  1455,  1459,  1463,  1467,  1471,  1475
};
#endif

#define YYPACT_NINF (-369)
#define YYTABLE_NINF (-149)

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     241,    57,    16,    57,   584,    46,    16,   639,  -369,    40,
    -369,   466,    91,   201,   118,  -369,   187,  -369,  -369,   215,
    -369,  -369,  -369,  -369,  -369,  -369,  -369,  -369,  -369,  -369,
     194,  -369,   639,   204,   207,   223,    23,   224,    58,  -369,
    -369,   639,  -369,  -369,  -369,   205,  -369,  -369,   639,  -369,
    -369,  -369,   244,   253,   262,   265,   276,   292,   384,   300,
     310,   316,   319,   639,   639,   639,   474,    12,   326,  -369,
    1915,  -369,  -369,  -369,  -369,  -369,   328,  -369,  -369,  -369,
    -369,  -369,  -369,  -369,  -369,   -54,   339,  -369,   -26,   225,
    1483,   420,   118,    -1,  -369,   383,  -369,    16,   433,    -2,
    -369,  1535,    94,   357,   357,  -369,  -369,  -369,   355,    16,
     368,   356,   357,    -2,   439,   294,   441,   639,     2,   901,
      18,   442,   443,   444,   449,   450,   639,  1577,   116,  -369,
     451,   639,   639,   639,  -369,  -369,  1105,    24,   694,  -369,
     109,   373,   374,   376,   377,   378,   381,   382,  -369,   169,
    -369,   584,   639,   639,   639,   639,   639,   639,   639,   639,
     271,   446,   453,   639,   639,   639,   639,   639,   639,   639,
     639,   639,   639,   456,   464,    46,   266,   413,   417,   470,
     489,   639,   455,   431,   493,   466,  -369,    50,  -369,   414,
    -369,   468,   639,   469,   496,   421,   422,    58,    96,   148,
     288,   418,   416,  -369,   423,    23,  -369,   469,  -369,   639,
     424,  -369,  1957,   161,   166,   851,   425,     9,   186,  -369,
     126,   168,   472,   475,   487,   459,   497,  1619,   171,   639,
    -369,  -369,  -369,   268,   312,   333,   452,   639,   191,   239,
     245,   355,  -369,   639,   639,   639,   639,   639,   639,   639,
     213,  -369,  -369,    13,    13,    13,    13,   901,   739,   826,
    -369,   467,  -369,   639,   639,    13,  1999,    13,    13,   102,
     102,   379,   379,   379,   876,  -369,  -369,  -369,   518,   520,
     530,  -369,   527,  -369,  -369,  -369,    13,   639,   482,   516,
    -369,   545,   533,   433,   639,  -369,   639,   521,  -369,   357,
     357,  -369,  -369,  -369,  -369,    31,   471,   325,    23,   -41,
     521,  1156,  -369,  -369,   639,  -369,   529,  -369,  -369,     2,
    -369,   547,  -369,   639,   639,   639,   639,   639,   639,   639,
    -369,  1661,  -369,  -369,  -369,   639,   756,   418,  1957,  1957,
    1957,  1957,  1957,  1957,  1957,  -369,  -369,    13,    13,  -369,
     584,   584,  1343,   550,   552,  -369,   639,  -369,  -369,   473,
    -369,  1465,  1957,   639,  -369,   160,   198,  -369,   200,  -369,
    -369,  -369,    41,   164,   365,   418,   533,  -369,  1957,   554,
    1957,  -369,   476,  1704,  1746,  1788,  1086,  1830,  1957,  1872,
    -369,   921,   639,   639,  -369,   480,   326,   326,   334,   532,
     500,  1957,   639,  -369,  -369,  1957,  -369,  -369,  -369,   366,
     418,   556,   567,   -31,    29,   150,   491,  -369,  -369,  -369,
     639,   639,   639,   565,   639,  -369,  -369,   781,   946,    58,
    -369,   111,  -369,  -369,  -369,  -369,  -369,   577,   579,  -369,
     -31,    29,   150,   504,   582,  -369,     5,   418,     6,   418,
      15,   418,   519,  1176,  1201,  1253,   555,  1272,   639,  -369,
    -369,   357,  -369,  -369,  -369,  -369,  -369,  -369,  -369,   528,
    -369,   418,   418,   418,   525,  -369,  -369,  -369,   370,   512,
    -369,  -369,   375,   513,  -369,  -369,   388,   517,   534,  -369,
    -369,  -369,   639,  -369,   991,    -3,   600,   522,   524,   526,
    -369,  -369,  -369,   535,  -369,  -369,   539,  -369,  -369,   541,
    -369,  1395,  -369,   639,   639,  -369,   543,   546,   548,   549,
     553,   562,   639,  1413,  1016,  -369,  -369,  -369,  -369,  -369,
    -369,  1324,   639,  -369,  -369,  1061,  -369
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
      24,    46,     0,    46,     0,     0,     0,     0,    87,     0,
      25,    24,     0,     0,     2,     4,     7,     8,    10,     0,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,    47,     0,     0,     0,     0,   112,    69,    93,    89,
      91,     0,   225,   226,   227,   231,   234,   232,     0,   230,
     228,   229,     0,     0,     0,     0,     0,     0,     0,   233,
       0,     0,     0,     0,     0,     0,     0,     0,    77,    65,
       0,   140,   166,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   184,   167,   168,     0,    82,    83,    70,   231,
       0,     0,     3,     0,     1,    24,     9,     0,     0,    48,
      61,    63,     0,     0,     0,   113,   114,   115,   122,     0,
       0,     0,     0,    48,     0,     0,     0,     0,     0,   162,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   222,
       0,     0,     0,     0,   141,   142,     0,   231,   112,   201,
       0,     0,     0,     0,     0,     0,     0,     0,   209,     0,
     238,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    71,    72,     0,
       0,     0,     0,     0,     0,    24,     5,    22,    81,    78,
      79,     0,     0,    50,     0,     0,     0,    94,     0,     0,
       0,   235,   123,    90,     0,   112,    92,    50,   178,     0,
       0,   185,   199,     0,   180,     0,     0,     0,     0,   212,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     218,   223,   179,     0,     0,     0,   165,     0,   231,   232,
     233,   122,   202,     0,     0,     0,     0,     0,     0,     0,
       0,   210,    66,   149,   152,   153,   154,   155,   156,   157,
     158,     0,   163,     0,     0,   161,    67,   150,   151,   143,
     144,   145,   146,   147,     0,    85,    86,    84,     0,     0,
       0,    73,     0,    74,   180,    28,   148,     0,     0,     0,
       6,     0,    54,     0,     0,    62,     0,    52,    64,     0,
       0,    96,    98,   124,   125,     0,     0,     0,   112,   116,
      52,     0,   188,   186,     0,   181,     0,   214,   215,     0,
     211,     0,   189,     0,     0,     0,     0,     0,     0,     0,
     220,     0,   192,   193,   191,     0,     0,   235,   242,   240,
     241,   245,   246,   243,   244,   239,   164,   159,   160,   183,
       0,     0,     0,     0,     0,    23,     0,    21,    80,    49,
      56,    58,    51,     0,    26,     0,     0,   236,     0,    99,
     126,   127,   116,   117,     0,   235,    54,   187,   200,     0,
     217,   213,     0,     0,     0,     0,     0,     0,   224,     0,
     219,     0,     0,     0,   203,     0,    76,    75,    88,     0,
      30,    55,     0,    59,    60,    53,    95,    97,   237,     0,
     235,   118,     0,   116,   116,   116,     0,    27,   216,   190,
       0,     0,     0,     0,     0,   221,   182,     0,     0,     0,
      39,    88,    34,    37,    35,    36,    38,     0,     0,    57,
     116,   116,   116,     0,   120,   121,     0,   235,     0,   235,
       0,   235,     0,     0,     0,     0,     0,     0,     0,   204,
     205,     0,    29,    45,    40,    43,    41,    42,    44,    31,
      32,   235,   235,   235,     0,   119,   128,   130,     0,     0,
     132,   134,     0,     0,   136,   137,     0,     0,   108,   195,
     196,   197,     0,   194,     0,     0,     0,     0,     0,     0,
     104,   129,   131,     0,   133,   135,     0,   138,   139,     0,
     100,     0,   206,     0,     0,    33,     0,     0,     0,   109,
     110,   111,     0,     0,     0,   105,   106,   107,   101,   102,
     103,     0,     0,   207,   198,     0,   208
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -369,  -369,   597,   -85,  -369,   604,  -369,  -369,  -369,  -369,
    -369,  -369,  -368,  -369,  -369,   622,   536,   432,   330,   272,
    -369,   236,     8,   458,    72,   505,  -360,  -358,   477,   483,
    -348,  -304,  -369,   358,  -292,  -369,   486,  -369,     1,  -100,
     557,  -111,   228,  -133,  -345,   427,  -369,   254,    -7,  -369,
    -369,  -369,  -369,  -369,   234,  -369,  -369,  -369,  -369,  -369,
    -369,   343,  -369,   537,  -119,  -369,  -369,  -369,  -331,   360,
     419
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    13,    14,    15,    16,    17,    18,   292,    19,    20,
      21,    22,    23,    24,   431,    32,   193,   297,   364,   357,
     359,   360,    99,   100,    68,    69,    25,    26,   177,   178,
      27,    28,   189,   190,    29,    86,    87,    30,    37,    38,
      39,    40,   112,   108,   375,   201,   202,   415,    70,    71,
      72,    73,    74,    75,   213,    76,    77,    78,    79,    80,
     218,   219,    81,   128,   129,    82,    83,    84,   306,   149,
     150
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      90,   206,   197,   198,   199,   241,   395,    88,   216,   231,
     186,   476,   480,   477,   481,   317,   513,   141,   142,   183,
     143,   484,    33,   485,   220,   101,   173,   410,   191,   105,
     430,   106,   176,   174,   101,   373,   141,   142,   432,   143,
     433,   119,   144,   145,   416,   373,   374,   146,   147,   113,
     434,   127,    85,   446,   159,   160,   134,   135,   136,   101,
     109,   144,   145,   463,   184,   237,   146,   147,   447,   449,
     451,   464,   309,   465,   140,    34,    35,    34,    35,   443,
     107,   514,   217,   466,   192,   318,    31,   167,   168,   169,
     170,   171,   478,   482,   435,   471,   472,   473,   187,    36,
     290,    36,   486,   148,   114,   373,   436,   115,   212,   231,
     215,   179,   117,   448,   118,   291,   479,   373,   483,   227,
     487,   221,   367,    91,   212,   212,   212,   467,   409,     2,
     110,   136,     4,   111,     5,     6,   109,     8,     9,   468,
     497,   498,   499,   159,   160,   253,   254,   255,   256,   257,
     258,   259,   260,   195,   196,    93,   265,   266,   267,   268,
     269,   270,   271,   272,   273,   274,   -68,   411,   110,    95,
     126,   111,   229,   230,   286,   372,   412,    36,   169,   170,
     171,   301,   -68,   -68,   -68,   101,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,   -68,   -68,   192,   462,   102,   242,   365,
     366,    94,   311,   -24,     1,     2,   321,     3,     4,   -88,
       5,     6,     7,     8,     9,    10,    98,   -68,   141,   142,
     110,   143,   331,   111,   -68,   126,   373,   329,   330,   -68,
     336,    97,   110,   302,   450,   111,   338,   339,   340,   341,
     342,   343,   344,   144,   145,   406,   313,   314,   146,   147,
      12,   -68,   -68,   322,   109,   250,   347,   348,     1,     2,
     251,     3,     4,   -88,     5,     6,     7,     8,     9,    10,
     110,   114,   319,   111,   115,   102,  -113,   320,   179,   117,
     352,   118,   278,   407,   279,   114,   250,   361,   115,   362,
     103,   408,   116,   117,   303,   118,   304,    42,    43,    44,
      89,    46,    47,    11,    12,   114,   104,   378,   115,   380,
     109,   261,   179,   117,   262,   118,   383,   384,   385,   386,
     387,   388,   389,   209,  -114,   130,  -114,   120,   391,  -114,
    -115,   370,  -115,   371,    48,  -115,   121,    49,    50,    51,
      52,    53,    54,    55,    56,   122,    57,    58,   123,   401,
     495,    59,     2,   332,   314,     4,   405,     5,     6,   124,
       8,     9,    60,    61,    62,   233,   234,   235,    63,    64,
     210,   413,   440,   414,   441,   125,   501,    65,   502,   211,
     130,   504,    66,   505,    67,   427,   428,    42,    43,    44,
      89,    46,    47,   131,   507,   361,   508,   333,   314,   132,
       1,     2,   133,     3,     4,   -88,     5,     6,     7,     8,
       9,    10,   151,   453,   454,   455,   172,   457,   334,   314,
     159,   160,   396,   397,    48,   175,   182,    49,    50,    51,
      52,    53,    54,    55,    56,   185,    57,    58,   126,   188,
      36,    59,   200,   204,   205,   208,    12,   214,   222,   223,
     224,   494,    60,    61,    62,   225,   226,   232,    63,    64,
     243,   244,   275,   245,   246,   247,   263,    65,   248,   249,
     276,   280,    66,   264,    67,   282,   284,    42,    43,    44,
     137,    46,    47,     1,     2,   511,     3,     4,   -88,     5,
       6,     7,     8,     9,    10,   285,   287,   288,   289,   294,
     293,   296,   298,   307,   299,   300,   523,   524,   305,   312,
     346,   308,   316,   323,    48,   531,   324,    49,    50,    51,
      52,    53,    54,    55,    56,   535,    57,    58,   325,    12,
     326,    59,    42,    43,    44,    89,    46,    47,   327,   350,
     335,   351,    60,    61,    62,   279,   278,   353,    63,    64,
     354,   355,   356,   382,   363,   399,   369,   138,   400,   402,
     418,   419,    66,   139,    67,   429,   437,   438,   444,    48,
     445,   456,    49,    50,    51,    52,    53,    54,    55,    56,
     452,    57,    58,   469,   470,   475,    59,    42,    43,    44,
      45,    46,    47,   474,   488,   496,   492,    60,    61,    62,
     500,   503,   506,    63,    64,   515,   509,   510,    92,   379,
     519,   516,    65,   517,   520,   518,   521,    66,   525,    67,
      96,   526,   528,   527,    48,    41,   529,    49,    50,    51,
      52,    53,    54,    55,    56,   530,    57,    58,   439,   310,
     376,    59,    42,    43,    44,    89,    46,    47,   417,   207,
     295,   358,    60,    61,    62,   283,   252,   461,    63,    64,
     281,   277,   381,   442,   228,   368,   203,    65,   337,   345,
       0,     0,    66,     0,    67,     0,     0,     0,     0,    48,
       0,     0,    49,    50,    51,    52,    53,    54,    55,    56,
       0,    57,    58,     0,     0,     0,    59,    42,    43,    44,
     238,    46,   239,     0,     0,     0,     0,    60,    61,    62,
       0,     0,     0,    63,    64,     0,     0,     0,     0,     0,
       0,     0,    65,     0,     0,     0,     0,    66,     0,    67,
       0,     0,     0,     0,    48,     0,     0,    49,    50,    51,
      52,    53,    54,    55,    56,     0,    57,    58,   152,   153,
     154,   240,     0,     0,   155,     0,     0,     0,     0,     0,
       0,     0,    60,    61,    62,   152,   153,   154,    63,    64,
       0,   155,     0,     0,     0,   392,   156,    65,   158,     0,
     159,   160,    66,     0,    67,     0,     0,     0,     0,     0,
     152,   153,   154,   156,   157,   158,   155,   159,   160,     0,
       0,     0,     0,     0,     0,     0,     0,   161,   162,   163,
     181,   165,   166,   167,   168,   169,   170,   171,   156,   157,
     158,     0,   159,   160,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,   152,   153,   154,     0,     0,
     393,   155,     0,     0,     0,   394,     0,     0,     0,   161,
     162,   163,   181,   165,   166,   167,   168,   169,   170,   171,
     152,   153,   154,   156,     0,   458,   155,   159,   160,     0,
     459,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   152,   153,   154,   156,   157,
     158,   155,   159,   160,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,     0,     0,     0,     0,     0,
     152,   153,   154,   156,   157,   158,   155,   159,   160,   161,
     162,   163,   181,   165,   166,   167,   168,   169,   170,   171,
     152,   153,   154,     0,     0,     0,   155,     0,     0,     0,
     315,     0,   159,   160,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,   152,   153,   154,   156,   157,
     158,   155,   159,   160,     0,   349,     0,     0,     0,   161,
     162,   163,   181,   165,   166,   167,   168,   169,   170,   171,
       0,     0,     0,   156,   157,   158,     0,   159,   160,   161,
     162,   163,   181,   165,   166,   167,   168,   169,   170,   171,
     152,   153,   154,     0,     0,     0,   155,     0,     0,     0,
     426,     0,     0,     0,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,   152,   153,   154,   156,   157,
     158,   155,   159,   160,     0,   460,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   156,   157,   158,     0,   159,   160,   161,
     162,   163,   181,   165,   166,   167,   168,   169,   170,   171,
     152,   153,   154,     0,     0,     0,   155,     0,     0,     0,
     512,     0,     0,     0,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,   152,   153,   154,   156,   157,
     158,   155,   159,   160,     0,   533,     0,     0,     0,     0,
       0,     0,     0,     0,   152,   153,   154,     0,     0,     0,
     155,     0,     0,   156,   157,   158,     0,   159,   160,   161,
     162,   163,   181,   165,   166,   167,   168,   169,   170,   171,
       0,     0,   156,   157,   158,     0,   159,   160,     0,     0,
     536,     0,     0,     0,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,   152,   153,   154,     0,     0,
       0,   155,   423,   161,   162,   163,   181,   165,   166,   167,
     168,   169,   170,   171,     0,   152,   153,   154,     0,     0,
     236,   155,     0,   156,   157,   158,     0,   159,   160,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     152,   153,   154,   156,   157,   158,   155,   159,   160,     0,
       0,     0,     0,     0,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,     0,     0,     0,   156,   157,
     158,   377,   159,   160,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,     0,     0,     0,     0,     0,
       0,   489,   152,   153,   154,     0,     0,     0,   155,   161,
     162,   163,   181,   165,   166,   167,   168,   169,   170,   171,
       0,   152,   153,   154,     0,     0,   490,   155,     0,     0,
     156,   157,   158,     0,   159,   160,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   156,
     157,   158,     0,   159,   160,     0,     0,     0,     0,     0,
       0,   161,   162,   163,   181,   165,   166,   167,   168,   169,
     170,   171,     0,   152,   153,   154,     0,     0,   491,   155,
     161,   162,   163,   181,   165,   166,   167,   168,   169,   170,
     171,     0,   152,   153,   154,     0,     0,   493,   155,     0,
       0,   156,   157,   158,     0,   159,   160,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     156,   157,   158,     0,   159,   160,     0,     0,     0,     0,
       0,     0,   161,   162,   163,   181,   165,   166,   167,   168,
     169,   170,   171,     0,   152,   153,   154,     0,     0,   534,
     155,   161,   162,   163,   181,   165,   166,   167,   168,   169,
     170,   171,   152,   153,   154,     0,     0,   398,   155,     0,
       0,     0,   156,   157,   158,     0,   159,   160,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     156,   157,   158,     0,   159,   160,     0,     0,     0,     0,
       0,     0,     0,   161,   162,   163,   181,   165,   166,   167,
     168,   169,   170,   171,   152,   153,   154,     0,     0,   522,
     155,   161,   162,   163,   181,   165,   166,   167,   168,   169,
     170,   171,   152,   153,   154,     0,     0,   532,   155,     0,
     403,   404,   156,   157,   158,     0,   159,   160,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   180,     0,     0,
     156,   157,   158,     0,   159,   160,     0,     0,     0,     0,
       0,     0,     0,   161,   162,   163,   181,   165,   166,   167,
     168,   169,   170,   171,   152,   153,   154,     0,     0,     0,
     155,   161,   162,   163,   181,   165,   166,   167,   168,   169,
     170,   171,     0,     0,     0,     0,     0,     0,     0,   194,
       0,     0,   156,   157,   158,     0,   159,   160,     0,     0,
       0,     0,     0,     0,     0,     0,   152,   153,   154,     0,
       0,     0,   155,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   161,   162,   163,   181,   165,   166,   167,
     168,   169,   170,   171,   156,   157,   158,     0,   159,   160,
       0,     0,     0,     0,     0,     0,     0,     0,   152,   153,
     154,   126,     0,     0,   155,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   161,   162,   163,   181,   165,
     166,   167,   168,   169,   170,   171,   156,   157,   158,     0,
     159,   160,     0,     0,     0,     0,     0,     0,     0,     0,
     152,   153,   154,     0,   328,     0,   155,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   161,   162,   163,
     181,   165,   166,   167,   168,   169,   170,   171,   156,   157,
     158,     0,   159,   160,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   152,   153,   154,     0,     0,   390,   155,
       0,     0,     0,   420,     0,     0,     0,     0,     0,   161,
     162,   163,   181,   165,   166,   167,   168,   169,   170,   171,
       0,   156,   157,   158,     0,   159,   160,     0,     0,     0,
       0,     0,     0,     0,     0,   152,   153,   154,     0,     0,
       0,   155,     0,     0,     0,   421,     0,     0,     0,     0,
       0,     0,   161,   162,   163,   181,   165,   166,   167,   168,
     169,   170,   171,   156,   157,   158,     0,   159,   160,     0,
       0,     0,     0,     0,     0,     0,     0,   152,   153,   154,
       0,     0,     0,   155,     0,     0,     0,   422,     0,     0,
       0,     0,     0,     0,   161,   162,   163,   181,   165,   166,
     167,   168,   169,   170,   171,   156,   157,   158,     0,   159,
     160,     0,     0,     0,     0,     0,     0,     0,     0,   152,
     153,   154,     0,     0,     0,   155,     0,     0,     0,   424,
       0,     0,     0,     0,     0,     0,   161,   162,   163,   181,
     165,   166,   167,   168,   169,   170,   171,   156,   157,   158,
       0,   159,   160,     0,     0,     0,     0,     0,     0,     0,
       0,   152,   153,   154,     0,     0,     0,   155,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   161,   162,
     163,   181,   165,   166,   167,   168,   169,   170,   171,   156,
     157,   158,     0,   159,   160,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   152,   153,   154,     0,     0,   425,
     155,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     161,   162,   163,   181,   165,   166,   167,   168,   169,   170,
     171,     0,   156,   157,   158,     0,   159,   160,     0,     0,
       0,     0,     0,     0,     0,     0,   152,   153,   154,     0,
       0,     0,   155,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   156,   157,   158,     0,   159,   160,
       0,     0,     0,     0,     0,     0,     0,     0,  -148,  -148,
    -148,     0,     0,     0,  -148,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   161,   162,   163,   181,   165,
     166,   167,   168,   169,   170,   171,  -148,  -148,  -148,     0,
     159,   160,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -148,  -148,  -148,
    -148,  -148,  -148,   167,   168,   169,   170,   171
};

static const yytype_int16 yycheck[] =
{
       7,   112,   102,   103,   104,   138,   337,     6,     6,   128,
      95,     6,     6,     8,     8,     6,    19,     5,     6,    20,
       8,     6,     6,     8,     6,    32,    80,   372,    30,     6,
     398,     8,    58,    87,    41,    76,     5,     6,   398,     8,
     398,    48,    30,    31,   375,    76,    87,    35,    36,    41,
     398,    58,     6,    84,    41,    42,    63,    64,    65,    66,
      86,    30,    31,   431,    65,    41,    35,    36,   413,   414,
     415,   431,   205,   431,    66,    59,    60,    59,    60,   410,
      57,    84,    80,   431,    86,    76,    29,    74,    75,    76,
      77,    78,    87,    87,   398,   440,   441,   442,    97,    83,
     185,    83,    87,    91,    80,    76,   398,    83,   115,   228,
     117,    87,    88,    84,    90,    65,   447,    76,   449,   126,
     451,   120,    91,    83,   131,   132,   133,   431,    87,    18,
      72,   138,    21,    75,    23,    24,    86,    26,    27,   431,
     471,   472,   473,    41,    42,   152,   153,   154,   155,   156,
     157,   158,   159,    59,    60,    64,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,     0,     3,    72,    51,
      54,    75,    56,    57,   181,   308,    12,    83,    76,    77,
      78,    85,    16,    17,    18,   192,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    86,    85,    71,    89,   299,
     300,     0,   209,    16,    17,    18,    80,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    22,    51,     5,     6,
      72,     8,   229,    75,    58,    54,    76,    56,    57,    63,
     237,    16,    72,    85,    84,    75,   243,   244,   245,   246,
     247,   248,   249,    30,    31,    85,    85,    86,    35,    36,
      63,    85,    86,    85,    86,    86,   263,   264,    17,    18,
      91,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      72,    80,    86,    75,    83,    71,    85,    91,    87,    88,
     287,    90,    16,    85,    18,    80,    86,   294,    83,   296,
      83,    91,    87,    88,     6,    90,     8,     3,     4,     5,
       6,     7,     8,    62,    63,    80,    83,   314,    83,   316,
      86,    40,    87,    88,    43,    90,   323,   324,   325,   326,
     327,   328,   329,    29,    85,    80,    87,    83,   335,    90,
      85,     6,    87,     8,    40,    90,    83,    43,    44,    45,
      46,    47,    48,    49,    50,    83,    52,    53,    83,   356,
     461,    57,    18,    85,    86,    21,   363,    23,    24,    83,
      26,    27,    68,    69,    70,   131,   132,   133,    74,    75,
      76,     6,     6,     8,     8,    83,     6,    83,     8,    85,
      80,     6,    88,     8,    90,   392,   393,     3,     4,     5,
       6,     7,     8,    83,     6,   402,     8,    85,    86,    83,
      17,    18,    83,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    86,   420,   421,   422,    88,   424,    85,    86,
      41,    42,   350,   351,    40,    86,     6,    43,    44,    45,
      46,    47,    48,    49,    50,    52,    52,    53,    54,     6,
      83,    57,    87,    75,    88,     6,    63,     6,     6,     6,
       6,   458,    68,    69,    70,     6,     6,     6,    74,    75,
      87,    87,     6,    87,    87,    87,    20,    83,    87,    87,
       6,    58,    88,    20,    90,    58,     6,     3,     4,     5,
       6,     7,     8,    17,    18,   492,    20,    21,    22,    23,
      24,    25,    26,    27,    28,     6,    41,    66,     5,    31,
      86,    32,     6,    87,    83,    83,   513,   514,    90,    85,
      43,    88,    87,    41,    40,   522,    41,    43,    44,    45,
      46,    47,    48,    49,    50,   532,    52,    53,    41,    63,
      71,    57,     3,     4,     5,     6,     7,     8,    41,    21,
      88,    21,    68,    69,    70,    18,    16,    65,    74,    75,
      34,     6,    19,     6,    33,     5,    85,    83,     6,    86,
       6,    85,    88,    89,    90,    85,    34,    67,    12,    40,
       3,     6,    43,    44,    45,    46,    47,    48,    49,    50,
      89,    52,    53,     6,     5,     3,    57,     3,     4,     5,
       6,     7,     8,    89,    75,    67,    41,    68,    69,    70,
      75,    89,    89,    74,    75,     5,    89,    73,    11,    80,
      75,    89,    83,    89,    75,    89,    75,    88,    75,    90,
      16,    75,    73,    75,    40,     3,    73,    43,    44,    45,
      46,    47,    48,    49,    50,    73,    52,    53,   402,   207,
     310,    57,     3,     4,     5,     6,     7,     8,   376,   113,
     192,   293,    68,    69,    70,   178,   151,   429,    74,    75,
     177,   175,   319,   409,   127,   305,   109,    83,   241,   250,
      -1,    -1,    88,    -1,    90,    -1,    -1,    -1,    -1,    40,
      -1,    -1,    43,    44,    45,    46,    47,    48,    49,    50,
      -1,    52,    53,    -1,    -1,    -1,    57,     3,     4,     5,
       6,     7,     8,    -1,    -1,    -1,    -1,    68,    69,    70,
      -1,    -1,    -1,    74,    75,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    83,    -1,    -1,    -1,    -1,    88,    -1,    90,
      -1,    -1,    -1,    -1,    40,    -1,    -1,    43,    44,    45,
      46,    47,    48,    49,    50,    -1,    52,    53,     9,    10,
      11,    57,    -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    68,    69,    70,     9,    10,    11,    74,    75,
      -1,    15,    -1,    -1,    -1,    19,    37,    83,    39,    -1,
      41,    42,    88,    -1,    90,    -1,    -1,    -1,    -1,    -1,
       9,    10,    11,    37,    38,    39,    15,    41,    42,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    37,    38,
      39,    -1,    41,    42,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,     9,    10,    11,    -1,    -1,
      84,    15,    -1,    -1,    -1,    89,    -1,    -1,    -1,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
       9,    10,    11,    37,    -1,    84,    15,    41,    42,    -1,
      89,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     9,    10,    11,    37,    38,
      39,    15,    41,    42,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    -1,    -1,    -1,    -1,    -1,
       9,    10,    11,    37,    38,    39,    15,    41,    42,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
       9,    10,    11,    -1,    -1,    -1,    15,    -1,    -1,    -1,
      89,    -1,    41,    42,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,     9,    10,    11,    37,    38,
      39,    15,    41,    42,    -1,    89,    -1,    -1,    -1,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      -1,    -1,    -1,    37,    38,    39,    -1,    41,    42,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
       9,    10,    11,    -1,    -1,    -1,    15,    -1,    -1,    -1,
      89,    -1,    -1,    -1,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,     9,    10,    11,    37,    38,
      39,    15,    41,    42,    -1,    89,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    39,    -1,    41,    42,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
       9,    10,    11,    -1,    -1,    -1,    15,    -1,    -1,    -1,
      89,    -1,    -1,    -1,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,     9,    10,    11,    37,    38,
      39,    15,    41,    42,    -1,    89,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     9,    10,    11,    -1,    -1,    -1,
      15,    -1,    -1,    37,    38,    39,    -1,    41,    42,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      -1,    -1,    37,    38,    39,    -1,    41,    42,    -1,    -1,
      89,    -1,    -1,    -1,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,     9,    10,    11,    -1,    -1,
      -1,    15,    86,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    -1,     9,    10,    11,    -1,    -1,
      85,    15,    -1,    37,    38,    39,    -1,    41,    42,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       9,    10,    11,    37,    38,    39,    15,    41,    42,    -1,
      -1,    -1,    -1,    -1,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    -1,    -1,    -1,    37,    38,
      39,    85,    41,    42,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    -1,    -1,    -1,    -1,    -1,
      -1,    85,     9,    10,    11,    -1,    -1,    -1,    15,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      -1,     9,    10,    11,    -1,    -1,    85,    15,    -1,    -1,
      37,    38,    39,    -1,    41,    42,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    39,    -1,    41,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    -1,     9,    10,    11,    -1,    -1,    85,    15,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    -1,     9,    10,    11,    -1,    -1,    85,    15,    -1,
      -1,    37,    38,    39,    -1,    41,    42,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    39,    -1,    41,    42,    -1,    -1,    -1,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    -1,     9,    10,    11,    -1,    -1,    85,
      15,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,     9,    10,    11,    -1,    -1,    84,    15,    -1,
      -1,    -1,    37,    38,    39,    -1,    41,    42,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    39,    -1,    41,    42,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,     9,    10,    11,    -1,    -1,    84,
      15,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,     9,    10,    11,    -1,    -1,    84,    15,    -1,
      35,    36,    37,    38,    39,    -1,    41,    42,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    -1,    -1,
      37,    38,    39,    -1,    41,    42,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,     9,    10,    11,    -1,    -1,    -1,
      15,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,
      -1,    -1,    37,    38,    39,    -1,    41,    42,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     9,    10,    11,    -1,
      -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    37,    38,    39,    -1,    41,    42,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     9,    10,
      11,    54,    -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    37,    38,    39,    -1,
      41,    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       9,    10,    11,    -1,    55,    -1,    15,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    37,    38,
      39,    -1,    41,    42,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     9,    10,    11,    -1,    -1,    57,    15,
      -1,    -1,    -1,    19,    -1,    -1,    -1,    -1,    -1,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      -1,    37,    38,    39,    -1,    41,    42,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     9,    10,    11,    -1,    -1,
      -1,    15,    -1,    -1,    -1,    19,    -1,    -1,    -1,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    37,    38,    39,    -1,    41,    42,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     9,    10,    11,
      -1,    -1,    -1,    15,    -1,    -1,    -1,    19,    -1,    -1,
      -1,    -1,    -1,    -1,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    37,    38,    39,    -1,    41,
      42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     9,
      10,    11,    -1,    -1,    -1,    15,    -1,    -1,    -1,    19,
      -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    37,    38,    39,
      -1,    41,    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     9,    10,    11,    -1,    -1,    -1,    15,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    37,
      38,    39,    -1,    41,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     9,    10,    11,    -1,    -1,    57,
      15,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    -1,    37,    38,    39,    -1,    41,    42,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     9,    10,    11,    -1,
      -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    37,    38,    39,    -1,    41,    42,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     9,    10,
      11,    -1,    -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    37,    38,    39,    -1,
      41,    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    17,    18,    20,    21,    23,    24,    25,    26,    27,
      28,    62,    63,    93,    94,    95,    96,    97,    98,   100,
     101,   102,   103,   104,   105,   118,   119,   122,   123,   126,
     129,    29,   107,     6,    59,    60,    83,   130,   131,   132,
     133,   107,     3,     4,     5,     6,     7,     8,    40,    43,
      44,    45,    46,    47,    48,    49,    50,    52,    53,    57,
      68,    69,    70,    74,    75,    83,    88,    90,   116,   117,
     140,   141,   142,   143,   144,   145,   147,   148,   149,   150,
     151,   154,   157,   158,   159,     6,   127,   128,   130,     6,
     140,    83,    94,    64,     0,    51,    97,    16,    22,   114,
     115,   140,    71,    83,    83,     6,     8,    57,   135,    86,
      72,    75,   134,   114,    80,    83,    87,    88,    90,   140,
      83,    83,    83,    83,    83,    83,    54,   140,   155,   156,
      80,    83,    83,    83,   140,   140,   140,     6,    83,    89,
     114,     5,     6,     8,    30,    31,    35,    36,    91,   161,
     162,    86,     9,    10,    11,    15,    37,    38,    39,    41,
      42,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    88,    80,    87,    86,    58,   120,   121,    87,
      34,    71,     6,    20,    65,    52,    95,   130,     6,   124,
     125,    30,    86,   108,    34,    59,    60,   131,   131,   131,
      87,   137,   138,   132,    75,    88,   133,   108,     6,    29,
      76,    85,   140,   146,     6,   140,     6,    80,   152,   153,
       6,   130,     6,     6,     6,     6,     6,   140,   155,    56,
      57,   156,     6,   146,   146,   146,    85,    41,     6,     8,
      57,   135,    89,    87,    87,    87,    87,    87,    87,    87,
      86,    91,   117,   140,   140,   140,   140,   140,   140,   140,
     140,    40,    43,    20,    20,   140,   140,   140,   140,   140,
     140,   140,   140,   140,   140,     6,     6,   128,    16,    18,
      58,   121,    58,   120,     6,     6,   140,    41,    66,     5,
      95,    65,    99,    86,    31,   115,    32,   109,     6,    83,
      83,    85,    85,     6,     8,    90,   160,    87,    88,   135,
     109,   140,    85,    85,    86,    89,    87,     6,    76,    86,
      91,    80,    85,    41,    41,    41,    71,    41,    55,    56,
      57,   140,    85,    85,    85,    88,   140,   137,   140,   140,
     140,   140,   140,   140,   140,   162,    43,   140,   140,    89,
      21,    21,   140,    65,    34,     6,    19,   111,   125,   112,
     113,   140,   140,    33,   110,   131,   131,    91,   161,    85,
       6,     8,   135,    76,    87,   136,   110,    85,   140,    80,
     140,   153,     6,   140,   140,   140,   140,   140,   140,   140,
      57,   140,    19,    84,    89,   160,   116,   116,    84,     5,
       6,   140,    86,    35,    36,   140,    85,    85,    91,    87,
     136,     3,    12,     6,     8,   139,   160,   111,     6,    85,
      19,    19,    19,    86,    19,    57,    89,   140,   140,    85,
     104,   106,   118,   119,   122,   123,   126,    34,    67,   113,
       6,     8,   139,   160,    12,     3,    84,   136,    84,   136,
      84,   136,    89,   140,   140,   140,     6,   140,    84,    89,
      89,   134,    85,   104,   118,   119,   122,   123,   126,     6,
       5,   136,   136,   136,    89,     3,     6,     8,    87,   160,
       6,     8,    87,   160,     6,     8,    87,   160,    75,    85,
      85,    85,    41,    85,   140,   133,    67,   160,   160,   160,
      75,     6,     8,    89,     6,     8,    89,     6,     8,    89,
      73,   140,    89,    19,    84,     5,    89,    89,    89,    75,
      75,    75,    84,   140,   140,    75,    75,    75,    73,    73,
      73,   140,    84,    89,    85,   140,    89
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    92,    93,    93,    94,    94,    94,    95,    96,    96,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    98,    99,    99,   100,   100,   101,   102,   103,   104,
     105,   105,   105,   105,   106,   106,   106,   106,   106,   106,
     106,   106,   106,   106,   106,   106,   107,   107,   108,   108,
     109,   109,   110,   110,   111,   111,   112,   112,   113,   113,
     113,   114,   114,   115,   115,   116,   116,   117,   117,   118,
     119,   119,   119,   119,   119,   120,   121,   122,   123,   124,
     124,   125,   126,   127,   127,   128,   128,   129,   129,   130,
     130,   131,   131,   132,   132,   132,   132,   132,   132,   133,
     134,   134,   134,   134,   134,   134,   134,   134,   134,   134,
     134,   134,   135,   135,   135,   135,   136,   136,   136,   136,
     136,   136,   137,   137,   138,   138,   138,   138,   139,   139,
     139,   139,   139,   139,   139,   139,   139,   139,   139,   139,
     140,   140,   140,   140,   140,   140,   140,   140,   140,   140,
     140,   140,   140,   140,   140,   140,   140,   140,   140,   140,
     140,   140,   140,   140,   140,   140,   141,   141,   141,   141,
     141,   141,   141,   141,   141,   141,   141,   141,   141,   141,
     141,   141,   141,   141,   142,   143,   143,   143,   143,   143,
     143,   143,   143,   143,   144,   144,   144,   144,   145,   146,
     146,   147,   147,   148,   148,   148,   148,   149,   149,   150,
     150,   151,   152,   152,   153,   153,   153,   153,   154,   154,
     154,   154,   155,   155,   156,   157,   157,   157,   157,   157,
     157,   158,   158,   158,   159,   160,   160,   160,   161,   161,
     162,   162,   162,   162,   162,   162,   162
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     2,     1,     3,     4,     1,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     5,     0,     2,     0,     1,     6,     7,     4,     8,
       6,     8,     8,    10,     1,     1,     1,     1,     1,     1,
       2,     2,     2,     2,     2,     2,     0,     1,     0,     3,
       0,     2,     0,     2,     0,     2,     1,     3,     1,     2,
       2,     1,     3,     1,     3,     1,     3,     3,     3,     2,
       2,     3,     3,     4,     4,     4,     4,     2,     3,     1,
       3,     1,     2,     1,     3,     3,     3,     1,     0,     1,
       3,     1,     3,     1,     3,     6,     4,     6,     4,     5,
       8,    10,    10,    10,     8,    10,    10,    10,     7,     9,
       9,     9,     0,     1,     1,     1,     0,     1,     2,     4,
       3,     3,     0,     1,     2,     2,     3,     3,     3,     4,
       3,     4,     3,     4,     3,     4,     3,     3,     4,     4,
       1,     2,     2,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     4,
       4,     3,     2,     3,     4,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     3,     3,
       3,     4,     6,     4,     1,     3,     4,     5,     4,     4,
       6,     4,     4,     4,     8,     8,     8,     8,    12,     1,
       3,     2,     3,     5,     7,     7,     9,    11,    13,     2,
       3,     4,     1,     3,     2,     2,     4,     3,     3,     5,
       4,     6,     1,     2,     4,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     0,     2,     3,     1,     3,
       3,     3,     3,     3,     3,     3,     3
};


/* YYDPREC[RULE-NUM] -- Dynamic precedence of rule #RULE-NUM (0 if none).  */
static const yytype_int8 yydprec[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     1,     2,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0
};

/* YYMERGER[RULE-NUM] -- Index of merging function for rule #RULE-NUM.  */
static const yytype_int8 yymerger[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0
};

/* YYIMMEDIATE[RULE-NUM] -- True iff rule #RULE-NUM is not to be deferred, as
   in the case of predicates.  */
static const yybool yyimmediate[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0
};

/* YYCONFLP[YYPACT[STATE-NUM]] -- Pointer into YYCONFL of start of
   list of conflicting reductions corresponding to action entry for
   state STATE-NUM in yytable.  0 means no conflicts.  The list in
   yyconfl is terminated by a rule number of 0.  */
static const yytype_int8 yyconflp[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     1,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     0,     7,     0,
       0,     9,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    11,     0,     0,     0,     0,     0,
      13,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     3,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0
};

/* YYCONFL[I] -- lists of conflicting rule numbers, each terminated by
   0, pointed into by YYCONFLP.  */
static const short yyconfl[] =
{
       0,   231,     0,   112,     0,   231,     0,   113,     0,   113,
       0,   232,     0,   233,     0
};


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

# define YYRHSLOC(Rhs, K) ((Rhs)[K].yystate.yyloc)



#undef yynerrs
#define yynerrs (yystackp->yyerrcnt)
#undef yychar
#define yychar (yystackp->yyrawchar)
#undef yylval
#define yylval (yystackp->yyval)
#undef yylloc
#define yylloc (yystackp->yyloc)
#define cypher_yynerrs yynerrs
#define cypher_yychar yychar
#define cypher_yylval yylval
#define cypher_yylloc yylloc

enum { YYENOMEM = -2 };

typedef enum { yyok, yyaccept, yyabort, yyerr, yynomem } YYRESULTTAG;

#define YYCHK(YYE)                              \
  do {                                          \
    YYRESULTTAG yychk_flag = YYE;               \
    if (yychk_flag != yyok)                     \
      return yychk_flag;                        \
  } while (0)

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYMAXDEPTH * sizeof (GLRStackItem)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

/* Minimum number of free items on the stack allowed after an
   allocation.  This is to allow allocation and initialization
   to be completed by functions that call yyexpandGLRStack before the
   stack is expanded, thus insuring that all necessary pointers get
   properly redirected to new data.  */
#define YYHEADROOM 2

#ifndef YYSTACKEXPANDABLE
#  define YYSTACKEXPANDABLE 1
#endif

#if YYSTACKEXPANDABLE
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyexpandGLRStack (Yystack);                       \
  } while (0)
#else
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyMemoryExhausted (Yystack);                      \
  } while (0)
#endif

/** State numbers. */
typedef int yy_state_t;

/** Rule numbers. */
typedef int yyRuleNum;

/** Item references. */
typedef short yyItemNum;

typedef struct yyGLRState yyGLRState;
typedef struct yyGLRStateSet yyGLRStateSet;
typedef struct yySemanticOption yySemanticOption;
typedef union yyGLRStackItem yyGLRStackItem;
typedef struct yyGLRStack yyGLRStack;

struct yyGLRState
{
  /** Type tag: always true.  */
  yybool yyisState;
  /** Type tag for yysemantics.  If true, yyval applies, otherwise
   *  yyfirstVal applies.  */
  yybool yyresolved;
  /** Number of corresponding LALR(1) machine state.  */
  yy_state_t yylrState;
  /** Preceding state in this stack */
  yyGLRState* yypred;
  /** Source position of the last token produced by my symbol */
  YYPTRDIFF_T yyposn;
  union {
    /** First in a chain of alternative reductions producing the
     *  nonterminal corresponding to this state, threaded through
     *  yynext.  */
    yySemanticOption* yyfirstVal;
    /** Semantic value for this state.  */
    YYSTYPE yyval;
  } yysemantics;
  /** Source location for this state.  */
  YYLTYPE yyloc;
};

struct yyGLRStateSet
{
  yyGLRState** yystates;
  /** During nondeterministic operation, yylookaheadNeeds tracks which
   *  stacks have actually needed the current lookahead.  During deterministic
   *  operation, yylookaheadNeeds[0] is not maintained since it would merely
   *  duplicate yychar != CYPHER_CYPHER_YYEMPTY.  */
  yybool* yylookaheadNeeds;
  YYPTRDIFF_T yysize;
  YYPTRDIFF_T yycapacity;
};

struct yySemanticOption
{
  /** Type tag: always false.  */
  yybool yyisState;
  /** Rule number for this reduction */
  yyRuleNum yyrule;
  /** The last RHS state in the list of states to be reduced.  */
  yyGLRState* yystate;
  /** The lookahead for this reduction.  */
  int yyrawchar;
  YYSTYPE yyval;
  YYLTYPE yyloc;
  /** Next sibling in chain of options.  To facilitate merging,
   *  options are chained in decreasing order by address.  */
  yySemanticOption* yynext;
};

/** Type of the items in the GLR stack.  The yyisState field
 *  indicates which item of the union is valid.  */
union yyGLRStackItem {
  yyGLRState yystate;
  yySemanticOption yyoption;
};

struct yyGLRStack {
  int yyerrState;
  /* To compute the location of the error token.  */
  yyGLRStackItem yyerror_range[3];

  int yyerrcnt;
  int yyrawchar;
  YYSTYPE yyval;
  YYLTYPE yyloc;

  YYJMP_BUF yyexception_buffer;
  yyGLRStackItem* yyitems;
  yyGLRStackItem* yynextFree;
  YYPTRDIFF_T yyspaceLeft;
  yyGLRState* yysplitPoint;
  yyGLRState* yylastDeleted;
  yyGLRStateSet yytops;
};

#if YYSTACKEXPANDABLE
static void yyexpandGLRStack (yyGLRStack* yystackp);
#endif

_Noreturn static void
yyFail (yyGLRStack* yystackp, YYLTYPE *yylocp, cypher_parser_context *context, const char* yymsg)
{
  if (yymsg != YY_NULLPTR)
    yyerror (yylocp, context, yymsg);
  YYLONGJMP (yystackp->yyexception_buffer, 1);
}

_Noreturn static void
yyMemoryExhausted (yyGLRStack* yystackp)
{
  YYLONGJMP (yystackp->yyexception_buffer, 2);
}

/** Accessing symbol of state YYSTATE.  */
static inline yysymbol_kind_t
yy_accessing_symbol (yy_state_t yystate)
{
  return YY_CAST (yysymbol_kind_t, yystos[yystate]);
}

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  static const char *const yy_sname[] =
  {
  "end of file", "error", "invalid token", "INTEGER", "DECIMAL", "STRING",
  "IDENTIFIER", "PARAMETER", "BQIDENT", "NOT_EQ", "LT_EQ", "GT_EQ",
  "DOT_DOT", "TYPECAST", "PLUS_EQ", "REGEX_MATCH", "MATCH", "RETURN",
  "CREATE", "WHERE", "WITH", "SET", "DELETE", "REMOVE", "MERGE", "UNWIND",
  "DETACH", "FOREACH", "OPTIONAL", "DISTINCT", "ORDER", "BY", "SKIP",
  "LIMIT", "AS", "ASC", "DESC", "AND", "OR", "XOR", "NOT", "IN", "IS",
  "NULL_P", "TRUE_P", "FALSE_P", "EXISTS", "ANY", "NONE", "SINGLE",
  "REDUCE", "UNION", "ALL", "CASE", "WHEN", "THEN", "ELSE", "END_P", "ON",
  "SHORTESTPATH", "ALLSHORTESTPATHS", "PATTERN", "EXPLAIN", "LOAD", "CSV",
  "FROM", "HEADERS", "FIELDTERMINATOR", "STARTS", "ENDS", "CONTAINS",
  "'='", "'<'", "'>'", "'+'", "'-'", "'*'", "'/'", "'%'", "'^'", "'.'",
  "UNARY_MINUS", "UNARY_PLUS", "'('", "'|'", "')'", "','", "':'", "'['",
  "']'", "'{'", "'}'", "$accept", "stmt", "union_query", "single_query",
  "clause_list", "clause", "match_clause", "from_graph_opt",
  "optional_opt", "return_clause", "with_clause", "unwind_clause",
  "foreach_clause", "load_csv_clause", "foreach_update_list",
  "distinct_opt", "order_by_opt", "skip_opt", "limit_opt", "where_opt",
  "order_by_list", "order_by_item", "return_item_list", "return_item",
  "set_item_list", "set_item", "create_clause", "merge_clause",
  "on_create_clause", "on_match_clause", "set_clause", "delete_clause",
  "delete_item_list", "delete_item", "remove_clause", "remove_item_list",
  "remove_item", "detach_opt", "pattern_list", "simple_path", "path",
  "node_pattern", "rel_pattern", "variable_opt", "varlen_range_opt",
  "label_opt", "label_list", "rel_type_list", "expr", "primary_expr",
  "literal_expr", "function_call", "list_predicate", "reduce_expr",
  "argument_list", "list_literal", "list_comprehension",
  "pattern_comprehension", "map_literal", "map_projection",
  "map_projection_list", "map_projection_item", "case_expression",
  "when_clause_list", "when_clause", "literal", "identifier", "parameter",
  "properties_opt", "map_pair_list", "map_pair", YY_NULLPTR
  };
  return yy_sname[yysymbol];
}
#endif

/** Left-hand-side symbol for rule #YYRULE.  */
static inline yysymbol_kind_t
yylhsNonterm (yyRuleNum yyrule)
{
  return YY_CAST (yysymbol_kind_t, yyr1[yyrule]);
}

#if CYPHER_YYDEBUG

# ifndef YYFPRINTF
#  define YYFPRINTF fprintf
# endif

# define YY_FPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_FPRINTF_

# define YY_FPRINTF_(Args)                      \
  do {                                          \
    YYFPRINTF Args;                             \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)

# define YY_DPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_DPRINTF_

# define YY_DPRINTF_(Args)                      \
  do {                                          \
    if (yydebug)                                \
      YYFPRINTF Args;                           \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined CYPHER_YYLTYPE_IS_TRIVIAL && CYPHER_YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */



/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, cypher_parser_context *context)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (context);
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
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, cypher_parser_context *context)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, context);
  YYFPRINTF (yyo, ")");
}

# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                  \
  do {                                                                  \
    if (yydebug)                                                        \
      {                                                                 \
        YY_FPRINTF ((stderr, "%s ", Title));                            \
        yy_symbol_print (stderr, Kind, Value, Location, context);        \
        YY_FPRINTF ((stderr, "\n"));                                    \
      }                                                                 \
  } while (0)

static inline void
yy_reduce_print (yybool yynormal, yyGLRStackItem* yyvsp, YYPTRDIFF_T yyk,
                 yyRuleNum yyrule, cypher_parser_context *context);

# define YY_REDUCE_PRINT(Args)          \
  do {                                  \
    if (yydebug)                        \
      yy_reduce_print Args;             \
  } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;

static void yypstack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
  YY_ATTRIBUTE_UNUSED;
static void yypdumpstack (yyGLRStack* yystackp)
  YY_ATTRIBUTE_UNUSED;

#else /* !CYPHER_YYDEBUG */

# define YY_DPRINTF(Args) do {} while (yyfalse)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_REDUCE_PRINT(Args)

#endif /* !CYPHER_YYDEBUG */

#ifndef yystrlen
# define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
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



/** Fill in YYVSP[YYLOW1 .. YYLOW0-1] from the chain of states starting
 *  at YYVSP[YYLOW0].yystate.yypred.  Leaves YYVSP[YYLOW1].yystate.yypred
 *  containing the pointer to the next state in the chain.  */
static void yyfillin (yyGLRStackItem *, int, int) YY_ATTRIBUTE_UNUSED;
static void
yyfillin (yyGLRStackItem *yyvsp, int yylow0, int yylow1)
{
  int i;
  yyGLRState *s = yyvsp[yylow0].yystate.yypred;
  for (i = yylow0-1; i >= yylow1; i -= 1)
    {
#if CYPHER_YYDEBUG
      yyvsp[i].yystate.yylrState = s->yylrState;
#endif
      yyvsp[i].yystate.yyresolved = s->yyresolved;
      if (s->yyresolved)
        yyvsp[i].yystate.yysemantics.yyval = s->yysemantics.yyval;
      else
        /* The effect of using yyval or yyloc (in an immediate rule) is
         * undefined.  */
        yyvsp[i].yystate.yysemantics.yyfirstVal = YY_NULLPTR;
      yyvsp[i].yystate.yyloc = s->yyloc;
      s = yyvsp[i].yystate.yypred = s->yypred;
    }
}


/** If yychar is empty, fetch the next token.  */
static inline yysymbol_kind_t
yygetToken (int *yycharp, yyGLRStack* yystackp, cypher_parser_context *context)
{
  yysymbol_kind_t yytoken;
  YY_USE (context);
  if (*yycharp == CYPHER_CYPHER_YYEMPTY)
    {
      YY_DPRINTF ((stderr, "Reading a token\n"));
      *yycharp = yylex (&yylval, &yylloc, context);
    }
  if (*yycharp <= CYPHER_YYEOF)
    {
      *yycharp = CYPHER_YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YY_DPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (*yycharp);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }
  return yytoken;
}

/* Do nothing if YYNORMAL or if *YYLOW <= YYLOW1.  Otherwise, fill in
 * YYVSP[YYLOW1 .. *YYLOW-1] as in yyfillin and set *YYLOW = YYLOW1.
 * For convenience, always return YYLOW1.  */
static inline int yyfill (yyGLRStackItem *, int *, int, yybool)
     YY_ATTRIBUTE_UNUSED;
static inline int
yyfill (yyGLRStackItem *yyvsp, int *yylow, int yylow1, yybool yynormal)
{
  if (!yynormal && yylow1 < *yylow)
    {
      yyfillin (yyvsp, *yylow, yylow1);
      *yylow = yylow1;
    }
  return yylow1;
}

/** Perform user action for rule number YYN, with RHS length YYRHSLEN,
 *  and top stack item YYVSP.  YYLVALP points to place to put semantic
 *  value ($$), and yylocp points to place for location information
 *  (@$).  Returns yyok for normal return, yyaccept for YYACCEPT,
 *  yyerr for YYERROR, yyabort for YYABORT, yynomem for YYNOMEM.  */
static YYRESULTTAG
yyuserAction (yyRuleNum yyrule, int yyrhslen, yyGLRStackItem* yyvsp,
              yyGLRStack* yystackp, YYPTRDIFF_T yyk,
              YYSTYPE* yyvalp, YYLTYPE *yylocp, cypher_parser_context *context)
{
  const yybool yynormal YY_ATTRIBUTE_UNUSED = yystackp->yysplitPoint == YY_NULLPTR;
  int yylow = 1;
  YY_USE (yyvalp);
  YY_USE (yylocp);
  YY_USE (context);
  YY_USE (yyk);
  YY_USE (yyrhslen);
# undef yyerrok
# define yyerrok (yystackp->yyerrState = 0)
# undef YYACCEPT
# define YYACCEPT return yyaccept
# undef YYABORT
# define YYABORT return yyabort
# undef YYNOMEM
# define YYNOMEM return yynomem
# undef YYERROR
# define YYERROR return yyerrok, yyerr
# undef YYRECOVERING
# define YYRECOVERING() (yystackp->yyerrState != 0)
# undef yyclearin
# define yyclearin (yychar = CYPHER_CYPHER_YYEMPTY)
# undef YYFILL
# define YYFILL(N) yyfill (yyvsp, &yylow, (N), yynormal)
# undef YYBACKUP
# define YYBACKUP(Token, Value)                                              \
  return yyerror (yylocp, context, YY_("syntax error: cannot back up")),     \
         yyerrok, yyerr

  if (yyrhslen == 0)
    *yyvalp = yyval_default;
  else
    *yyvalp = yyvsp[YYFILL (1-yyrhslen)].yystate.yysemantics.yyval;
  /* Default location. */
  YYLLOC_DEFAULT ((*yylocp), (yyvsp - yyrhslen), yyrhslen);
  yystackp->yyerror_range[1].yystate.yyloc = *yylocp;
  /* If yyk == -1, we are running a deferred action on a temporary
     stack.  In that case, YY_REDUCE_PRINT must not play with YYFILL,
     so pretend the stack is "normal". */
  YY_REDUCE_PRINT ((yynormal || yyk == -1, yyvsp, yyk, yyrule, context));
  switch (yyrule)
    {
  case 2: /* stmt: union_query  */
#line 161 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            context->result = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 2229 "build/parser/cypher_gram.tab.c"
    break;

  case 3: /* stmt: EXPLAIN union_query  */
#line 166 "src/backend/parser/cypher_gram.y"
        {
            /* For EXPLAIN with UNION, wrap if needed */
            if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)->type == AST_NODE_QUERY) {
                ((cypher_query*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node))->explain = true;
            }
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            context->result = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 2242 "build/parser/cypher_gram.tab.c"
    break;

  case 4: /* union_query: single_query  */
#line 178 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
#line 2250 "build/parser/cypher_gram.tab.c"
    break;

  case 5: /* union_query: union_query UNION single_query  */
#line 182 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_union((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
        }
#line 2258 "build/parser/cypher_gram.tab.c"
    break;

  case 6: /* union_query: union_query UNION ALL single_query  */
#line 186 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_union((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), true, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 2266 "build/parser/cypher_gram.tab.c"
    break;

  case 7: /* single_query: clause_list  */
#line 193 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_query((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), false);
        }
#line 2274 "build/parser/cypher_gram.tab.c"
    break;

  case 8: /* clause_list: clause  */
#line 200 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 2283 "build/parser/cypher_gram.tab.c"
    break;

  case 9: /* clause_list: clause_list clause  */
#line 205 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2292 "build/parser/cypher_gram.tab.c"
    break;

  case 10: /* clause: match_clause  */
#line 212 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.match); }
#line 2298 "build/parser/cypher_gram.tab.c"
    break;

  case 11: /* clause: return_clause  */
#line 213 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.return_clause); }
#line 2304 "build/parser/cypher_gram.tab.c"
    break;

  case 12: /* clause: with_clause  */
#line 214 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.with_clause); }
#line 2310 "build/parser/cypher_gram.tab.c"
    break;

  case 13: /* clause: unwind_clause  */
#line 215 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2316 "build/parser/cypher_gram.tab.c"
    break;

  case 14: /* clause: foreach_clause  */
#line 216 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2322 "build/parser/cypher_gram.tab.c"
    break;

  case 15: /* clause: load_csv_clause  */
#line 217 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2328 "build/parser/cypher_gram.tab.c"
    break;

  case 16: /* clause: create_clause  */
#line 218 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.create); }
#line 2334 "build/parser/cypher_gram.tab.c"
    break;

  case 17: /* clause: merge_clause  */
#line 219 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.merge); }
#line 2340 "build/parser/cypher_gram.tab.c"
    break;

  case 18: /* clause: set_clause  */
#line 220 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.set); }
#line 2346 "build/parser/cypher_gram.tab.c"
    break;

  case 19: /* clause: delete_clause  */
#line 221 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.delete); }
#line 2352 "build/parser/cypher_gram.tab.c"
    break;

  case 20: /* clause: remove_clause  */
#line 222 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.remove); }
#line 2358 "build/parser/cypher_gram.tab.c"
    break;

  case 21: /* match_clause: optional_opt MATCH pattern_list from_graph_opt where_opt  */
#line 228 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).match) = make_cypher_match((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.boolean), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.string));
        }
#line 2366 "build/parser/cypher_gram.tab.c"
    break;

  case 22: /* from_graph_opt: %empty  */
#line 234 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).string) = NULL; }
#line 2372 "build/parser/cypher_gram.tab.c"
    break;

  case 23: /* from_graph_opt: FROM IDENTIFIER  */
#line 236 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).string) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string);
        }
#line 2380 "build/parser/cypher_gram.tab.c"
    break;

  case 24: /* optional_opt: %empty  */
#line 242 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).boolean) = false; }
#line 2386 "build/parser/cypher_gram.tab.c"
    break;

  case 25: /* optional_opt: OPTIONAL  */
#line 243 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).boolean) = true; }
#line 2392 "build/parser/cypher_gram.tab.c"
    break;

  case 26: /* return_clause: RETURN distinct_opt return_item_list order_by_opt skip_opt limit_opt  */
#line 249 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).return_clause) = make_cypher_return((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.boolean), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 2400 "build/parser/cypher_gram.tab.c"
    break;

  case 27: /* with_clause: WITH distinct_opt return_item_list order_by_opt skip_opt limit_opt where_opt  */
#line 257 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).with_clause) = make_cypher_with((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.boolean), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 2408 "build/parser/cypher_gram.tab.c"
    break;

  case 28: /* unwind_clause: UNWIND expr AS IDENTIFIER  */
#line 265 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_unwind((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2417 "build/parser/cypher_gram.tab.c"
    break;

  case 29: /* foreach_clause: FOREACH '(' IDENTIFIER IN expr '|' foreach_update_list ')'  */
#line 274 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_foreach((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 2426 "build/parser/cypher_gram.tab.c"
    break;

  case 30: /* load_csv_clause: LOAD CSV FROM STRING AS IDENTIFIER  */
#line 283 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_load_csv((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), false, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2436 "build/parser/cypher_gram.tab.c"
    break;

  case 31: /* load_csv_clause: LOAD CSV WITH HEADERS FROM STRING AS IDENTIFIER  */
#line 289 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_load_csv((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), true, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2446 "build/parser/cypher_gram.tab.c"
    break;

  case 32: /* load_csv_clause: LOAD CSV FROM STRING AS IDENTIFIER FIELDTERMINATOR STRING  */
#line 295 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_load_csv((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2457 "build/parser/cypher_gram.tab.c"
    break;

  case 33: /* load_csv_clause: LOAD CSV WITH HEADERS FROM STRING AS IDENTIFIER FIELDTERMINATOR STRING  */
#line 302 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_cypher_load_csv((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), true, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2468 "build/parser/cypher_gram.tab.c"
    break;

  case 34: /* foreach_update_list: create_clause  */
#line 313 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.create));
        }
#line 2477 "build/parser/cypher_gram.tab.c"
    break;

  case 35: /* foreach_update_list: set_clause  */
#line 318 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.set));
        }
#line 2486 "build/parser/cypher_gram.tab.c"
    break;

  case 36: /* foreach_update_list: delete_clause  */
#line 323 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.delete));
        }
#line 2495 "build/parser/cypher_gram.tab.c"
    break;

  case 37: /* foreach_update_list: merge_clause  */
#line 328 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.merge));
        }
#line 2504 "build/parser/cypher_gram.tab.c"
    break;

  case 38: /* foreach_update_list: remove_clause  */
#line 333 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.remove));
        }
#line 2513 "build/parser/cypher_gram.tab.c"
    break;

  case 39: /* foreach_update_list: foreach_clause  */
#line 338 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 2522 "build/parser/cypher_gram.tab.c"
    break;

  case 40: /* foreach_update_list: foreach_update_list create_clause  */
#line 343 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.create));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2531 "build/parser/cypher_gram.tab.c"
    break;

  case 41: /* foreach_update_list: foreach_update_list set_clause  */
#line 348 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.set));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2540 "build/parser/cypher_gram.tab.c"
    break;

  case 42: /* foreach_update_list: foreach_update_list delete_clause  */
#line 353 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.delete));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2549 "build/parser/cypher_gram.tab.c"
    break;

  case 43: /* foreach_update_list: foreach_update_list merge_clause  */
#line 358 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.merge));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2558 "build/parser/cypher_gram.tab.c"
    break;

  case 44: /* foreach_update_list: foreach_update_list remove_clause  */
#line 363 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.remove));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2567 "build/parser/cypher_gram.tab.c"
    break;

  case 45: /* foreach_update_list: foreach_update_list foreach_clause  */
#line 368 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 2576 "build/parser/cypher_gram.tab.c"
    break;

  case 46: /* distinct_opt: %empty  */
#line 375 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).boolean) = false; }
#line 2582 "build/parser/cypher_gram.tab.c"
    break;

  case 47: /* distinct_opt: DISTINCT  */
#line 376 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).boolean) = true; }
#line 2588 "build/parser/cypher_gram.tab.c"
    break;

  case 48: /* order_by_opt: %empty  */
#line 380 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).list) = NULL; }
#line 2594 "build/parser/cypher_gram.tab.c"
    break;

  case 49: /* order_by_opt: ORDER BY order_by_list  */
#line 381 "src/backend/parser/cypher_gram.y"
                             { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 2600 "build/parser/cypher_gram.tab.c"
    break;

  case 50: /* skip_opt: %empty  */
#line 385 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).node) = NULL; }
#line 2606 "build/parser/cypher_gram.tab.c"
    break;

  case 51: /* skip_opt: SKIP expr  */
#line 386 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2612 "build/parser/cypher_gram.tab.c"
    break;

  case 52: /* limit_opt: %empty  */
#line 390 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).node) = NULL; }
#line 2618 "build/parser/cypher_gram.tab.c"
    break;

  case 53: /* limit_opt: LIMIT expr  */
#line 391 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2624 "build/parser/cypher_gram.tab.c"
    break;

  case 54: /* where_opt: %empty  */
#line 395 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).node) = NULL; }
#line 2630 "build/parser/cypher_gram.tab.c"
    break;

  case 55: /* where_opt: WHERE expr  */
#line 396 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 2636 "build/parser/cypher_gram.tab.c"
    break;

  case 56: /* order_by_list: order_by_item  */
#line 401 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.order_by_item));
        }
#line 2645 "build/parser/cypher_gram.tab.c"
    break;

  case 57: /* order_by_list: order_by_list ',' order_by_item  */
#line 406 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.order_by_item));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 2654 "build/parser/cypher_gram.tab.c"
    break;

  case 58: /* order_by_item: expr  */
#line 413 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).order_by_item) = make_order_by_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), false); /* Default is ASC */ }
#line 2660 "build/parser/cypher_gram.tab.c"
    break;

  case 59: /* order_by_item: expr ASC  */
#line 414 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).order_by_item) = make_order_by_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), false); }
#line 2666 "build/parser/cypher_gram.tab.c"
    break;

  case 60: /* order_by_item: expr DESC  */
#line 415 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).order_by_item) = make_order_by_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), true); }
#line 2672 "build/parser/cypher_gram.tab.c"
    break;

  case 61: /* return_item_list: return_item  */
#line 421 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.return_item));
        }
#line 2681 "build/parser/cypher_gram.tab.c"
    break;

  case 62: /* return_item_list: return_item_list ',' return_item  */
#line 426 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.return_item));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 2690 "build/parser/cypher_gram.tab.c"
    break;

  case 63: /* return_item: expr  */
#line 434 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).return_item) = make_return_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), NULL);
        }
#line 2698 "build/parser/cypher_gram.tab.c"
    break;

  case 64: /* return_item: expr AS IDENTIFIER  */
#line 438 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).return_item) = make_return_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2707 "build/parser/cypher_gram.tab.c"
    break;

  case 65: /* set_item_list: set_item  */
#line 446 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.set_item));
        }
#line 2716 "build/parser/cypher_gram.tab.c"
    break;

  case 66: /* set_item_list: set_item_list ',' set_item  */
#line 451 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.set_item));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 2725 "build/parser/cypher_gram.tab.c"
    break;

  case 67: /* set_item: expr '=' expr  */
#line 459 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).set_item) = make_cypher_set_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 2733 "build/parser/cypher_gram.tab.c"
    break;

  case 68: /* set_item: IDENTIFIER ':' IDENTIFIER  */
#line 463 "src/backend/parser/cypher_gram.y"
        {
            /* SET n:Label syntax */
            cypher_identifier *var = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            cypher_label_expr *label = make_label_expr((ast_node*)var, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).set_item) = make_cypher_set_item((ast_node*)label, NULL);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2746 "build/parser/cypher_gram.tab.c"
    break;

  case 69: /* create_clause: CREATE pattern_list  */
#line 476 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).create) = make_cypher_create((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list));
        }
#line 2754 "build/parser/cypher_gram.tab.c"
    break;

  case 70: /* merge_clause: MERGE pattern_list  */
#line 484 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).merge) = make_cypher_merge((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), NULL, NULL);
        }
#line 2762 "build/parser/cypher_gram.tab.c"
    break;

  case 71: /* merge_clause: MERGE pattern_list on_create_clause  */
#line 488 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).merge) = make_cypher_merge((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), NULL);
        }
#line 2770 "build/parser/cypher_gram.tab.c"
    break;

  case 72: /* merge_clause: MERGE pattern_list on_match_clause  */
#line 492 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).merge) = make_cypher_merge((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list));
        }
#line 2778 "build/parser/cypher_gram.tab.c"
    break;

  case 73: /* merge_clause: MERGE pattern_list on_create_clause on_match_clause  */
#line 496 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).merge) = make_cypher_merge((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list));
        }
#line 2786 "build/parser/cypher_gram.tab.c"
    break;

  case 74: /* merge_clause: MERGE pattern_list on_match_clause on_create_clause  */
#line 500 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).merge) = make_cypher_merge((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list));
        }
#line 2794 "build/parser/cypher_gram.tab.c"
    break;

  case 75: /* on_create_clause: ON CREATE SET set_item_list  */
#line 507 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list);
        }
#line 2802 "build/parser/cypher_gram.tab.c"
    break;

  case 76: /* on_match_clause: ON MATCH SET set_item_list  */
#line 514 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list);
        }
#line 2810 "build/parser/cypher_gram.tab.c"
    break;

  case 77: /* set_clause: SET set_item_list  */
#line 522 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).set) = make_cypher_set((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list));
        }
#line 2818 "build/parser/cypher_gram.tab.c"
    break;

  case 78: /* delete_clause: detach_opt DELETE delete_item_list  */
#line 530 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).delete) = make_cypher_delete((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.boolean));
        }
#line 2826 "build/parser/cypher_gram.tab.c"
    break;

  case 79: /* delete_item_list: delete_item  */
#line 537 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.delete_item));
        }
#line 2835 "build/parser/cypher_gram.tab.c"
    break;

  case 80: /* delete_item_list: delete_item_list ',' delete_item  */
#line 542 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.delete_item));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 2844 "build/parser/cypher_gram.tab.c"
    break;

  case 81: /* delete_item: IDENTIFIER  */
#line 550 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).delete_item) = make_delete_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2853 "build/parser/cypher_gram.tab.c"
    break;

  case 82: /* remove_clause: REMOVE remove_item_list  */
#line 559 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).remove) = make_cypher_remove((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list));
        }
#line 2861 "build/parser/cypher_gram.tab.c"
    break;

  case 83: /* remove_item_list: remove_item  */
#line 566 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.remove_item));
        }
#line 2870 "build/parser/cypher_gram.tab.c"
    break;

  case 84: /* remove_item_list: remove_item_list ',' remove_item  */
#line 571 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.remove_item));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 2879 "build/parser/cypher_gram.tab.c"
    break;

  case 85: /* remove_item: IDENTIFIER '.' IDENTIFIER  */
#line 579 "src/backend/parser/cypher_gram.y"
        {
            /* REMOVE n.property - property access */
            cypher_identifier *base = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ast_node *prop = (ast_node*)make_property((ast_node*)base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).remove_item) = make_remove_item(prop);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2892 "build/parser/cypher_gram.tab.c"
    break;

  case 86: /* remove_item: IDENTIFIER ':' IDENTIFIER  */
#line 588 "src/backend/parser/cypher_gram.y"
        {
            /* REMOVE n:Label syntax */
            cypher_identifier *var = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            cypher_label_expr *label = make_label_expr((ast_node*)var, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ((*yyvalp).remove_item) = make_remove_item((ast_node*)label);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 2905 "build/parser/cypher_gram.tab.c"
    break;

  case 87: /* detach_opt: DETACH  */
#line 600 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).boolean) = true;
        }
#line 2913 "build/parser/cypher_gram.tab.c"
    break;

  case 88: /* detach_opt: %empty  */
#line 604 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).boolean) = false;
        }
#line 2921 "build/parser/cypher_gram.tab.c"
    break;

  case 89: /* pattern_list: path  */
#line 612 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.path));
        }
#line 2930 "build/parser/cypher_gram.tab.c"
    break;

  case 90: /* pattern_list: pattern_list ',' path  */
#line 617 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.path));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 2939 "build/parser/cypher_gram.tab.c"
    break;

  case 91: /* simple_path: node_pattern  */
#line 626 "src/backend/parser/cypher_gram.y"
        {
            ast_list *elements = ast_list_create();
            ast_list_append(elements, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node_pattern));
            ((*yyvalp).path) = make_path(elements);
        }
#line 2949 "build/parser/cypher_gram.tab.c"
    break;

  case 92: /* simple_path: simple_path rel_pattern node_pattern  */
#line 632 "src/backend/parser/cypher_gram.y"
        {
            /* Create a new list copying elements from the existing path */
            ast_list *new_elements = ast_list_create();
            for (int i = 0; i < (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.path)->elements->count; i++) {
                ast_list_append(new_elements, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.path)->elements->items[i]);
            }
            ast_list_append(new_elements, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.rel_pattern));
            ast_list_append(new_elements, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node_pattern));

            /* Note: Let Bison handle memory cleanup of $1 automatically */
            /* Manual freeing during parsing can cause parser state corruption */

            ((*yyvalp).path) = make_path(new_elements);
        }
#line 2968 "build/parser/cypher_gram.tab.c"
    break;

  case 93: /* path: simple_path  */
#line 651 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).path) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.path);
        }
#line 2976 "build/parser/cypher_gram.tab.c"
    break;

  case 94: /* path: IDENTIFIER '=' simple_path  */
#line 655 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).path) = make_path_with_var((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.path)->elements);
            /* Free the anonymous path structure, but keep its elements */
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.path));
        }
#line 2986 "build/parser/cypher_gram.tab.c"
    break;

  case 95: /* path: IDENTIFIER '=' SHORTESTPATH '(' simple_path ')'  */
#line 661 "src/backend/parser/cypher_gram.y"
        {
            cypher_path *sp = make_shortest_path((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.path)->elements, PATH_TYPE_SHORTEST);
            sp->var_name = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string);
            ((*yyvalp).path) = sp;
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.path));
        }
#line 2997 "build/parser/cypher_gram.tab.c"
    break;

  case 96: /* path: SHORTESTPATH '(' simple_path ')'  */
#line 668 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).path) = make_shortest_path((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.path)->elements, PATH_TYPE_SHORTEST);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.path));
        }
#line 3006 "build/parser/cypher_gram.tab.c"
    break;

  case 97: /* path: IDENTIFIER '=' ALLSHORTESTPATHS '(' simple_path ')'  */
#line 673 "src/backend/parser/cypher_gram.y"
        {
            cypher_path *sp = make_shortest_path((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.path)->elements, PATH_TYPE_ALL_SHORTEST);
            sp->var_name = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string);
            ((*yyvalp).path) = sp;
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.path));
        }
#line 3017 "build/parser/cypher_gram.tab.c"
    break;

  case 98: /* path: ALLSHORTESTPATHS '(' simple_path ')'  */
#line 680 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).path) = make_shortest_path((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.path)->elements, PATH_TYPE_ALL_SHORTEST);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.path));
        }
#line 3026 "build/parser/cypher_gram.tab.c"
    break;

  case 99: /* node_pattern: '(' variable_opt label_opt properties_opt ')'  */
#line 688 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node_pattern) = make_node_pattern((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.map));
        }
#line 3034 "build/parser/cypher_gram.tab.c"
    break;

  case 100: /* rel_pattern: '-' '[' variable_opt varlen_range_opt properties_opt ']' '-' '>'  */
#line 703 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), NULL, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.map), false, true, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.varlen_range));
        }
#line 3042 "build/parser/cypher_gram.tab.c"
    break;

  case 101: /* rel_pattern: '-' '[' variable_opt ':' IDENTIFIER varlen_range_opt properties_opt ']' '-' '>'  */
#line 707 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.map), false, true, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.varlen_range));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3051 "build/parser/cypher_gram.tab.c"
    break;

  case 102: /* rel_pattern: '-' '[' variable_opt ':' BQIDENT varlen_range_opt properties_opt ']' '-' '>'  */
#line 712 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.map), false, true, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.varlen_range));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3060 "build/parser/cypher_gram.tab.c"
    break;

  case 103: /* rel_pattern: '-' '[' variable_opt ':' rel_type_list varlen_range_opt properties_opt ']' '-' '>'  */
#line 717 "src/backend/parser/cypher_gram.y"
        {
            cypher_rel_pattern *p = make_rel_pattern_multi_type((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.map), false, true);
            if (p) p->varlen = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.varlen_range);
            ((*yyvalp).rel_pattern) = p;
        }
#line 3070 "build/parser/cypher_gram.tab.c"
    break;

  case 104: /* rel_pattern: '<' '-' '[' variable_opt varlen_range_opt properties_opt ']' '-'  */
#line 724 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), NULL, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.map), true, false, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.varlen_range));
        }
#line 3078 "build/parser/cypher_gram.tab.c"
    break;

  case 105: /* rel_pattern: '<' '-' '[' variable_opt ':' IDENTIFIER varlen_range_opt properties_opt ']' '-'  */
#line 728 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.map), true, false, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.varlen_range));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string));
        }
#line 3087 "build/parser/cypher_gram.tab.c"
    break;

  case 106: /* rel_pattern: '<' '-' '[' variable_opt ':' BQIDENT varlen_range_opt properties_opt ']' '-'  */
#line 733 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.map), true, false, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.varlen_range));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string));
        }
#line 3096 "build/parser/cypher_gram.tab.c"
    break;

  case 107: /* rel_pattern: '<' '-' '[' variable_opt ':' rel_type_list varlen_range_opt properties_opt ']' '-'  */
#line 738 "src/backend/parser/cypher_gram.y"
        {
            cypher_rel_pattern *p = make_rel_pattern_multi_type((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.map), true, false);
            if (p) p->varlen = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.varlen_range);
            ((*yyvalp).rel_pattern) = p;
        }
#line 3106 "build/parser/cypher_gram.tab.c"
    break;

  case 108: /* rel_pattern: '-' '[' variable_opt varlen_range_opt properties_opt ']' '-'  */
#line 745 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), NULL, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.map), false, false, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.varlen_range));
        }
#line 3114 "build/parser/cypher_gram.tab.c"
    break;

  case 109: /* rel_pattern: '-' '[' variable_opt ':' IDENTIFIER varlen_range_opt properties_opt ']' '-'  */
#line 749 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.map), false, false, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.varlen_range));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string));
        }
#line 3123 "build/parser/cypher_gram.tab.c"
    break;

  case 110: /* rel_pattern: '-' '[' variable_opt ':' BQIDENT varlen_range_opt properties_opt ']' '-'  */
#line 754 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).rel_pattern) = make_rel_pattern_varlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.map), false, false, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.varlen_range));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string));
        }
#line 3132 "build/parser/cypher_gram.tab.c"
    break;

  case 111: /* rel_pattern: '-' '[' variable_opt ':' rel_type_list varlen_range_opt properties_opt ']' '-'  */
#line 759 "src/backend/parser/cypher_gram.y"
        {
            cypher_rel_pattern *p = make_rel_pattern_multi_type((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.map), false, false);
            if (p) p->varlen = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.varlen_range);
            ((*yyvalp).rel_pattern) = p;
        }
#line 3142 "build/parser/cypher_gram.tab.c"
    break;

  case 112: /* variable_opt: %empty  */
#line 767 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).string) = NULL; }
#line 3148 "build/parser/cypher_gram.tab.c"
    break;

  case 113: /* variable_opt: IDENTIFIER  */
#line 768 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).string) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string); }
#line 3154 "build/parser/cypher_gram.tab.c"
    break;

  case 114: /* variable_opt: BQIDENT  */
#line 769 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).string) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string); }
#line 3160 "build/parser/cypher_gram.tab.c"
    break;

  case 115: /* variable_opt: END_P  */
#line 770 "src/backend/parser/cypher_gram.y"
                    { ((*yyvalp).string) = strdup("end"); }
#line 3166 "build/parser/cypher_gram.tab.c"
    break;

  case 116: /* varlen_range_opt: %empty  */
#line 776 "src/backend/parser/cypher_gram.y"
        { ((*yyvalp).varlen_range) = NULL; }
#line 3172 "build/parser/cypher_gram.tab.c"
    break;

  case 117: /* varlen_range_opt: '*'  */
#line 778 "src/backend/parser/cypher_gram.y"
        { ((*yyvalp).varlen_range) = make_varlen_range(1, -1); }
#line 3178 "build/parser/cypher_gram.tab.c"
    break;

  case 118: /* varlen_range_opt: '*' INTEGER  */
#line 780 "src/backend/parser/cypher_gram.y"
        { ((*yyvalp).varlen_range) = make_varlen_range((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.integer), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.integer)); }
#line 3184 "build/parser/cypher_gram.tab.c"
    break;

  case 119: /* varlen_range_opt: '*' INTEGER DOT_DOT INTEGER  */
#line 782 "src/backend/parser/cypher_gram.y"
        { ((*yyvalp).varlen_range) = make_varlen_range((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.integer), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.integer)); }
#line 3190 "build/parser/cypher_gram.tab.c"
    break;

  case 120: /* varlen_range_opt: '*' INTEGER DOT_DOT  */
#line 784 "src/backend/parser/cypher_gram.y"
        { ((*yyvalp).varlen_range) = make_varlen_range((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.integer), -1); }
#line 3196 "build/parser/cypher_gram.tab.c"
    break;

  case 121: /* varlen_range_opt: '*' DOT_DOT INTEGER  */
#line 786 "src/backend/parser/cypher_gram.y"
        { ((*yyvalp).varlen_range) = make_varlen_range(1, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.integer)); }
#line 3202 "build/parser/cypher_gram.tab.c"
    break;

  case 122: /* label_opt: %empty  */
#line 790 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).list) = NULL; }
#line 3208 "build/parser/cypher_gram.tab.c"
    break;

  case 123: /* label_opt: label_list  */
#line 791 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.list); }
#line 3214 "build/parser/cypher_gram.tab.c"
    break;

  case 124: /* label_list: ':' IDENTIFIER  */
#line 797 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *label = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)label);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3225 "build/parser/cypher_gram.tab.c"
    break;

  case 125: /* label_list: ':' BQIDENT  */
#line 804 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *label = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)label);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3236 "build/parser/cypher_gram.tab.c"
    break;

  case 126: /* label_list: label_list ':' IDENTIFIER  */
#line 811 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            cypher_literal *label = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)label);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3247 "build/parser/cypher_gram.tab.c"
    break;

  case 127: /* label_list: label_list ':' BQIDENT  */
#line 818 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            cypher_literal *label = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)label);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3258 "build/parser/cypher_gram.tab.c"
    break;

  case 128: /* rel_type_list: IDENTIFIER '|' IDENTIFIER  */
#line 828 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            cypher_literal *type_lit3 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit1);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit3);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3272 "build/parser/cypher_gram.tab.c"
    break;

  case 129: /* rel_type_list: IDENTIFIER '|' ':' IDENTIFIER  */
#line 838 "src/backend/parser/cypher_gram.y"
        {
            /* Support [:TYPE1|:TYPE2] syntax with colon before second type */
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            cypher_literal *type_lit4 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit1);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit4);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3287 "build/parser/cypher_gram.tab.c"
    break;

  case 130: /* rel_type_list: IDENTIFIER '|' BQIDENT  */
#line 849 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            cypher_literal *type_lit3 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit1);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit3);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3301 "build/parser/cypher_gram.tab.c"
    break;

  case 131: /* rel_type_list: IDENTIFIER '|' ':' BQIDENT  */
#line 859 "src/backend/parser/cypher_gram.y"
        {
            /* Support [:TYPE1|:`backtick-type`] syntax */
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            cypher_literal *type_lit4 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit1);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit4);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3316 "build/parser/cypher_gram.tab.c"
    break;

  case 132: /* rel_type_list: BQIDENT '|' IDENTIFIER  */
#line 870 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            cypher_literal *type_lit3 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit1);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit3);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3330 "build/parser/cypher_gram.tab.c"
    break;

  case 133: /* rel_type_list: BQIDENT '|' ':' IDENTIFIER  */
#line 880 "src/backend/parser/cypher_gram.y"
        {
            /* Support [:`backtick-type`|:TYPE2] syntax */
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            cypher_literal *type_lit4 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit1);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit4);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3345 "build/parser/cypher_gram.tab.c"
    break;

  case 134: /* rel_type_list: BQIDENT '|' BQIDENT  */
#line 891 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            cypher_literal *type_lit3 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit1);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit3);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3359 "build/parser/cypher_gram.tab.c"
    break;

  case 135: /* rel_type_list: BQIDENT '|' ':' BQIDENT  */
#line 901 "src/backend/parser/cypher_gram.y"
        {
            /* Support [:`backtick-type`|:`backtick-type2`] syntax */
            ((*yyvalp).list) = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            cypher_literal *type_lit4 = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit1);
            ast_list_append(((*yyvalp).list), (ast_node*)type_lit4);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3374 "build/parser/cypher_gram.tab.c"
    break;

  case 136: /* rel_type_list: rel_type_list '|' IDENTIFIER  */
#line 912 "src/backend/parser/cypher_gram.y"
        {
            cypher_literal *type_lit = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)type_lit);
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3385 "build/parser/cypher_gram.tab.c"
    break;

  case 137: /* rel_type_list: rel_type_list '|' BQIDENT  */
#line 919 "src/backend/parser/cypher_gram.y"
        {
            cypher_literal *type_lit = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)type_lit);
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3396 "build/parser/cypher_gram.tab.c"
    break;

  case 138: /* rel_type_list: rel_type_list '|' ':' IDENTIFIER  */
#line 926 "src/backend/parser/cypher_gram.y"
        {
            cypher_literal *type_lit = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (ast_node*)type_lit);
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3407 "build/parser/cypher_gram.tab.c"
    break;

  case 139: /* rel_type_list: rel_type_list '|' ':' BQIDENT  */
#line 933 "src/backend/parser/cypher_gram.y"
        {
            cypher_literal *type_lit = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (ast_node*)type_lit);
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3418 "build/parser/cypher_gram.tab.c"
    break;

  case 140: /* expr: primary_expr  */
#line 943 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3424 "build/parser/cypher_gram.tab.c"
    break;

  case 141: /* expr: '+' expr  */
#line 944 "src/backend/parser/cypher_gram.y"
                                 { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3430 "build/parser/cypher_gram.tab.c"
    break;

  case 142: /* expr: '-' expr  */
#line 945 "src/backend/parser/cypher_gram.y"
                                  { 
        /* Handle unary minus */
        if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node)->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            if (lit->literal_type == LITERAL_INTEGER) {
                lit->value.integer = -lit->value.integer;
                ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            } else if (lit->literal_type == LITERAL_DECIMAL) {
                lit->value.decimal = -lit->value.decimal;
                ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            } else {
                /* For other types, we'd need a unary minus node */
                ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
            }
        } else {
            /* For non-literals, we'd need a unary minus expression node */
            ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node);
        }
    }
#line 3454 "build/parser/cypher_gram.tab.c"
    break;

  case 143: /* expr: expr '+' expr  */
#line 964 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_ADD, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3460 "build/parser/cypher_gram.tab.c"
    break;

  case 144: /* expr: expr '-' expr  */
#line 965 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_SUB, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3466 "build/parser/cypher_gram.tab.c"
    break;

  case 145: /* expr: expr '*' expr  */
#line 966 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_MUL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3472 "build/parser/cypher_gram.tab.c"
    break;

  case 146: /* expr: expr '/' expr  */
#line 967 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_DIV, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3478 "build/parser/cypher_gram.tab.c"
    break;

  case 147: /* expr: expr '%' expr  */
#line 968 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_MOD, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3484 "build/parser/cypher_gram.tab.c"
    break;

  case 148: /* expr: expr '=' expr  */
#line 969 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_EQ, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3490 "build/parser/cypher_gram.tab.c"
    break;

  case 149: /* expr: expr NOT_EQ expr  */
#line 970 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_NEQ, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3496 "build/parser/cypher_gram.tab.c"
    break;

  case 150: /* expr: expr '<' expr  */
#line 971 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_LT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3502 "build/parser/cypher_gram.tab.c"
    break;

  case 151: /* expr: expr '>' expr  */
#line 972 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_GT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3508 "build/parser/cypher_gram.tab.c"
    break;

  case 152: /* expr: expr LT_EQ expr  */
#line 973 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_LTE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3514 "build/parser/cypher_gram.tab.c"
    break;

  case 153: /* expr: expr GT_EQ expr  */
#line 974 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_GTE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3520 "build/parser/cypher_gram.tab.c"
    break;

  case 154: /* expr: expr REGEX_MATCH expr  */
#line 975 "src/backend/parser/cypher_gram.y"
                            { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_REGEX_MATCH, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3526 "build/parser/cypher_gram.tab.c"
    break;

  case 155: /* expr: expr AND expr  */
#line 976 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_AND, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3532 "build/parser/cypher_gram.tab.c"
    break;

  case 156: /* expr: expr OR expr  */
#line 977 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_OR, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3538 "build/parser/cypher_gram.tab.c"
    break;

  case 157: /* expr: expr XOR expr  */
#line 978 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_XOR, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3544 "build/parser/cypher_gram.tab.c"
    break;

  case 158: /* expr: expr IN expr  */
#line 979 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_IN, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3550 "build/parser/cypher_gram.tab.c"
    break;

  case 159: /* expr: expr STARTS WITH expr  */
#line 980 "src/backend/parser/cypher_gram.y"
                                         { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_STARTS_WITH, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); }
#line 3556 "build/parser/cypher_gram.tab.c"
    break;

  case 160: /* expr: expr ENDS WITH expr  */
#line 981 "src/backend/parser/cypher_gram.y"
                                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_ENDS_WITH, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); }
#line 3562 "build/parser/cypher_gram.tab.c"
    break;

  case 161: /* expr: expr CONTAINS expr  */
#line 982 "src/backend/parser/cypher_gram.y"
                                        { ((*yyvalp).node) = (ast_node*)make_binary_op(BINARY_OP_CONTAINS, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3568 "build/parser/cypher_gram.tab.c"
    break;

  case 162: /* expr: NOT expr  */
#line 983 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)make_not_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3574 "build/parser/cypher_gram.tab.c"
    break;

  case 163: /* expr: expr IS NULL_P  */
#line 984 "src/backend/parser/cypher_gram.y"
                          { ((*yyvalp).node) = (ast_node*)make_null_check((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line); }
#line 3580 "build/parser/cypher_gram.tab.c"
    break;

  case 164: /* expr: expr IS NOT NULL_P  */
#line 985 "src/backend/parser/cypher_gram.y"
                          { ((*yyvalp).node) = (ast_node*)make_null_check((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), true, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line); }
#line 3586 "build/parser/cypher_gram.tab.c"
    break;

  case 165: /* expr: '(' expr ')'  */
#line 986 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node); }
#line 3592 "build/parser/cypher_gram.tab.c"
    break;

  case 166: /* primary_expr: literal_expr  */
#line 990 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3598 "build/parser/cypher_gram.tab.c"
    break;

  case 167: /* primary_expr: identifier  */
#line 991 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.identifier); }
#line 3604 "build/parser/cypher_gram.tab.c"
    break;

  case 168: /* primary_expr: parameter  */
#line 992 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.parameter); }
#line 3610 "build/parser/cypher_gram.tab.c"
    break;

  case 169: /* primary_expr: function_call  */
#line 993 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3616 "build/parser/cypher_gram.tab.c"
    break;

  case 170: /* primary_expr: list_predicate  */
#line 994 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3622 "build/parser/cypher_gram.tab.c"
    break;

  case 171: /* primary_expr: reduce_expr  */
#line 995 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3628 "build/parser/cypher_gram.tab.c"
    break;

  case 172: /* primary_expr: list_literal  */
#line 996 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3634 "build/parser/cypher_gram.tab.c"
    break;

  case 173: /* primary_expr: list_comprehension  */
#line 997 "src/backend/parser/cypher_gram.y"
                         { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3640 "build/parser/cypher_gram.tab.c"
    break;

  case 174: /* primary_expr: pattern_comprehension  */
#line 998 "src/backend/parser/cypher_gram.y"
                            { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3646 "build/parser/cypher_gram.tab.c"
    break;

  case 175: /* primary_expr: map_literal  */
#line 999 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3652 "build/parser/cypher_gram.tab.c"
    break;

  case 176: /* primary_expr: map_projection  */
#line 1000 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3658 "build/parser/cypher_gram.tab.c"
    break;

  case 177: /* primary_expr: case_expression  */
#line 1001 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node); }
#line 3664 "build/parser/cypher_gram.tab.c"
    break;

  case 178: /* primary_expr: IDENTIFIER '.' IDENTIFIER  */
#line 1003 "src/backend/parser/cypher_gram.y"
        {
            cypher_identifier *base = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node) = (ast_node*)make_property((ast_node*)base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3675 "build/parser/cypher_gram.tab.c"
    break;

  case 179: /* primary_expr: END_P '.' IDENTIFIER  */
#line 1010 "src/backend/parser/cypher_gram.y"
        {
            /* Allow 'end' as identifier in property access: end.name */
            cypher_identifier *base = make_identifier(strdup("end"), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node) = (ast_node*)make_property((ast_node*)base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3686 "build/parser/cypher_gram.tab.c"
    break;

  case 180: /* primary_expr: IDENTIFIER ':' IDENTIFIER  */
#line 1017 "src/backend/parser/cypher_gram.y"
        {
            cypher_identifier *base = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            ((*yyvalp).node) = (ast_node*)make_label_expr((ast_node*)base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 3697 "build/parser/cypher_gram.tab.c"
    break;

  case 181: /* primary_expr: IDENTIFIER '[' expr ']'  */
#line 1024 "src/backend/parser/cypher_gram.y"
        {
            /* Array subscript on identifier: items[idx] */
            cypher_identifier *base = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node) = (ast_node*)make_subscript((ast_node*)base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
        }
#line 3708 "build/parser/cypher_gram.tab.c"
    break;

  case 182: /* primary_expr: '(' expr ')' '[' expr ']'  */
#line 1031 "src/backend/parser/cypher_gram.y"
        {
            /* Array subscript on parenthesized expression: (expr)[idx] */
            ((*yyvalp).node) = (ast_node*)make_subscript((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 3717 "build/parser/cypher_gram.tab.c"
    break;

  case 183: /* primary_expr: list_literal '[' expr ']'  */
#line 1036 "src/backend/parser/cypher_gram.y"
        {
            /* Array subscript on list literal: [1,2,3][0] */
            ((*yyvalp).node) = (ast_node*)make_subscript((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 3726 "build/parser/cypher_gram.tab.c"
    break;

  case 184: /* literal_expr: literal  */
#line 1043 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).node) = (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.literal); }
#line 3732 "build/parser/cypher_gram.tab.c"
    break;

  case 185: /* function_call: IDENTIFIER '(' ')'  */
#line 1048 "src/backend/parser/cypher_gram.y"
        {
            /* Check if this is EXISTS function call */
            if (strcasecmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), "exists") == 0) {
                /* Empty EXISTS() - invalid */
                free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
                cypher_yyerror(&(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc), context, "EXISTS requires an argument");
                YYERROR;
            } else {
                ast_list *args = ast_list_create();
                ((*yyvalp).node) = (ast_node*)make_function_call((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), args, false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
                free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
            }
        }
#line 3750 "build/parser/cypher_gram.tab.c"
    break;

  case 186: /* function_call: IDENTIFIER '(' argument_list ')'  */
#line 1063 "src/backend/parser/cypher_gram.y"
        {
            /* Check if this is EXISTS function call */
            if (strcasecmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), "exists") == 0) {
                /* EXISTS with argument list - check first arg */
                if ((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list)->count == 1 && (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list)->items[0] != NULL) {
                    ast_node *arg = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list)->items[0];
                    if (arg->type == AST_NODE_PROPERTY) {
                        ((*yyvalp).node) = (ast_node*)make_exists_property_expr(arg, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                        (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list)->items[0] = NULL;  /* Transfer ownership */
                        ast_list_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list));
                    } else {
                        ((*yyvalp).node) = (ast_node*)make_exists_pattern_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                    }
                } else {
                    cypher_yyerror(&(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc), context, "EXISTS requires exactly one argument");
                    ast_list_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list));
                    free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
                    YYERROR;
                }
                free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
            } else {
                ((*yyvalp).node) = (ast_node*)make_function_call((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
                free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
            }
        }
#line 3780 "build/parser/cypher_gram.tab.c"
    break;

  case 187: /* function_call: IDENTIFIER '(' DISTINCT expr ')'  */
#line 1089 "src/backend/parser/cypher_gram.y"
        {
            ast_list *args = ast_list_create();
            ast_list_append(args, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node));
            ((*yyvalp).node) = (ast_node*)make_function_call((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string), args, true, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.string));
        }
#line 3791 "build/parser/cypher_gram.tab.c"
    break;

  case 188: /* function_call: IDENTIFIER '(' '*' ')'  */
#line 1096 "src/backend/parser/cypher_gram.y"
        {
            ast_list *args = ast_list_create();
            /* For count(*), we'll use a special NULL argument to indicate * */
            ast_list_append(args, NULL);
            ((*yyvalp).node) = (ast_node*)make_function_call((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), args, false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
        }
#line 3803 "build/parser/cypher_gram.tab.c"
    break;

  case 189: /* function_call: EXISTS '(' pattern_list ')'  */
#line 1104 "src/backend/parser/cypher_gram.y"
        {
            /* EXISTS((pattern)) - check for relationship/path existence */
            ((*yyvalp).node) = (ast_node*)make_exists_pattern_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
        }
#line 3812 "build/parser/cypher_gram.tab.c"
    break;

  case 190: /* function_call: EXISTS '(' IDENTIFIER '.' IDENTIFIER ')'  */
#line 1109 "src/backend/parser/cypher_gram.y"
        {
            /* EXISTS(n.property) - unambiguous property existence check */
            ast_node *var = (ast_node*)make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ast_node *prop = (ast_node*)make_property(var, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            ((*yyvalp).node) = (ast_node*)make_exists_property_expr(prop, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.string));
        }
#line 3825 "build/parser/cypher_gram.tab.c"
    break;

  case 191: /* function_call: CONTAINS '(' argument_list ')'  */
#line 1119 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_function_call(strdup("contains"), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
        }
#line 3833 "build/parser/cypher_gram.tab.c"
    break;

  case 192: /* function_call: STARTS '(' argument_list ')'  */
#line 1123 "src/backend/parser/cypher_gram.y"
        {
            /* startsWith function uses STARTS keyword */
            ((*yyvalp).node) = (ast_node*)make_function_call(strdup("startsWith"), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
        }
#line 3842 "build/parser/cypher_gram.tab.c"
    break;

  case 193: /* function_call: ENDS '(' argument_list ')'  */
#line 1128 "src/backend/parser/cypher_gram.y"
        {
            /* endsWith function uses ENDS keyword */
            ((*yyvalp).node) = (ast_node*)make_function_call(strdup("endsWith"), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
        }
#line 3851 "build/parser/cypher_gram.tab.c"
    break;

  case 194: /* list_predicate: ALL '(' IDENTIFIER IN expr WHERE expr ')'  */
#line 1137 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list_predicate(LIST_PRED_ALL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3860 "build/parser/cypher_gram.tab.c"
    break;

  case 195: /* list_predicate: ANY '(' IDENTIFIER IN expr WHERE expr ')'  */
#line 1142 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list_predicate(LIST_PRED_ANY, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3869 "build/parser/cypher_gram.tab.c"
    break;

  case 196: /* list_predicate: NONE '(' IDENTIFIER IN expr WHERE expr ')'  */
#line 1147 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list_predicate(LIST_PRED_NONE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3878 "build/parser/cypher_gram.tab.c"
    break;

  case 197: /* list_predicate: SINGLE '(' IDENTIFIER IN expr WHERE expr ')'  */
#line 1152 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list_predicate(LIST_PRED_SINGLE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3887 "build/parser/cypher_gram.tab.c"
    break;

  case 198: /* reduce_expr: REDUCE '(' IDENTIFIER '=' expr ',' IDENTIFIER IN expr '|' expr ')'  */
#line 1161 "src/backend/parser/cypher_gram.y"
        {
            /* reduce(acc = initial, x IN list | expression) */
            ((*yyvalp).node) = (ast_node*)make_reduce_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-11)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3898 "build/parser/cypher_gram.tab.c"
    break;

  case 199: /* argument_list: expr  */
#line 1172 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 3907 "build/parser/cypher_gram.tab.c"
    break;

  case 200: /* argument_list: argument_list ',' expr  */
#line 1177 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 3916 "build/parser/cypher_gram.tab.c"
    break;

  case 201: /* list_literal: '[' ']'  */
#line 1186 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list(ast_list_create(), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
        }
#line 3924 "build/parser/cypher_gram.tab.c"
    break;

  case 202: /* list_literal: '[' return_item_list ']'  */
#line 1190 "src/backend/parser/cypher_gram.y"
        {
            /* Reuse return_item_list for comma-separated expressions */
            /* But we need to extract the expressions from return_items */
            ast_list *exprs = ast_list_create();
            for (int i = 0; i < (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list)->count; i++) {
                cypher_return_item *item = (cypher_return_item*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list)->items[i];
                ast_list_append(exprs, item->expr);
                item->expr = NULL;  /* Prevent double-free */
            }
            ast_list_free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list));
            ((*yyvalp).node) = (ast_node*)make_list(exprs, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 3941 "build/parser/cypher_gram.tab.c"
    break;

  case 203: /* list_comprehension: '[' IDENTIFIER IN expr ']'  */
#line 1207 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list_comprehension((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), NULL, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
        }
#line 3950 "build/parser/cypher_gram.tab.c"
    break;

  case 204: /* list_comprehension: '[' IDENTIFIER IN expr WHERE expr ']'  */
#line 1212 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list_comprehension((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3959 "build/parser/cypher_gram.tab.c"
    break;

  case 205: /* list_comprehension: '[' IDENTIFIER IN expr '|' expr ']'  */
#line 1217 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list_comprehension((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.string));
        }
#line 3968 "build/parser/cypher_gram.tab.c"
    break;

  case 206: /* list_comprehension: '[' IDENTIFIER IN expr WHERE expr '|' expr ']'  */
#line 1222 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_list_comprehension((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.string));
        }
#line 3977 "build/parser/cypher_gram.tab.c"
    break;

  case 207: /* pattern_comprehension: '[' '(' variable_opt label_opt properties_opt ')' rel_pattern node_pattern '|' expr ']'  */
#line 1239 "src/backend/parser/cypher_gram.y"
        {
            cypher_node_pattern *first = make_node_pattern((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.map));
            ast_list *elements = ast_list_create();
            ast_list_append(elements, (ast_node*)first);
            ast_list_append(elements, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.rel_pattern));
            ast_list_append(elements, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node_pattern));
            cypher_path *path = make_path(elements);

            ast_list *pattern = ast_list_create();
            ast_list_append(pattern, (ast_node*)path);
            ((*yyvalp).node) = (ast_node*)make_pattern_comprehension(pattern, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yyloc).first_line);
        }
#line 3994 "build/parser/cypher_gram.tab.c"
    break;

  case 208: /* pattern_comprehension: '[' '(' variable_opt label_opt properties_opt ')' rel_pattern node_pattern WHERE expr '|' expr ']'  */
#line 1252 "src/backend/parser/cypher_gram.y"
        {
            cypher_node_pattern *first = make_node_pattern((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.map));
            ast_list *elements = ast_list_create();
            ast_list_append(elements, (ast_node*)first);
            ast_list_append(elements, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.rel_pattern));
            ast_list_append(elements, (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.node_pattern));
            cypher_path *path = make_path(elements);

            ast_list *pattern = ast_list_create();
            ast_list_append(pattern, (ast_node*)path);
            ((*yyvalp).node) = (ast_node*)make_pattern_comprehension(pattern, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yyloc).first_line);
        }
#line 4011 "build/parser/cypher_gram.tab.c"
    break;

  case 209: /* map_literal: '{' '}'  */
#line 1269 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_map(ast_list_create(), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
        }
#line 4019 "build/parser/cypher_gram.tab.c"
    break;

  case 210: /* map_literal: '{' map_pair_list '}'  */
#line 1273 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_map((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4027 "build/parser/cypher_gram.tab.c"
    break;

  case 211: /* map_projection: IDENTIFIER '{' map_projection_list '}'  */
#line 1281 "src/backend/parser/cypher_gram.y"
        {
            cypher_identifier *base = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            ((*yyvalp).node) = (ast_node*)make_map_projection((ast_node*)base, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
        }
#line 4037 "build/parser/cypher_gram.tab.c"
    break;

  case 212: /* map_projection_list: map_projection_item  */
#line 1290 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 4046 "build/parser/cypher_gram.tab.c"
    break;

  case 213: /* map_projection_list: map_projection_list ',' map_projection_item  */
#line 1295 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 4055 "build/parser/cypher_gram.tab.c"
    break;

  case 214: /* map_projection_item: '.' IDENTIFIER  */
#line 1303 "src/backend/parser/cypher_gram.y"
        {
            /* Shorthand: .prop -> key=prop, property=prop, expr=NULL */
            ((*yyvalp).node) = (ast_node*)make_map_projection_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 4065 "build/parser/cypher_gram.tab.c"
    break;

  case 215: /* map_projection_item: '.' '*'  */
#line 1309 "src/backend/parser/cypher_gram.y"
        {
            /* All properties: .* -> key=NULL, property="*", expr=NULL */
            ((*yyvalp).node) = (ast_node*)make_map_projection_item(NULL, strdup("*"), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yyloc).first_line);
        }
#line 4074 "build/parser/cypher_gram.tab.c"
    break;

  case 216: /* map_projection_item: IDENTIFIER ':' '.' IDENTIFIER  */
#line 1314 "src/backend/parser/cypher_gram.y"
        {
            /* Aliased property: alias: .prop */
            ((*yyvalp).node) = (ast_node*)make_map_projection_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.string));
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 4085 "build/parser/cypher_gram.tab.c"
    break;

  case 217: /* map_projection_item: IDENTIFIER ':' expr  */
#line 1321 "src/backend/parser/cypher_gram.y"
        {
            /* Computed value: alias: expr */
            ((*yyvalp).node) = (ast_node*)make_map_projection_item((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string));
        }
#line 4095 "build/parser/cypher_gram.tab.c"
    break;

  case 218: /* case_expression: CASE when_clause_list END_P  */
#line 1335 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_case_expr(NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4103 "build/parser/cypher_gram.tab.c"
    break;

  case 219: /* case_expression: CASE when_clause_list ELSE expr END_P  */
#line 1339 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_case_expr(NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yyloc).first_line);
        }
#line 4111 "build/parser/cypher_gram.tab.c"
    break;

  case 220: /* case_expression: CASE expr when_clause_list END_P  */
#line 1344 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_case_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
        }
#line 4119 "build/parser/cypher_gram.tab.c"
    break;

  case 221: /* case_expression: CASE expr when_clause_list ELSE expr END_P  */
#line 1348 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_case_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yyloc).first_line);
        }
#line 4127 "build/parser/cypher_gram.tab.c"
    break;

  case 222: /* when_clause_list: when_clause  */
#line 1355 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
        }
#line 4136 "build/parser/cypher_gram.tab.c"
    break;

  case 223: /* when_clause_list: when_clause_list when_clause  */
#line 1360 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list);
        }
#line 4145 "build/parser/cypher_gram.tab.c"
    break;

  case 224: /* when_clause: WHEN expr THEN expr  */
#line 1368 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).node) = (ast_node*)make_when_clause((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yyloc).first_line);
        }
#line 4153 "build/parser/cypher_gram.tab.c"
    break;

  case 225: /* literal: INTEGER  */
#line 1375 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).literal) = make_integer_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.integer), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 4161 "build/parser/cypher_gram.tab.c"
    break;

  case 226: /* literal: DECIMAL  */
#line 1379 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).literal) = make_decimal_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.decimal), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 4169 "build/parser/cypher_gram.tab.c"
    break;

  case 227: /* literal: STRING  */
#line 1383 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).literal) = make_string_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 4178 "build/parser/cypher_gram.tab.c"
    break;

  case 228: /* literal: TRUE_P  */
#line 1388 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).literal) = make_boolean_literal(true, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 4186 "build/parser/cypher_gram.tab.c"
    break;

  case 229: /* literal: FALSE_P  */
#line 1392 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).literal) = make_boolean_literal(false, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 4194 "build/parser/cypher_gram.tab.c"
    break;

  case 230: /* literal: NULL_P  */
#line 1396 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).literal) = make_null_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 4202 "build/parser/cypher_gram.tab.c"
    break;

  case 231: /* identifier: IDENTIFIER  */
#line 1403 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).identifier) = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 4211 "build/parser/cypher_gram.tab.c"
    break;

  case 232: /* identifier: BQIDENT  */
#line 1408 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).identifier) = make_identifier((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 4220 "build/parser/cypher_gram.tab.c"
    break;

  case 233: /* identifier: END_P  */
#line 1413 "src/backend/parser/cypher_gram.y"
        {
            /* Allow 'end' as an identifier - it's only reserved in CASE...END context */
            ((*yyvalp).identifier) = make_identifier(strdup("end"), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
        }
#line 4229 "build/parser/cypher_gram.tab.c"
    break;

  case 234: /* parameter: PARAMETER  */
#line 1421 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).parameter) = make_parameter((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yyloc).first_line);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.string));
        }
#line 4238 "build/parser/cypher_gram.tab.c"
    break;

  case 235: /* properties_opt: %empty  */
#line 1429 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).map) = NULL; }
#line 4244 "build/parser/cypher_gram.tab.c"
    break;

  case 236: /* properties_opt: '{' '}'  */
#line 1430 "src/backend/parser/cypher_gram.y"
                        { ((*yyvalp).map) = NULL; }
#line 4250 "build/parser/cypher_gram.tab.c"
    break;

  case 237: /* properties_opt: '{' map_pair_list '}'  */
#line 1432 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).map) = make_map((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.list), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4258 "build/parser/cypher_gram.tab.c"
    break;

  case 238: /* map_pair_list: map_pair  */
#line 1439 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).list) = ast_list_create();
            ast_list_append(((*yyvalp).list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.map_pair));
        }
#line 4267 "build/parser/cypher_gram.tab.c"
    break;

  case 239: /* map_pair_list: map_pair_list ',' map_pair  */
#line 1444 "src/backend/parser/cypher_gram.y"
        {
            ast_list_append((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list), (ast_node*)(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.map_pair));
            ((*yyvalp).list) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.list);
        }
#line 4276 "build/parser/cypher_gram.tab.c"
    break;

  case 240: /* map_pair: IDENTIFIER ':' expr  */
#line 1452 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).map_pair) = make_map_pair((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4284 "build/parser/cypher_gram.tab.c"
    break;

  case 241: /* map_pair: BQIDENT ':' expr  */
#line 1456 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).map_pair) = make_map_pair((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4292 "build/parser/cypher_gram.tab.c"
    break;

  case 242: /* map_pair: STRING ':' expr  */
#line 1460 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).map_pair) = make_map_pair((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.string), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4300 "build/parser/cypher_gram.tab.c"
    break;

  case 243: /* map_pair: ASC ':' expr  */
#line 1464 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).map_pair) = make_map_pair("asc", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4308 "build/parser/cypher_gram.tab.c"
    break;

  case 244: /* map_pair: DESC ':' expr  */
#line 1468 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).map_pair) = make_map_pair("desc", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4316 "build/parser/cypher_gram.tab.c"
    break;

  case 245: /* map_pair: ORDER ':' expr  */
#line 1472 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).map_pair) = make_map_pair("order", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4324 "build/parser/cypher_gram.tab.c"
    break;

  case 246: /* map_pair: BY ':' expr  */
#line 1476 "src/backend/parser/cypher_gram.y"
        {
            ((*yyvalp).map_pair) = make_map_pair("by", (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.node), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yyloc).first_line);
        }
#line 4332 "build/parser/cypher_gram.tab.c"
    break;


#line 4336 "build/parser/cypher_gram.tab.c"

      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yylhsNonterm (yyrule), yyvalp, yylocp);

  return yyok;
# undef yyerrok
# undef YYABORT
# undef YYACCEPT
# undef YYNOMEM
# undef YYERROR
# undef YYBACKUP
# undef yyclearin
# undef YYRECOVERING
}


static void
yyuserMerge (int yyn, YYSTYPE* yy0, YYSTYPE* yy1)
{
  YY_USE (yy0);
  YY_USE (yy1);

  switch (yyn)
    {

      default: break;
    }
}

                              /* Bison grammar-table manipulation.  */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, cypher_parser_context *context)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (context);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/** Number of symbols composing the right hand side of rule #RULE.  */
static inline int
yyrhsLength (yyRuleNum yyrule)
{
  return yyr2[yyrule];
}

static void
yydestroyGLRState (char const *yymsg, yyGLRState *yys, cypher_parser_context *context)
{
  if (yys->yyresolved)
    yydestruct (yymsg, yy_accessing_symbol (yys->yylrState),
                &yys->yysemantics.yyval, &yys->yyloc, context);
  else
    {
#if CYPHER_YYDEBUG
      if (yydebug)
        {
          if (yys->yysemantics.yyfirstVal)
            YY_FPRINTF ((stderr, "%s unresolved", yymsg));
          else
            YY_FPRINTF ((stderr, "%s incomplete", yymsg));
          YY_SYMBOL_PRINT ("", yy_accessing_symbol (yys->yylrState), YY_NULLPTR, &yys->yyloc);
        }
#endif

      if (yys->yysemantics.yyfirstVal)
        {
          yySemanticOption *yyoption = yys->yysemantics.yyfirstVal;
          yyGLRState *yyrh;
          int yyn;
          for (yyrh = yyoption->yystate, yyn = yyrhsLength (yyoption->yyrule);
               yyn > 0;
               yyrh = yyrh->yypred, yyn -= 1)
            yydestroyGLRState (yymsg, yyrh, context);
        }
    }
}

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

/** True iff LR state YYSTATE has only a default reduction (regardless
 *  of token).  */
static inline yybool
yyisDefaultedState (yy_state_t yystate)
{
  return yypact_value_is_default (yypact[yystate]);
}

/** The default reduction for YYSTATE, assuming it has one.  */
static inline yyRuleNum
yydefaultAction (yy_state_t yystate)
{
  return yydefact[yystate];
}

#define yytable_value_is_error(Yyn) \
  0

/** The action to take in YYSTATE on seeing YYTOKEN.
 *  Result R means
 *    R < 0:  Reduce on rule -R.
 *    R = 0:  Error.
 *    R > 0:  Shift to state R.
 *  Set *YYCONFLICTS to a pointer into yyconfl to a 0-terminated list
 *  of conflicting reductions.
 */
static inline int
yygetLRActions (yy_state_t yystate, yysymbol_kind_t yytoken, const short** yyconflicts)
{
  int yyindex = yypact[yystate] + yytoken;
  if (yytoken == YYSYMBOL_YYerror)
    {
      // This is the error token.
      *yyconflicts = yyconfl;
      return 0;
    }
  else if (yyisDefaultedState (yystate)
           || yyindex < 0 || YYLAST < yyindex || yycheck[yyindex] != yytoken)
    {
      *yyconflicts = yyconfl;
      return -yydefact[yystate];
    }
  else if (! yytable_value_is_error (yytable[yyindex]))
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return yytable[yyindex];
    }
  else
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return 0;
    }
}

/** Compute post-reduction state.
 * \param yystate   the current state
 * \param yysym     the nonterminal to push on the stack
 */
static inline yy_state_t
yyLRgotoState (yy_state_t yystate, yysymbol_kind_t yysym)
{
  int yyr = yypgoto[yysym - YYNTOKENS] + yystate;
  if (0 <= yyr && yyr <= YYLAST && yycheck[yyr] == yystate)
    return yytable[yyr];
  else
    return yydefgoto[yysym - YYNTOKENS];
}

static inline yybool
yyisShiftAction (int yyaction)
{
  return 0 < yyaction;
}

static inline yybool
yyisErrorAction (int yyaction)
{
  return yyaction == 0;
}

                                /* GLRStates */

/** Return a fresh GLRStackItem in YYSTACKP.  The item is an LR state
 *  if YYISSTATE, and otherwise a semantic option.  Callers should call
 *  YY_RESERVE_GLRSTACK afterwards to make sure there is sufficient
 *  headroom.  */

static inline yyGLRStackItem*
yynewGLRStackItem (yyGLRStack* yystackp, yybool yyisState)
{
  yyGLRStackItem* yynewItem = yystackp->yynextFree;
  yystackp->yyspaceLeft -= 1;
  yystackp->yynextFree += 1;
  yynewItem->yystate.yyisState = yyisState;
  return yynewItem;
}

/** Add a new semantic action that will execute the action for rule
 *  YYRULE on the semantic values in YYRHS to the list of
 *  alternative actions for YYSTATE.  Assumes that YYRHS comes from
 *  stack #YYK of *YYSTACKP. */
static void
yyaddDeferredAction (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyGLRState* yystate,
                     yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yySemanticOption* yynewOption =
    &yynewGLRStackItem (yystackp, yyfalse)->yyoption;
  YY_ASSERT (!yynewOption->yyisState);
  yynewOption->yystate = yyrhs;
  yynewOption->yyrule = yyrule;
  if (yystackp->yytops.yylookaheadNeeds[yyk])
    {
      yynewOption->yyrawchar = yychar;
      yynewOption->yyval = yylval;
      yynewOption->yyloc = yylloc;
    }
  else
    yynewOption->yyrawchar = CYPHER_CYPHER_YYEMPTY;
  yynewOption->yynext = yystate->yysemantics.yyfirstVal;
  yystate->yysemantics.yyfirstVal = yynewOption;

  YY_RESERVE_GLRSTACK (yystackp);
}

                                /* GLRStacks */

/** Initialize YYSET to a singleton set containing an empty stack.  */
static yybool
yyinitStateSet (yyGLRStateSet* yyset)
{
  yyset->yysize = 1;
  yyset->yycapacity = 16;
  yyset->yystates
    = YY_CAST (yyGLRState**,
               YYMALLOC (YY_CAST (YYSIZE_T, yyset->yycapacity)
                         * sizeof yyset->yystates[0]));
  if (! yyset->yystates)
    return yyfalse;
  yyset->yystates[0] = YY_NULLPTR;
  yyset->yylookaheadNeeds
    = YY_CAST (yybool*,
               YYMALLOC (YY_CAST (YYSIZE_T, yyset->yycapacity)
                         * sizeof yyset->yylookaheadNeeds[0]));
  if (! yyset->yylookaheadNeeds)
    {
      YYFREE (yyset->yystates);
      return yyfalse;
    }
  memset (yyset->yylookaheadNeeds,
          0,
          YY_CAST (YYSIZE_T, yyset->yycapacity) * sizeof yyset->yylookaheadNeeds[0]);
  return yytrue;
}

static void yyfreeStateSet (yyGLRStateSet* yyset)
{
  YYFREE (yyset->yystates);
  YYFREE (yyset->yylookaheadNeeds);
}

/** Initialize *YYSTACKP to a single empty stack, with total maximum
 *  capacity for all stacks of YYSIZE.  */
static yybool
yyinitGLRStack (yyGLRStack* yystackp, YYPTRDIFF_T yysize)
{
  yystackp->yyerrState = 0;
  yynerrs = 0;
  yystackp->yyspaceLeft = yysize;
  yystackp->yyitems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (YYSIZE_T, yysize)
                         * sizeof yystackp->yynextFree[0]));
  if (!yystackp->yyitems)
    return yyfalse;
  yystackp->yynextFree = yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;
  return yyinitStateSet (&yystackp->yytops);
}


#if YYSTACKEXPANDABLE
# define YYRELOC(YYFROMITEMS, YYTOITEMS, YYX, YYTYPE)                   \
  &((YYTOITEMS)                                                         \
    - ((YYFROMITEMS) - YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX))))->YYTYPE

/** If *YYSTACKP is expandable, extend it.  WARNING: Pointers into the
    stack from outside should be considered invalid after this call.
    We always expand when there are 1 or fewer items left AFTER an
    allocation, so that we can avoid having external pointers exist
    across an allocation.  */
static void
yyexpandGLRStack (yyGLRStack* yystackp)
{
  yyGLRStackItem* yynewItems;
  yyGLRStackItem* yyp0, *yyp1;
  YYPTRDIFF_T yynewSize;
  YYPTRDIFF_T yyn;
  YYPTRDIFF_T yysize = yystackp->yynextFree - yystackp->yyitems;
  if (YYMAXDEPTH - YYHEADROOM < yysize)
    yyMemoryExhausted (yystackp);
  yynewSize = 2*yysize;
  if (YYMAXDEPTH < yynewSize)
    yynewSize = YYMAXDEPTH;
  yynewItems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (YYSIZE_T, yynewSize)
                         * sizeof yynewItems[0]));
  if (! yynewItems)
    yyMemoryExhausted (yystackp);
  for (yyp0 = yystackp->yyitems, yyp1 = yynewItems, yyn = yysize;
       0 < yyn;
       yyn -= 1, yyp0 += 1, yyp1 += 1)
    {
      *yyp1 = *yyp0;
      if (*YY_REINTERPRET_CAST (yybool *, yyp0))
        {
          yyGLRState* yys0 = &yyp0->yystate;
          yyGLRState* yys1 = &yyp1->yystate;
          if (yys0->yypred != YY_NULLPTR)
            yys1->yypred =
              YYRELOC (yyp0, yyp1, yys0->yypred, yystate);
          if (! yys0->yyresolved && yys0->yysemantics.yyfirstVal != YY_NULLPTR)
            yys1->yysemantics.yyfirstVal =
              YYRELOC (yyp0, yyp1, yys0->yysemantics.yyfirstVal, yyoption);
        }
      else
        {
          yySemanticOption* yyv0 = &yyp0->yyoption;
          yySemanticOption* yyv1 = &yyp1->yyoption;
          if (yyv0->yystate != YY_NULLPTR)
            yyv1->yystate = YYRELOC (yyp0, yyp1, yyv0->yystate, yystate);
          if (yyv0->yynext != YY_NULLPTR)
            yyv1->yynext = YYRELOC (yyp0, yyp1, yyv0->yynext, yyoption);
        }
    }
  if (yystackp->yysplitPoint != YY_NULLPTR)
    yystackp->yysplitPoint = YYRELOC (yystackp->yyitems, yynewItems,
                                      yystackp->yysplitPoint, yystate);

  for (yyn = 0; yyn < yystackp->yytops.yysize; yyn += 1)
    if (yystackp->yytops.yystates[yyn] != YY_NULLPTR)
      yystackp->yytops.yystates[yyn] =
        YYRELOC (yystackp->yyitems, yynewItems,
                 yystackp->yytops.yystates[yyn], yystate);
  YYFREE (yystackp->yyitems);
  yystackp->yyitems = yynewItems;
  yystackp->yynextFree = yynewItems + yysize;
  yystackp->yyspaceLeft = yynewSize - yysize;
}
#endif

static void
yyfreeGLRStack (yyGLRStack* yystackp)
{
  YYFREE (yystackp->yyitems);
  yyfreeStateSet (&yystackp->yytops);
}

/** Assuming that YYS is a GLRState somewhere on *YYSTACKP, update the
 *  splitpoint of *YYSTACKP, if needed, so that it is at least as deep as
 *  YYS.  */
static inline void
yyupdateSplit (yyGLRStack* yystackp, yyGLRState* yys)
{
  if (yystackp->yysplitPoint != YY_NULLPTR && yystackp->yysplitPoint > yys)
    yystackp->yysplitPoint = yys;
}

/** Invalidate stack #YYK in *YYSTACKP.  */
static inline void
yymarkStackDeleted (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    yystackp->yylastDeleted = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yystates[yyk] = YY_NULLPTR;
}

/** Undelete the last stack in *YYSTACKP that was marked as deleted.  Can
    only be done once after a deletion, and only when all other stacks have
    been deleted.  */
static void
yyundeleteLastStack (yyGLRStack* yystackp)
{
  if (yystackp->yylastDeleted == YY_NULLPTR || yystackp->yytops.yysize != 0)
    return;
  yystackp->yytops.yystates[0] = yystackp->yylastDeleted;
  yystackp->yytops.yysize = 1;
  YY_DPRINTF ((stderr, "Restoring last deleted stack as stack #0.\n"));
  yystackp->yylastDeleted = YY_NULLPTR;
}

static inline void
yyremoveDeletes (yyGLRStack* yystackp)
{
  YYPTRDIFF_T yyi, yyj;
  yyi = yyj = 0;
  while (yyj < yystackp->yytops.yysize)
    {
      if (yystackp->yytops.yystates[yyi] == YY_NULLPTR)
        {
          if (yyi == yyj)
            YY_DPRINTF ((stderr, "Removing dead stacks.\n"));
          yystackp->yytops.yysize -= 1;
        }
      else
        {
          yystackp->yytops.yystates[yyj] = yystackp->yytops.yystates[yyi];
          /* In the current implementation, it's unnecessary to copy
             yystackp->yytops.yylookaheadNeeds[yyi] since, after
             yyremoveDeletes returns, the parser immediately either enters
             deterministic operation or shifts a token.  However, it doesn't
             hurt, and the code might evolve to need it.  */
          yystackp->yytops.yylookaheadNeeds[yyj] =
            yystackp->yytops.yylookaheadNeeds[yyi];
          if (yyj != yyi)
            YY_DPRINTF ((stderr, "Rename stack %ld -> %ld.\n",
                        YY_CAST (long, yyi), YY_CAST (long, yyj)));
          yyj += 1;
        }
      yyi += 1;
    }
}

/** Shift to a new state on stack #YYK of *YYSTACKP, corresponding to LR
 * state YYLRSTATE, at input position YYPOSN, with (resolved) semantic
 * value *YYVALP and source location *YYLOCP.  */
static inline void
yyglrShift (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yy_state_t yylrState,
            YYPTRDIFF_T yyposn,
            YYSTYPE* yyvalp, YYLTYPE* yylocp)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yytrue;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yyval = *yyvalp;
  yynewState->yyloc = *yylocp;
  yystackp->yytops.yystates[yyk] = yynewState;

  YY_RESERVE_GLRSTACK (yystackp);
}

/** Shift stack #YYK of *YYSTACKP, to a new state corresponding to LR
 *  state YYLRSTATE, at input position YYPOSN, with the (unresolved)
 *  semantic value of YYRHS under the action for YYRULE.  */
static inline void
yyglrShiftDefer (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yy_state_t yylrState,
                 YYPTRDIFF_T yyposn, yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;
  YY_ASSERT (yynewState->yyisState);

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yyfalse;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yyfirstVal = YY_NULLPTR;
  yystackp->yytops.yystates[yyk] = yynewState;

  /* Invokes YY_RESERVE_GLRSTACK.  */
  yyaddDeferredAction (yystackp, yyk, yynewState, yyrhs, yyrule);
}

#if CYPHER_YYDEBUG

/*----------------------------------------------------------------------.
| Report that stack #YYK of *YYSTACKP is going to be reduced by YYRULE. |
`----------------------------------------------------------------------*/

static inline void
yy_reduce_print (yybool yynormal, yyGLRStackItem* yyvsp, YYPTRDIFF_T yyk,
                 yyRuleNum yyrule, cypher_parser_context *context)
{
  int yynrhs = yyrhsLength (yyrule);
  int yylow = 1;
  int yyi;
  YY_FPRINTF ((stderr, "Reducing stack %ld by rule %d (line %d):\n",
               YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule]));
  if (! yynormal)
    yyfillin (yyvsp, 1, -yynrhs);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YY_FPRINTF ((stderr, "   $%d = ", yyi + 1));
      yy_symbol_print (stderr,
                       yy_accessing_symbol (yyvsp[yyi - yynrhs + 1].yystate.yylrState),
                       &yyvsp[yyi - yynrhs + 1].yystate.yysemantics.yyval,
                       &(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL ((yyi + 1) - (yynrhs))].yystate.yyloc)                       , context);
      if (!yyvsp[yyi - yynrhs + 1].yystate.yyresolved)
        YY_FPRINTF ((stderr, " (unresolved)"));
      YY_FPRINTF ((stderr, "\n"));
    }
}
#endif

/** Pop the symbols consumed by reduction #YYRULE from the top of stack
 *  #YYK of *YYSTACKP, and perform the appropriate semantic action on their
 *  semantic values.  Assumes that all ambiguities in semantic values
 *  have been previously resolved.  Set *YYVALP to the resulting value,
 *  and *YYLOCP to the computed location (if any).  Return value is as
 *  for userAction.  */
static inline YYRESULTTAG
yydoAction (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyRuleNum yyrule,
            YYSTYPE* yyvalp, YYLTYPE *yylocp, cypher_parser_context *context)
{
  int yynrhs = yyrhsLength (yyrule);

  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      /* Standard special case: single stack.  */
      yyGLRStackItem* yyrhs
        = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yytops.yystates[yyk]);
      YY_ASSERT (yyk == 0);
      yystackp->yynextFree -= yynrhs;
      yystackp->yyspaceLeft += yynrhs;
      yystackp->yytops.yystates[0] = & yystackp->yynextFree[-1].yystate;
      return yyuserAction (yyrule, yynrhs, yyrhs, yystackp, yyk,
                           yyvalp, yylocp, context);
    }
  else
    {
      yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
      yyGLRState* yys = yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred
        = yystackp->yytops.yystates[yyk];
      int yyi;
      if (yynrhs == 0)
        /* Set default location.  */
        yyrhsVals[YYMAXRHS + YYMAXLEFT - 1].yystate.yyloc = yys->yyloc;
      for (yyi = 0; yyi < yynrhs; yyi += 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yystackp->yytops.yystates[yyk] = yys;
      return yyuserAction (yyrule, yynrhs, yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, yyk, yyvalp, yylocp, context);
    }
}

/** Pop items off stack #YYK of *YYSTACKP according to grammar rule YYRULE,
 *  and push back on the resulting nonterminal symbol.  Perform the
 *  semantic action associated with YYRULE and store its value with the
 *  newly pushed state, if YYFORCEEVAL or if *YYSTACKP is currently
 *  unambiguous.  Otherwise, store the deferred semantic action with
 *  the new state.  If the new state would have an identical input
 *  position, LR state, and predecessor to an existing state on the stack,
 *  it is identified with that existing state, eliminating stack #YYK from
 *  *YYSTACKP.  In this case, the semantic value is
 *  added to the options for the existing state's semantic value.
 */
static inline YYRESULTTAG
yyglrReduce (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyRuleNum yyrule,
             yybool yyforceEval, cypher_parser_context *context)
{
  YYPTRDIFF_T yyposn = yystackp->yytops.yystates[yyk]->yyposn;

  if (yyforceEval || yystackp->yysplitPoint == YY_NULLPTR)
    {
      YYSTYPE yyval;
      YYLTYPE yyloc;

      YYRESULTTAG yyflag = yydoAction (yystackp, yyk, yyrule, &yyval, &yyloc, context);
      if (yyflag == yyerr && yystackp->yysplitPoint != YY_NULLPTR)
        YY_DPRINTF ((stderr,
                     "Parse on stack %ld rejected by rule %d (line %d).\n",
                     YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule]));
      if (yyflag != yyok)
        return yyflag;
      yyglrShift (yystackp, yyk,
                  yyLRgotoState (yystackp->yytops.yystates[yyk]->yylrState,
                                 yylhsNonterm (yyrule)),
                  yyposn, &yyval, &yyloc);
    }
  else
    {
      YYPTRDIFF_T yyi;
      int yyn;
      yyGLRState* yys, *yys0 = yystackp->yytops.yystates[yyk];
      yy_state_t yynewLRState;

      for (yys = yystackp->yytops.yystates[yyk], yyn = yyrhsLength (yyrule);
           0 < yyn; yyn -= 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yynewLRState = yyLRgotoState (yys->yylrState, yylhsNonterm (yyrule));
      YY_DPRINTF ((stderr,
                   "Reduced stack %ld by rule %d (line %d); action deferred.  "
                   "Now in state %d.\n",
                   YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule],
                   yynewLRState));
      for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
        if (yyi != yyk && yystackp->yytops.yystates[yyi] != YY_NULLPTR)
          {
            yyGLRState *yysplit = yystackp->yysplitPoint;
            yyGLRState *yyp = yystackp->yytops.yystates[yyi];
            while (yyp != yys && yyp != yysplit && yyp->yyposn >= yyposn)
              {
                if (yyp->yylrState == yynewLRState && yyp->yypred == yys)
                  {
                    yyaddDeferredAction (yystackp, yyk, yyp, yys0, yyrule);
                    yymarkStackDeleted (yystackp, yyk);
                    YY_DPRINTF ((stderr, "Merging stack %ld into stack %ld.\n",
                                 YY_CAST (long, yyk), YY_CAST (long, yyi)));
                    return yyok;
                  }
                yyp = yyp->yypred;
              }
          }
      yystackp->yytops.yystates[yyk] = yys;
      yyglrShiftDefer (yystackp, yyk, yynewLRState, yyposn, yys0, yyrule);
    }
  return yyok;
}

static YYPTRDIFF_T
yysplitStack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      YY_ASSERT (yyk == 0);
      yystackp->yysplitPoint = yystackp->yytops.yystates[yyk];
    }
  if (yystackp->yytops.yycapacity <= yystackp->yytops.yysize)
    {
      YYPTRDIFF_T state_size = YYSIZEOF (yystackp->yytops.yystates[0]);
      YYPTRDIFF_T half_max_capacity = YYSIZE_MAXIMUM / 2 / state_size;
      if (half_max_capacity < yystackp->yytops.yycapacity)
        yyMemoryExhausted (yystackp);
      yystackp->yytops.yycapacity *= 2;

      {
        yyGLRState** yynewStates
          = YY_CAST (yyGLRState**,
                     YYREALLOC (yystackp->yytops.yystates,
                                (YY_CAST (YYSIZE_T, yystackp->yytops.yycapacity)
                                 * sizeof yynewStates[0])));
        if (yynewStates == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yystates = yynewStates;
      }

      {
        yybool* yynewLookaheadNeeds
          = YY_CAST (yybool*,
                     YYREALLOC (yystackp->yytops.yylookaheadNeeds,
                                (YY_CAST (YYSIZE_T, yystackp->yytops.yycapacity)
                                 * sizeof yynewLookaheadNeeds[0])));
        if (yynewLookaheadNeeds == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yylookaheadNeeds = yynewLookaheadNeeds;
      }
    }
  yystackp->yytops.yystates[yystackp->yytops.yysize]
    = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yylookaheadNeeds[yystackp->yytops.yysize]
    = yystackp->yytops.yylookaheadNeeds[yyk];
  yystackp->yytops.yysize += 1;
  return yystackp->yytops.yysize - 1;
}

/** True iff YYY0 and YYY1 represent identical options at the top level.
 *  That is, they represent the same rule applied to RHS symbols
 *  that produce the same terminal symbols.  */
static yybool
yyidenticalOptions (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  if (yyy0->yyrule == yyy1->yyrule)
    {
      yyGLRState *yys0, *yys1;
      int yyn;
      for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
           yyn = yyrhsLength (yyy0->yyrule);
           yyn > 0;
           yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
        if (yys0->yyposn != yys1->yyposn)
          return yyfalse;
      return yytrue;
    }
  else
    return yyfalse;
}

/** Assuming identicalOptions (YYY0,YYY1), destructively merge the
 *  alternative semantic values for the RHS-symbols of YYY1 and YYY0.  */
static void
yymergeOptionSets (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  yyGLRState *yys0, *yys1;
  int yyn;
  for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
       yyn = yyrhsLength (yyy0->yyrule);
       0 < yyn;
       yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
    {
      if (yys0 == yys1)
        break;
      else if (yys0->yyresolved)
        {
          yys1->yyresolved = yytrue;
          yys1->yysemantics.yyval = yys0->yysemantics.yyval;
        }
      else if (yys1->yyresolved)
        {
          yys0->yyresolved = yytrue;
          yys0->yysemantics.yyval = yys1->yysemantics.yyval;
        }
      else
        {
          yySemanticOption** yyz0p = &yys0->yysemantics.yyfirstVal;
          yySemanticOption* yyz1 = yys1->yysemantics.yyfirstVal;
          while (yytrue)
            {
              if (yyz1 == *yyz0p || yyz1 == YY_NULLPTR)
                break;
              else if (*yyz0p == YY_NULLPTR)
                {
                  *yyz0p = yyz1;
                  break;
                }
              else if (*yyz0p < yyz1)
                {
                  yySemanticOption* yyz = *yyz0p;
                  *yyz0p = yyz1;
                  yyz1 = yyz1->yynext;
                  (*yyz0p)->yynext = yyz;
                }
              yyz0p = &(*yyz0p)->yynext;
            }
          yys1->yysemantics.yyfirstVal = yys0->yysemantics.yyfirstVal;
        }
    }
}

/** Y0 and Y1 represent two possible actions to take in a given
 *  parsing state; return 0 if no combination is possible,
 *  1 if user-mergeable, 2 if Y0 is preferred, 3 if Y1 is preferred.  */
static int
yypreference (yySemanticOption* y0, yySemanticOption* y1)
{
  yyRuleNum r0 = y0->yyrule, r1 = y1->yyrule;
  int p0 = yydprec[r0], p1 = yydprec[r1];

  if (p0 == p1)
    {
      if (yymerger[r0] == 0 || yymerger[r0] != yymerger[r1])
        return 0;
      else
        return 1;
    }
  if (p0 == 0 || p1 == 0)
    return 0;
  if (p0 < p1)
    return 3;
  if (p1 < p0)
    return 2;
  return 0;
}

static YYRESULTTAG
yyresolveValue (yyGLRState* yys, yyGLRStack* yystackp, cypher_parser_context *context);


/** Resolve the previous YYN states starting at and including state YYS
 *  on *YYSTACKP. If result != yyok, some states may have been left
 *  unresolved possibly with empty semantic option chains.  Regardless
 *  of whether result = yyok, each state has been left with consistent
 *  data so that yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveStates (yyGLRState* yys, int yyn,
                 yyGLRStack* yystackp, cypher_parser_context *context)
{
  if (0 < yyn)
    {
      YY_ASSERT (yys->yypred);
      YYCHK (yyresolveStates (yys->yypred, yyn-1, yystackp, context));
      if (! yys->yyresolved)
        YYCHK (yyresolveValue (yys, yystackp, context));
    }
  return yyok;
}

/** Resolve the states for the RHS of YYOPT on *YYSTACKP, perform its
 *  user action, and return the semantic value and location in *YYVALP
 *  and *YYLOCP.  Regardless of whether result = yyok, all RHS states
 *  have been destroyed (assuming the user action destroys all RHS
 *  semantic values if invoked).  */
static YYRESULTTAG
yyresolveAction (yySemanticOption* yyopt, yyGLRStack* yystackp,
                 YYSTYPE* yyvalp, YYLTYPE *yylocp, cypher_parser_context *context)
{
  yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
  int yynrhs = yyrhsLength (yyopt->yyrule);
  YYRESULTTAG yyflag =
    yyresolveStates (yyopt->yystate, yynrhs, yystackp, context);
  if (yyflag != yyok)
    {
      yyGLRState *yys;
      for (yys = yyopt->yystate; yynrhs > 0; yys = yys->yypred, yynrhs -= 1)
        yydestroyGLRState ("Cleanup: popping", yys, context);
      return yyflag;
    }

  yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred = yyopt->yystate;
  if (yynrhs == 0)
    /* Set default location.  */
    yyrhsVals[YYMAXRHS + YYMAXLEFT - 1].yystate.yyloc = yyopt->yystate->yyloc;
  {
    int yychar_current = yychar;
    YYSTYPE yylval_current = yylval;
    YYLTYPE yylloc_current = yylloc;
    yychar = yyopt->yyrawchar;
    yylval = yyopt->yyval;
    yylloc = yyopt->yyloc;
    yyflag = yyuserAction (yyopt->yyrule, yynrhs,
                           yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, -1, yyvalp, yylocp, context);
    yychar = yychar_current;
    yylval = yylval_current;
    yylloc = yylloc_current;
  }
  return yyflag;
}

#if CYPHER_YYDEBUG
static void
yyreportTree (yySemanticOption* yyx, int yyindent)
{
  int yynrhs = yyrhsLength (yyx->yyrule);
  int yyi;
  yyGLRState* yys;
  yyGLRState* yystates[1 + YYMAXRHS];
  yyGLRState yyleftmost_state;

  for (yyi = yynrhs, yys = yyx->yystate; 0 < yyi; yyi -= 1, yys = yys->yypred)
    yystates[yyi] = yys;
  if (yys == YY_NULLPTR)
    {
      yyleftmost_state.yyposn = 0;
      yystates[0] = &yyleftmost_state;
    }
  else
    yystates[0] = yys;

  if (yyx->yystate->yyposn < yys->yyposn + 1)
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, empty>\n",
                 yyindent, "", yysymbol_name (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1));
  else
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, tokens %ld .. %ld>\n",
                 yyindent, "", yysymbol_name (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1, YY_CAST (long, yys->yyposn + 1),
                 YY_CAST (long, yyx->yystate->yyposn)));
  for (yyi = 1; yyi <= yynrhs; yyi += 1)
    {
      if (yystates[yyi]->yyresolved)
        {
          if (yystates[yyi-1]->yyposn+1 > yystates[yyi]->yyposn)
            YY_FPRINTF ((stderr, "%*s%s <empty>\n", yyindent+2, "",
                         yysymbol_name (yy_accessing_symbol (yystates[yyi]->yylrState))));
          else
            YY_FPRINTF ((stderr, "%*s%s <tokens %ld .. %ld>\n", yyindent+2, "",
                         yysymbol_name (yy_accessing_symbol (yystates[yyi]->yylrState)),
                         YY_CAST (long, yystates[yyi-1]->yyposn + 1),
                         YY_CAST (long, yystates[yyi]->yyposn)));
        }
      else
        yyreportTree (yystates[yyi]->yysemantics.yyfirstVal, yyindent+2);
    }
}
#endif

static YYRESULTTAG
yyreportAmbiguity (yySemanticOption* yyx0,
                   yySemanticOption* yyx1, YYLTYPE *yylocp, cypher_parser_context *context)
{
  YY_USE (yyx0);
  YY_USE (yyx1);

#if CYPHER_YYDEBUG
  YY_FPRINTF ((stderr, "Ambiguity detected.\n"));
  YY_FPRINTF ((stderr, "Option 1,\n"));
  yyreportTree (yyx0, 2);
  YY_FPRINTF ((stderr, "\nOption 2,\n"));
  yyreportTree (yyx1, 2);
  YY_FPRINTF ((stderr, "\n"));
#endif

  yyerror (yylocp, context, YY_("syntax is ambiguous"));
  return yyabort;
}

/** Resolve the locations for each of the YYN1 states in *YYSTACKP,
 *  ending at YYS1.  Has no effect on previously resolved states.
 *  The first semantic option of a state is always chosen.  */
static void
yyresolveLocations (yyGLRState *yys1, int yyn1,
                    yyGLRStack *yystackp, cypher_parser_context *context)
{
  if (0 < yyn1)
    {
      yyresolveLocations (yys1->yypred, yyn1 - 1, yystackp, context);
      if (!yys1->yyresolved)
        {
          yyGLRStackItem yyrhsloc[1 + YYMAXRHS];
          int yynrhs;
          yySemanticOption *yyoption = yys1->yysemantics.yyfirstVal;
          YY_ASSERT (yyoption);
          yynrhs = yyrhsLength (yyoption->yyrule);
          if (0 < yynrhs)
            {
              yyGLRState *yys;
              int yyn;
              yyresolveLocations (yyoption->yystate, yynrhs,
                                  yystackp, context);
              for (yys = yyoption->yystate, yyn = yynrhs;
                   yyn > 0;
                   yys = yys->yypred, yyn -= 1)
                yyrhsloc[yyn].yystate.yyloc = yys->yyloc;
            }
          else
            {
              /* Both yyresolveAction and yyresolveLocations traverse the GSS
                 in reverse rightmost order.  It is only necessary to invoke
                 yyresolveLocations on a subforest for which yyresolveAction
                 would have been invoked next had an ambiguity not been
                 detected.  Thus the location of the previous state (but not
                 necessarily the previous state itself) is guaranteed to be
                 resolved already.  */
              yyGLRState *yyprevious = yyoption->yystate;
              yyrhsloc[0].yystate.yyloc = yyprevious->yyloc;
            }
          YYLLOC_DEFAULT ((yys1->yyloc), yyrhsloc, yynrhs);
        }
    }
}

/** Resolve the ambiguity represented in state YYS in *YYSTACKP,
 *  perform the indicated actions, and set the semantic value of YYS.
 *  If result != yyok, the chain of semantic options in YYS has been
 *  cleared instead or it has been left unmodified except that
 *  redundant options may have been removed.  Regardless of whether
 *  result = yyok, YYS has been left with consistent data so that
 *  yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveValue (yyGLRState* yys, yyGLRStack* yystackp, cypher_parser_context *context)
{
  yySemanticOption* yyoptionList = yys->yysemantics.yyfirstVal;
  yySemanticOption* yybest = yyoptionList;
  yySemanticOption** yypp;
  yybool yymerge = yyfalse;
  YYSTYPE yyval;
  YYRESULTTAG yyflag;
  YYLTYPE *yylocp = &yys->yyloc;

  for (yypp = &yyoptionList->yynext; *yypp != YY_NULLPTR; )
    {
      yySemanticOption* yyp = *yypp;

      if (yyidenticalOptions (yybest, yyp))
        {
          yymergeOptionSets (yybest, yyp);
          *yypp = yyp->yynext;
        }
      else
        {
          switch (yypreference (yybest, yyp))
            {
            case 0:
              yyresolveLocations (yys, 1, yystackp, context);
              return yyreportAmbiguity (yybest, yyp, yylocp, context);
              break;
            case 1:
              yymerge = yytrue;
              break;
            case 2:
              break;
            case 3:
              yybest = yyp;
              yymerge = yyfalse;
              break;
            default:
              /* This cannot happen so it is not worth a YY_ASSERT (yyfalse),
                 but some compilers complain if the default case is
                 omitted.  */
              break;
            }
          yypp = &yyp->yynext;
        }
    }

  if (yymerge)
    {
      yySemanticOption* yyp;
      int yyprec = yydprec[yybest->yyrule];
      yyflag = yyresolveAction (yybest, yystackp, &yyval, yylocp, context);
      if (yyflag == yyok)
        for (yyp = yybest->yynext; yyp != YY_NULLPTR; yyp = yyp->yynext)
          {
            if (yyprec == yydprec[yyp->yyrule])
              {
                YYSTYPE yyval_other;
                YYLTYPE yydummy;
                yyflag = yyresolveAction (yyp, yystackp, &yyval_other, &yydummy, context);
                if (yyflag != yyok)
                  {
                    yydestruct ("Cleanup: discarding incompletely merged value for",
                                yy_accessing_symbol (yys->yylrState),
                                &yyval, yylocp, context);
                    break;
                  }
                yyuserMerge (yymerger[yyp->yyrule], &yyval, &yyval_other);
              }
          }
    }
  else
    yyflag = yyresolveAction (yybest, yystackp, &yyval, yylocp, context);

  if (yyflag == yyok)
    {
      yys->yyresolved = yytrue;
      yys->yysemantics.yyval = yyval;
    }
  else
    yys->yysemantics.yyfirstVal = YY_NULLPTR;
  return yyflag;
}

static YYRESULTTAG
yyresolveStack (yyGLRStack* yystackp, cypher_parser_context *context)
{
  if (yystackp->yysplitPoint != YY_NULLPTR)
    {
      yyGLRState* yys;
      int yyn;

      for (yyn = 0, yys = yystackp->yytops.yystates[0];
           yys != yystackp->yysplitPoint;
           yys = yys->yypred, yyn += 1)
        continue;
      YYCHK (yyresolveStates (yystackp->yytops.yystates[0], yyn, yystackp
                             , context));
    }
  return yyok;
}

/** Called when returning to deterministic operation to clean up the extra
 * stacks. */
static void
yycompressStack (yyGLRStack* yystackp)
{
  /* yyr is the state after the split point.  */
  yyGLRState *yyr;

  if (yystackp->yytops.yysize != 1 || yystackp->yysplitPoint == YY_NULLPTR)
    return;

  {
    yyGLRState *yyp, *yyq;
    for (yyp = yystackp->yytops.yystates[0], yyq = yyp->yypred, yyr = YY_NULLPTR;
         yyp != yystackp->yysplitPoint;
         yyr = yyp, yyp = yyq, yyq = yyp->yypred)
      yyp->yypred = yyr;
  }

  yystackp->yyspaceLeft += yystackp->yynextFree - yystackp->yyitems;
  yystackp->yynextFree = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yysplitPoint) + 1;
  yystackp->yyspaceLeft -= yystackp->yynextFree - yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;

  while (yyr != YY_NULLPTR)
    {
      yystackp->yynextFree->yystate = *yyr;
      yyr = yyr->yypred;
      yystackp->yynextFree->yystate.yypred = &yystackp->yynextFree[-1].yystate;
      yystackp->yytops.yystates[0] = &yystackp->yynextFree->yystate;
      yystackp->yynextFree += 1;
      yystackp->yyspaceLeft -= 1;
    }
}

static YYRESULTTAG
yyprocessOneStack (yyGLRStack* yystackp, YYPTRDIFF_T yyk,
                   YYPTRDIFF_T yyposn, YYLTYPE *yylocp, cypher_parser_context *context)
{
  while (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    {
      yy_state_t yystate = yystackp->yytops.yystates[yyk]->yylrState;
      YY_DPRINTF ((stderr, "Stack %ld Entering state %d\n",
                   YY_CAST (long, yyk), yystate));

      YY_ASSERT (yystate != YYFINAL);

      if (yyisDefaultedState (yystate))
        {
          YYRESULTTAG yyflag;
          yyRuleNum yyrule = yydefaultAction (yystate);
          if (yyrule == 0)
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          yyflag = yyglrReduce (yystackp, yyk, yyrule, yyimmediate[yyrule], context);
          if (yyflag == yyerr)
            {
              YY_DPRINTF ((stderr,
                           "Stack %ld dies "
                           "(predicate failure or explicit user error).\n",
                           YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          if (yyflag != yyok)
            return yyflag;
        }
      else
        {
          yysymbol_kind_t yytoken = yygetToken (&yychar, yystackp, context);
          const short* yyconflicts;
          const int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
          yystackp->yytops.yylookaheadNeeds[yyk] = yytrue;

          for (/* nothing */; *yyconflicts; yyconflicts += 1)
            {
              YYRESULTTAG yyflag;
              YYPTRDIFF_T yynewStack = yysplitStack (yystackp, yyk);
              YY_DPRINTF ((stderr, "Splitting off stack %ld from %ld.\n",
                           YY_CAST (long, yynewStack), YY_CAST (long, yyk)));
              yyflag = yyglrReduce (yystackp, yynewStack,
                                    *yyconflicts,
                                    yyimmediate[*yyconflicts], context);
              if (yyflag == yyok)
                YYCHK (yyprocessOneStack (yystackp, yynewStack,
                                          yyposn, yylocp, context));
              else if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yynewStack)));
                  yymarkStackDeleted (yystackp, yynewStack);
                }
              else
                return yyflag;
            }

          if (yyisShiftAction (yyaction))
            break;
          else if (yyisErrorAction (yyaction))
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              break;
            }
          else
            {
              YYRESULTTAG yyflag = yyglrReduce (yystackp, yyk, -yyaction,
                                                yyimmediate[-yyaction], context);
              if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr,
                               "Stack %ld dies "
                               "(predicate failure or explicit user error).\n",
                               YY_CAST (long, yyk)));
                  yymarkStackDeleted (yystackp, yyk);
                  break;
                }
              else if (yyflag != yyok)
                return yyflag;
            }
        }
    }
  return yyok;
}

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYSTACKP, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  */
static int
yypcontext_expected_tokens (const yyGLRStack* yystackp,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[yystackp->yytops.yystates[0]->yylrState];
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

static int
yy_syntax_error_arguments (const yyGLRStack* yystackp,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  yysymbol_kind_t yytoken = yychar == CYPHER_CYPHER_YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
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
  if (yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yystackp,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}



static void
yyreportSyntaxError (yyGLRStack* yystackp, cypher_parser_context *context)
{
  if (yystackp->yyerrState != 0)
    return;
  {
  yybool yysize_overflow = yyfalse;
  char* yymsg = YY_NULLPTR;
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount
    = yy_syntax_error_arguments (yystackp, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    yyMemoryExhausted (yystackp);

  switch (yycount)
    {
#define YYCASE_(N, S)                   \
      case N:                           \
        yyformat = S;                   \
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
        YYPTRDIFF_T yysz
          = yystrlen (yysymbol_name (yyarg[yyi]));
        if (YYSIZE_MAXIMUM - yysize < yysz)
          yysize_overflow = yytrue;
        else
          yysize += yysz;
      }
  }

  if (!yysize_overflow)
    yymsg = YY_CAST (char *, YYMALLOC (YY_CAST (YYSIZE_T, yysize)));

  if (yymsg)
    {
      char *yyp = yymsg;
      int yyi = 0;
      while ((*yyp = *yyformat))
        {
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
      yyerror (&yylloc, context, yymsg);
      YYFREE (yymsg);
    }
  else
    {
      yyerror (&yylloc, context, YY_("syntax error"));
      yyMemoryExhausted (yystackp);
    }
  }
  yynerrs += 1;
}

/* Recover from a syntax error on *YYSTACKP, assuming that *YYSTACKP->YYTOKENP,
   yylval, and yylloc are the syntactic category, semantic value, and location
   of the lookahead.  */
static void
yyrecoverSyntaxError (yyGLRStack* yystackp, cypher_parser_context *context)
{
  if (yystackp->yyerrState == 3)
    /* We just shifted the error token and (perhaps) took some
       reductions.  Skip tokens until we can proceed.  */
    while (yytrue)
      {
        yysymbol_kind_t yytoken;
        int yyj;
        if (yychar == CYPHER_YYEOF)
          yyFail (yystackp, &yylloc, context, YY_NULLPTR);
        if (yychar != CYPHER_CYPHER_YYEMPTY)
          {
            /* We throw away the lookahead, but the error range
               of the shifted error token must take it into account.  */
            yyGLRState *yys = yystackp->yytops.yystates[0];
            yyGLRStackItem yyerror_range[3];
            yyerror_range[1].yystate.yyloc = yys->yyloc;
            yyerror_range[2].yystate.yyloc = yylloc;
            YYLLOC_DEFAULT ((yys->yyloc), yyerror_range, 2);
            yytoken = YYTRANSLATE (yychar);
            yydestruct ("Error: discarding",
                        yytoken, &yylval, &yylloc, context);
            yychar = CYPHER_CYPHER_YYEMPTY;
          }
        yytoken = yygetToken (&yychar, yystackp, context);
        yyj = yypact[yystackp->yytops.yystates[0]->yylrState];
        if (yypact_value_is_default (yyj))
          return;
        yyj += yytoken;
        if (yyj < 0 || YYLAST < yyj || yycheck[yyj] != yytoken)
          {
            if (yydefact[yystackp->yytops.yystates[0]->yylrState] != 0)
              return;
          }
        else if (! yytable_value_is_error (yytable[yyj]))
          return;
      }

  /* Reduce to one stack.  */
  {
    YYPTRDIFF_T yyk;
    for (yyk = 0; yyk < yystackp->yytops.yysize; yyk += 1)
      if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
        break;
    if (yyk >= yystackp->yytops.yysize)
      yyFail (yystackp, &yylloc, context, YY_NULLPTR);
    for (yyk += 1; yyk < yystackp->yytops.yysize; yyk += 1)
      yymarkStackDeleted (yystackp, yyk);
    yyremoveDeletes (yystackp);
    yycompressStack (yystackp);
  }

  /* Pop stack until we find a state that shifts the error token.  */
  yystackp->yyerrState = 3;
  while (yystackp->yytops.yystates[0] != YY_NULLPTR)
    {
      yyGLRState *yys = yystackp->yytops.yystates[0];
      int yyj = yypact[yys->yylrState];
      if (! yypact_value_is_default (yyj))
        {
          yyj += YYSYMBOL_YYerror;
          if (0 <= yyj && yyj <= YYLAST && yycheck[yyj] == YYSYMBOL_YYerror
              && yyisShiftAction (yytable[yyj]))
            {
              /* Shift the error token.  */
              int yyaction = yytable[yyj];
              /* First adjust its location.*/
              YYLTYPE yyerrloc;
              yystackp->yyerror_range[2].yystate.yyloc = yylloc;
              YYLLOC_DEFAULT (yyerrloc, (yystackp->yyerror_range), 2);
              YY_SYMBOL_PRINT ("Shifting", yy_accessing_symbol (yyaction),
                               &yylval, &yyerrloc);
              yyglrShift (yystackp, 0, yyaction,
                          yys->yyposn, &yylval, &yyerrloc);
              yys = yystackp->yytops.yystates[0];
              break;
            }
        }
      yystackp->yyerror_range[1].yystate.yyloc = yys->yyloc;
      if (yys->yypred != YY_NULLPTR)
        yydestroyGLRState ("Error: popping", yys, context);
      yystackp->yytops.yystates[0] = yys->yypred;
      yystackp->yynextFree -= 1;
      yystackp->yyspaceLeft += 1;
    }
  if (yystackp->yytops.yystates[0] == YY_NULLPTR)
    yyFail (yystackp, &yylloc, context, YY_NULLPTR);
}

#define YYCHK1(YYE)                             \
  do {                                          \
    switch (YYE) {                              \
    case yyok:     break;                       \
    case yyabort:  goto yyabortlab;             \
    case yyaccept: goto yyacceptlab;            \
    case yyerr:    goto yyuser_error;           \
    case yynomem:  goto yyexhaustedlab;         \
    default:       goto yybuglab;               \
    }                                           \
  } while (0)

/*----------.
| yyparse.  |
`----------*/

int
yyparse (cypher_parser_context *context)
{
  int yyresult;
  yyGLRStack yystack;
  yyGLRStack* const yystackp = &yystack;
  YYPTRDIFF_T yyposn;

  YY_DPRINTF ((stderr, "Starting parse\n"));

  yychar = CYPHER_CYPHER_YYEMPTY;
  yylval = yyval_default;
  yylloc = yyloc_default;

  if (! yyinitGLRStack (yystackp, YYINITDEPTH))
    goto yyexhaustedlab;
  switch (YYSETJMP (yystack.yyexception_buffer))
    {
    case 0: break;
    case 1: goto yyabortlab;
    case 2: goto yyexhaustedlab;
    default: goto yybuglab;
    }
  yyglrShift (&yystack, 0, 0, 0, &yylval, &yylloc);
  yyposn = 0;

  while (yytrue)
    {
      /* For efficiency, we have two loops, the first of which is
         specialized to deterministic operation (single stack, no
         potential ambiguity).  */
      /* Standard mode. */
      while (yytrue)
        {
          yy_state_t yystate = yystack.yytops.yystates[0]->yylrState;
          YY_DPRINTF ((stderr, "Entering state %d\n", yystate));
          if (yystate == YYFINAL)
            goto yyacceptlab;
          if (yyisDefaultedState (yystate))
            {
              yyRuleNum yyrule = yydefaultAction (yystate);
              if (yyrule == 0)
                {
                  yystack.yyerror_range[1].yystate.yyloc = yylloc;
                  yyreportSyntaxError (&yystack, context);
                  goto yyuser_error;
                }
              YYCHK1 (yyglrReduce (&yystack, 0, yyrule, yytrue, context));
            }
          else
            {
              yysymbol_kind_t yytoken = yygetToken (&yychar, yystackp, context);
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
              if (*yyconflicts)
                /* Enter nondeterministic mode.  */
                break;
              if (yyisShiftAction (yyaction))
                {
                  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
                  yychar = CYPHER_CYPHER_YYEMPTY;
                  yyposn += 1;
                  yyglrShift (&yystack, 0, yyaction, yyposn, &yylval, &yylloc);
                  if (0 < yystack.yyerrState)
                    yystack.yyerrState -= 1;
                }
              else if (yyisErrorAction (yyaction))
                {
                  yystack.yyerror_range[1].yystate.yyloc = yylloc;
                  /* Issue an error message unless the scanner already
                     did. */
                  if (yychar != CYPHER_CYPHER_YYerror)
                    yyreportSyntaxError (&yystack, context);
                  goto yyuser_error;
                }
              else
                YYCHK1 (yyglrReduce (&yystack, 0, -yyaction, yytrue, context));
            }
        }

      /* Nondeterministic mode. */
      while (yytrue)
        {
          yysymbol_kind_t yytoken_to_shift;
          YYPTRDIFF_T yys;

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            yystackp->yytops.yylookaheadNeeds[yys] = yychar != CYPHER_CYPHER_YYEMPTY;

          /* yyprocessOneStack returns one of three things:

              - An error flag.  If the caller is yyprocessOneStack, it
                immediately returns as well.  When the caller is finally
                yyparse, it jumps to an error label via YYCHK1.

              - yyok, but yyprocessOneStack has invoked yymarkStackDeleted
                (&yystack, yys), which sets the top state of yys to NULL.  Thus,
                yyparse's following invocation of yyremoveDeletes will remove
                the stack.

              - yyok, when ready to shift a token.

             Except in the first case, yyparse will invoke yyremoveDeletes and
             then shift the next token onto all remaining stacks.  This
             synchronization of the shift (that is, after all preceding
             reductions on all stacks) helps prevent double destructor calls
             on yylval in the event of memory exhaustion.  */

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            YYCHK1 (yyprocessOneStack (&yystack, yys, yyposn, &yylloc, context));
          yyremoveDeletes (&yystack);
          if (yystack.yytops.yysize == 0)
            {
              yyundeleteLastStack (&yystack);
              if (yystack.yytops.yysize == 0)
                yyFail (&yystack, &yylloc, context, YY_("syntax error"));
              YYCHK1 (yyresolveStack (&yystack, context));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yystack.yyerror_range[1].yystate.yyloc = yylloc;
              yyreportSyntaxError (&yystack, context);
              goto yyuser_error;
            }

          /* If any yyglrShift call fails, it will fail after shifting.  Thus,
             a copy of yylval will already be on stack 0 in the event of a
             failure in the following loop.  Thus, yychar is set to CYPHER_CYPHER_YYEMPTY
             before the loop to make sure the user destructor for yylval isn't
             called twice.  */
          yytoken_to_shift = YYTRANSLATE (yychar);
          yychar = CYPHER_CYPHER_YYEMPTY;
          yyposn += 1;
          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            {
              yy_state_t yystate = yystack.yytops.yystates[yys]->yylrState;
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken_to_shift,
                              &yyconflicts);
              /* Note that yyconflicts were handled by yyprocessOneStack.  */
              YY_DPRINTF ((stderr, "On stack %ld, ", YY_CAST (long, yys)));
              YY_SYMBOL_PRINT ("shifting", yytoken_to_shift, &yylval, &yylloc);
              yyglrShift (&yystack, yys, yyaction, yyposn,
                          &yylval, &yylloc);
              YY_DPRINTF ((stderr, "Stack %ld now in state %d\n",
                           YY_CAST (long, yys),
                           yystack.yytops.yystates[yys]->yylrState));
            }

          if (yystack.yytops.yysize == 1)
            {
              YYCHK1 (yyresolveStack (&yystack, context));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yycompressStack (&yystack);
              break;
            }
        }
      continue;
    yyuser_error:
      yyrecoverSyntaxError (&yystack, context);
      yyposn = yystack.yytops.yystates[0]->yyposn;
    }

 yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;

 yybuglab:
  YY_ASSERT (yyfalse);
  goto yyabortlab;

 yyabortlab:
  yyresult = 1;
  goto yyreturnlab;

 yyexhaustedlab:
  yyerror (&yylloc, context, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;

 yyreturnlab:
  if (yychar != CYPHER_CYPHER_YYEMPTY)
    yydestruct ("Cleanup: discarding lookahead",
                YYTRANSLATE (yychar), &yylval, &yylloc, context);

  /* If the stack is well-formed, pop the stack until it is empty,
     destroying its entries as we go.  But free the stack regardless
     of whether it is well-formed.  */
  if (yystack.yyitems)
    {
      yyGLRState** yystates = yystack.yytops.yystates;
      if (yystates)
        {
          YYPTRDIFF_T yysize = yystack.yytops.yysize;
          YYPTRDIFF_T yyk;
          for (yyk = 0; yyk < yysize; yyk += 1)
            if (yystates[yyk])
              {
                while (yystates[yyk])
                  {
                    yyGLRState *yys = yystates[yyk];
                    yystack.yyerror_range[1].yystate.yyloc = yys->yyloc;
                    if (yys->yypred != YY_NULLPTR)
                      yydestroyGLRState ("Cleanup: popping", yys, context);
                    yystates[yyk] = yys->yypred;
                    yystack.yynextFree -= 1;
                    yystack.yyspaceLeft += 1;
                  }
                break;
              }
        }
      yyfreeGLRStack (&yystack);
    }

  return yyresult;
}

/* DEBUGGING ONLY */
#if CYPHER_YYDEBUG
/* Print *YYS and its predecessors. */
static void
yy_yypstack (yyGLRState* yys)
{
  if (yys->yypred)
    {
      yy_yypstack (yys->yypred);
      YY_FPRINTF ((stderr, " -> "));
    }
  YY_FPRINTF ((stderr, "%d@%ld", yys->yylrState, YY_CAST (long, yys->yyposn)));
}

/* Print YYS (possibly NULL) and its predecessors. */
static void
yypstates (yyGLRState* yys)
{
  if (yys == YY_NULLPTR)
    YY_FPRINTF ((stderr, "<null>"));
  else
    yy_yypstack (yys);
  YY_FPRINTF ((stderr, "\n"));
}

/* Print the stack #YYK.  */
static void
yypstack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  yypstates (yystackp->yytops.yystates[yyk]);
}

/* Print all the stacks.  */
static void
yypdumpstack (yyGLRStack* yystackp)
{
#define YYINDEX(YYX)                                                    \
  YY_CAST (long,                                                        \
           ((YYX)                                                       \
            ? YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX)) - yystackp->yyitems \
            : -1))

  yyGLRStackItem* yyp;
  for (yyp = yystackp->yyitems; yyp < yystackp->yynextFree; yyp += 1)
    {
      YY_FPRINTF ((stderr, "%3ld. ",
                   YY_CAST (long, yyp - yystackp->yyitems)));
      if (*YY_REINTERPRET_CAST (yybool *, yyp))
        {
          YY_ASSERT (yyp->yystate.yyisState);
          YY_ASSERT (yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Res: %d, LR State: %d, posn: %ld, pred: %ld",
                       yyp->yystate.yyresolved, yyp->yystate.yylrState,
                       YY_CAST (long, yyp->yystate.yyposn),
                       YYINDEX (yyp->yystate.yypred)));
          if (! yyp->yystate.yyresolved)
            YY_FPRINTF ((stderr, ", firstVal: %ld",
                         YYINDEX (yyp->yystate.yysemantics.yyfirstVal)));
        }
      else
        {
          YY_ASSERT (!yyp->yystate.yyisState);
          YY_ASSERT (!yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Option. rule: %d, state: %ld, next: %ld",
                       yyp->yyoption.yyrule - 1,
                       YYINDEX (yyp->yyoption.yystate),
                       YYINDEX (yyp->yyoption.yynext)));
        }
      YY_FPRINTF ((stderr, "\n"));
    }

  YY_FPRINTF ((stderr, "Tops:"));
  {
    YYPTRDIFF_T yyi;
    for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
      YY_FPRINTF ((stderr, "%ld: %ld; ", YY_CAST (long, yyi),
                   YYINDEX (yystackp->yytops.yystates[yyi])));
    YY_FPRINTF ((stderr, "\n"));
  }
#undef YYINDEX
}
#endif

#undef yylval
#undef yychar
#undef yynerrs
#undef yylloc

/* Substitute the variable and function names.  */
#define yyparse cypher_yyparse
#define yylex   cypher_yylex
#define yyerror cypher_yyerror
#define yylval  cypher_yylval
#define yychar  cypher_yychar
#define yydebug cypher_yydebug
#define yynerrs cypher_yynerrs
#define yylloc  cypher_yylloc


#line 1481 "src/backend/parser/cypher_gram.y"


/* Error handling function */
void cypher_yyerror(CYPHER_YYLTYPE *yylloc, cypher_parser_context *context, const char *msg)
{
    if (!context || !msg) {
        return;
    }

    context->has_error = true;
    context->error_location = yylloc ? yylloc->first_line : -1;

    /* Create error message with line number - Bison's detailed error mode
     * provides good context about what was expected */
    char error_buffer[512];
    if (yylloc && yylloc->first_line > 0) {
        snprintf(error_buffer, sizeof(error_buffer),
                 "Line %d: %s", yylloc->first_line, msg);
    } else {
        snprintf(error_buffer, sizeof(error_buffer), "%s", msg);
    }

    free(context->error_message);
    context->error_message = strdup(error_buffer);
}

/* cypher_yylex function is implemented in cypher_parser.c */

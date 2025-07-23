/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_CYPHER_TAB_H_INCLUDED
# define YY_YY_CYPHER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    IDENTIFIER = 258,              /* IDENTIFIER  */
    STRING_LITERAL = 259,          /* STRING_LITERAL  */
    INTEGER_LITERAL = 260,         /* INTEGER_LITERAL  */
    FLOAT_LITERAL = 261,           /* FLOAT_LITERAL  */
    CREATE = 262,                  /* CREATE  */
    MATCH = 263,                   /* MATCH  */
    RETURN = 264,                  /* RETURN  */
    TRUE = 265,                    /* TRUE  */
    FALSE = 266,                   /* FALSE  */
    LPAREN = 267,                  /* LPAREN  */
    RPAREN = 268,                  /* RPAREN  */
    LBRACE = 269,                  /* LBRACE  */
    RBRACE = 270,                  /* RBRACE  */
    LBRACKET = 271,                /* LBRACKET  */
    RBRACKET = 272,                /* RBRACKET  */
    DOT = 273,                     /* DOT  */
    COMMA = 274,                   /* COMMA  */
    COLON = 275,                   /* COLON  */
    SEMICOLON = 276,               /* SEMICOLON  */
    ARROW_RIGHT = 277,             /* ARROW_RIGHT  */
    ARROW_LEFT = 278,              /* ARROW_LEFT  */
    DASH = 279                     /* DASH  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
  char* IDENTIFIER;                        /* IDENTIFIER  */
  char* STRING_LITERAL;                    /* STRING_LITERAL  */
  char* INTEGER_LITERAL;                   /* INTEGER_LITERAL  */
  char* FLOAT_LITERAL;                     /* FLOAT_LITERAL  */
  cypher_ast_node_t* statement;            /* statement  */
  cypher_ast_node_t* create_statement;     /* create_statement  */
  cypher_ast_node_t* match_statement;      /* match_statement  */
  cypher_ast_node_t* return_statement;     /* return_statement  */
  cypher_ast_node_t* node_pattern;         /* node_pattern  */
  cypher_ast_node_t* relationship_pattern; /* relationship_pattern  */
  cypher_ast_node_t* property_list;        /* property_list  */
  cypher_ast_node_t* variable;             /* variable  */
  cypher_ast_node_t* label;                /* label  */
  cypher_ast_node_t* property;             /* property  */
  cypher_ast_node_t* literal;              /* literal  */
  cypher_ast_node_t* string_literal;       /* string_literal  */
  cypher_ast_node_t* integer_literal;      /* integer_literal  */
  cypher_ast_node_t* float_literal;        /* float_literal  */
  cypher_ast_node_t* boolean_literal;      /* boolean_literal  */

#line 108 "cypher.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_CYPHER_TAB_H_INCLUDED  */

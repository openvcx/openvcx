/** <!--
 *
 *  Copyright (C) 2014 OpenVCX openvcx@gmail.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  If you would like this software to be made available to you under an 
 *  alternate license please email openvcx@gmail.com for more information.
 *
 * -->
 */


#ifndef __LEXPARSE_H__
#define __LEXPARSE_H__



#define MOVE_WHILE_SPACE(p)     if((p)) { while((*(p) == ' ' || *(p) == '\t')) { (p)++; } }
#define MOVE_WHILE_CHAR(p,c)    if((p)) { while((*(p) == (c))) { (p)++; } }
#define MOVE_UNTIL_CHAR(p,c)    if((p)) { while((*(p) != (c) && *(p) != '\0'   )) { (p)++; } }


typedef enum LEX_COMPARISON_OPERATOR {
  LEX_COMPARISON_OPERATOR_NONE           = 0,
  LEX_COMPARISON_OPERATOR_EQ             = 1,
  LEX_COMPARISON_OPERATOR_GT             = 2,
  LEX_COMPARISON_OPERATOR_GTEQ           = 3,
  LEX_COMPARISON_OPERATOR_LT             = 4,
  LEX_COMPARISON_OPERATOR_LTEQ           = 5
} LEX_COMPARISON_OPERATOR_T;
      
typedef enum LEX_PARSE_STATE {
  LEX_PARSE_STATE_BEFORE_BRACKET         = 0,
  LEX_PARSE_STATE_OPEN_BRACKET           = 1,
  LEX_PARSE_STATE_CLOSE_BRACKET          = 2,
  LEX_PARSE_STATE_IMPLIED_BRACKET        = 3,
  LEX_PARSE_STATE_AFTER_IMPLIED          = 4,
} LEX_PARSE_STATE_T;
    
typedef struct LEX_CONDITIONAL_EXPR {
  char                                 var_name[32];
  LEX_COMPARISON_OPERATOR_T            comparison;
  float                                constval; 
} LEX_CONDITIONAL_EXPR_T;
  
struct LEX_CONDITIONAL_CONTEXT;

typedef int (*LEX_CONDITIONAL_CB) (struct LEX_CONDITIONAL_CONTEXT *, const char *);

typedef struct LEX_CONDITIONAL_CONTEXT {
  LEX_CONDITIONAL_EXPR_T               test;
  LEX_PARSE_STATE_T                    state;
  unsigned int                         numassignments;
  void                                *pCbArg;
  LEX_CONDITIONAL_CB                   cbFunc;
} LEX_CONDITIONAL_CONTEXT_T;

int lexical_parse_file_line(const char *line, LEX_CONDITIONAL_CONTEXT_T *pCtxt);
int lexical_compare_int(const LEX_CONDITIONAL_EXPR_T *pexpr, int val);



#endif // __LEXPARSE_H__

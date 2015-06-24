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


#include "vsx_common.h"



#define MOVE_WHILE_VARIABLE(p)  if((p)) { while((*(p) >= 'a' &&  *(p) <= 'z') || \
                                 (*(p) >= 'A' && *(p) <= 'Z') || (*(p) >= '0' && *(p) <= '9') || \
                                  *(p) == '_' || *(p) == '-') { (p)++; } }

#define MOVE_WHILE_FLOAT(p)  if((p)) { while((*(p) >= '0' && *(p) <= '9') || *(p) == '-' || \
                                  *(p) == '.') { (p)++; } }


static enum LEX_COMPARISON_OPERATOR lexical_get_conditional_type(const char **pp) {

  if(*pp[0] == '>') {
    (*pp)++;
    if(*pp[0] == '=') {
      (*pp)++;
      return LEX_COMPARISON_OPERATOR_GTEQ; 
    } else {
      (*pp)++;
      return LEX_COMPARISON_OPERATOR_GT; 
    }
  } else if(*pp[0] == '<') {
    (*pp)++;
    if(*pp[0] == '=') {
      (*pp)++;
      return LEX_COMPARISON_OPERATOR_LTEQ; 
    } else {
      (*pp)++;
      return LEX_COMPARISON_OPERATOR_LT; 
    }
  } else if(*pp[0] == '=') {
    (*pp)++;
    MOVE_WHILE_CHAR((*pp), '=');
    return LEX_COMPARISON_OPERATOR_EQ; 
  }

  return LEX_COMPARISON_OPERATOR_NONE;
}


static int lexical_parse_conditional(const char **ppline, LEX_CONDITIONAL_EXPR_T *pexpr) {
  int rc = 0;
  const char *p, *p2 ;
  char constval[32];

  p = *ppline;
  memset(pexpr, 0, sizeof(LEX_CONDITIONAL_EXPR_T));

  //
  // Expecting a conditional notation similar to the form of 'if ( x > 320 )'
  //

  MOVE_UNTIL_CHAR(p, '(');
  if(*p != '(') {
    return -1;
  }
  MOVE_WHILE_CHAR(p, '(');
  MOVE_WHILE_SPACE(p);

  //
  // Get the variable name
  //
  p2 = p;

  MOVE_WHILE_VARIABLE(p2);

  if(p == p2) {
    return -1;
  }
  strncpy(pexpr->var_name, p, MIN(sizeof(pexpr->var_name) - 1, p2 - p));

  //
  // Get the conditional type
  //
  MOVE_WHILE_SPACE(p2);
  if((pexpr->comparison = lexical_get_conditional_type(&p2)) == LEX_COMPARISON_OPERATOR_NONE) {
    return -1;
  }

  MOVE_WHILE_SPACE(p2);

  //
  // Get the constant numerical value 
  //
  p = p2;

  MOVE_WHILE_FLOAT(p2);

  if(p == p2) {
    return -1;
  }
  memset(constval, 0, sizeof(constval));
  strncpy(constval, p, MIN(sizeof(constval) - 1, p2 - p));
  pexpr->constval = atof(constval);

  MOVE_WHILE_SPACE(p2);
  if(*p2 != ')') {
    return -1;
  }
  MOVE_WHILE_CHAR(p2, ')');

  *ppline = p2;

 //fprintf(stderr, "IF LINE:'%s' p:'%s', p2:'%s',  variable:'%s', comparison:%d, constval:'%s' %f\n", *ppline, p, p2, pexpr->var_name, pexpr->comparison, constval, pexpr->constval);

  return rc;
}

static int lexical_parse_assignment(const char **ppline, LEX_CONDITIONAL_CONTEXT_T *pCtxt) {
  int rc = 0;
  size_t sz;
  int quoted = 0;
  const char *p, *p2;
  char buf[1024];

  p = *ppline;

  MOVE_WHILE_SPACE(p);

  if(*p == '}') {
    return 0;
  }

  if(pCtxt->state == LEX_PARSE_STATE_IMPLIED_BRACKET && pCtxt->numassignments >= 1) {
    LOG(X_ERROR("Multiple statements not permitted in '%s'"), p);
    return -1;
  }

  p2 = p;

  while(*p2 != '\r' && *p2 != '\n' && *p2 !=';' && *p2 != '\0') {

    if(*p2 == '"') {
      quoted = !quoted;
    }
    if(!quoted && *p2 == '}') {
      if(p2 > p) {
        p2--;
      }
      break;
    }

    p2++;
  }

  pCtxt->numassignments++;

  if(pCtxt->cbFunc) {
    sz = MIN(sizeof(buf) - 1, (p2 - p));
    strncpy(buf, p, sz);
    buf[sz] = '\0';

    //avc_dumpHex(stderr, buf, sz+1, 1);
    avc_strip_nl(buf, strlen(buf), 1);
    //avc_dumpHex(stderr, buf, sz+1, 1);
    rc = pCtxt->cbFunc(pCtxt, buf);
  }

  *ppline = p2;

  return rc;
}

static int lexical_parse_expression(const char **ppline, LEX_CONDITIONAL_CONTEXT_T *pCtxt) {
  int rc = 0;
  const char *p;

  //
  // Tries to extract the assignment expression from within a conditional statement context
  //
  // This should be called for each line, including the line containing a conditional expression,
  // following a call to lexical_parse_conditional, to complete parsing the conditional 
  // expression context
  //
  //

  p = *ppline;

  //fprintf(stderr, "PARSE_EXPR state:%d p:'%s'\n", pCtxt->state, p);

  MOVE_WHILE_SPACE(p);

  //
  // Check for opening bracket
  //
  if(pCtxt->state == LEX_PARSE_STATE_BEFORE_BRACKET ||
     pCtxt->state == LEX_PARSE_STATE_IMPLIED_BRACKET) {
    if(*p == '{') {
      pCtxt->state = LEX_PARSE_STATE_OPEN_BRACKET;
      pCtxt->numassignments = 0;
      p++;
      MOVE_WHILE_SPACE(p);
    } else if(pCtxt->state != LEX_PARSE_STATE_IMPLIED_BRACKET && CHAR_PRINTABLE(*p)) {
      pCtxt->state = LEX_PARSE_STATE_IMPLIED_BRACKET;
      pCtxt->numassignments = 0;
    }
  }

  //
  // End of line reached
  //
  if(*p == '\r' || *p == '\n' || *p == '\0') {
    if(pCtxt->state == LEX_PARSE_STATE_BEFORE_BRACKET) {
      pCtxt->state = LEX_PARSE_STATE_IMPLIED_BRACKET;
      pCtxt->numassignments = 0;
    } else if(pCtxt->state == LEX_PARSE_STATE_IMPLIED_BRACKET) {
      pCtxt->state = LEX_PARSE_STATE_CLOSE_BRACKET;
    }
    return 0;
  }

  if(pCtxt->state == LEX_PARSE_STATE_OPEN_BRACKET || 
     pCtxt->state == LEX_PARSE_STATE_IMPLIED_BRACKET) {

    //
    // Parse the expression
    //
    if((rc = lexical_parse_assignment(&p, pCtxt)) < 0) {
      return rc;
    }


    // 
    // Check for any closing bracket
    //
    if(pCtxt->state == LEX_PARSE_STATE_IMPLIED_BRACKET) {
      pCtxt->state = LEX_PARSE_STATE_AFTER_IMPLIED;
    } else if(pCtxt->state == LEX_PARSE_STATE_OPEN_BRACKET) {
      MOVE_WHILE_SPACE(p);
      if(*p == '}') {
        pCtxt->state = LEX_PARSE_STATE_CLOSE_BRACKET;
        p++;
      }
    }


  }

  return rc;
}

int lexical_parse_file_line(const char *line, LEX_CONDITIONAL_CONTEXT_T *pCtxt) {
  int rc = 0;
  const char *p;

  p = line;

  if(pCtxt->state == LEX_PARSE_STATE_CLOSE_BRACKET ||
     pCtxt->state == LEX_PARSE_STATE_AFTER_IMPLIED) {
    pCtxt->state = LEX_PARSE_STATE_BEFORE_BRACKET;
  }

  //TODO: we should handle 'else'

  if(pCtxt->state == LEX_PARSE_STATE_BEFORE_BRACKET) {
    if(!strncasecmp(p, "if", 2)) {

      if((rc = lexical_parse_conditional(&p, &pCtxt->test)) < 0) {
        return rc;
      }
    } else {
      //fprintf(stderr, "NOT IF, LINE:'%s' state:%d\n", p, pCtxt->state);

      //pCtxt->state = LEX_PARSE_STATE_CLOSE_BRACKET;
      pCtxt->state = LEX_PARSE_STATE_IMPLIED_BRACKET;
      pCtxt->numassignments = 0;
      memset(&pCtxt->test, 0, sizeof(pCtxt->test));
    }
  }

  rc = lexical_parse_expression(&p, pCtxt);

  return rc;
}

int lexical_compare_int(const LEX_CONDITIONAL_EXPR_T *pexpr, int val) {

  switch(pexpr->comparison) {
    case LEX_COMPARISON_OPERATOR_EQ:
      return val == (int) pexpr->constval;
      break;
    case LEX_COMPARISON_OPERATOR_GT:
      return val > (int) pexpr->constval;
      break;
    case LEX_COMPARISON_OPERATOR_GTEQ:
      return val >= (int) pexpr->constval;
      break;
    case LEX_COMPARISON_OPERATOR_LT:
      return val < (int) pexpr->constval;
      break;
    case LEX_COMPARISON_OPERATOR_LTEQ:
      return val <= (int) pexpr->constval;
      break;
    default:
      return 0;
  }

}

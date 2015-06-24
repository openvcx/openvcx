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

LIST_NODE_T *list_sort(LIST_NODE_T *pHead, LIST_SORT_COMPARATOR cbCompare) {
  int rc;
  LIST_NODE_T *pNode, *pNodePrev = NULL, *pNodeTmp;
  unsigned int iter = 0, index, count = 0;

  //
  // bubble sort
  //
  while(iter == 0 || iter < count) {

    index = 0;
    pNode = pHead;
    pNodePrev = NULL;

    while(pNode && (iter == 0 || index < count)) {

      //fprintf(stderr, "compare iter[%d], index[%d]/%d\n", iter, index, count);
      if(pNode->pnext && cbCompare && (rc = cbCompare(pNode, pNode->pnext)) > 0) {

        //
        // Move the node down one slot in the list
        //

        if(pNodePrev) {
          pNodePrev->pnext = pNode->pnext;
        } else {
          pHead = pNode->pnext;
        }
        pNodeTmp = pNode->pnext;
        pNode->pnext = pNodeTmp->pnext;
        pNodeTmp->pnext = pNode;
        pNode = pNodeTmp;
      }
      pNodePrev = pNode;
      pNode = pNode->pnext;
      index++;
      if(iter == 0) {
        count++;
      }
    }

    iter++;
    if(count > 0) {
      count--;
    }
  }

  return pHead;
}




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


#ifndef __LIST_H__
#define __LIST_H__


typedef struct LIST_NODE {
  struct LIST_NODE    *pnext;
} LIST_NODE_T;

#define LIST_NEXT(pl)  (((LIST_NODE_T *) (pl))->pnext)

typedef int (* LIST_SORT_COMPARATOR) (const LIST_NODE_T *, const LIST_NODE_T *);

LIST_NODE_T *list_sort(LIST_NODE_T *pHead, LIST_SORT_COMPARATOR cbCompare);




#endif // __LIST_H__



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


#ifndef __EBML_H__
#define __EBML_H__


#define EBML_VERSION                 1
#define EBML_MAX_ID_LENGTH           4
#define EBML_MAX_SIZE_LENGTH         8

#define EBML_ID_VOID                 0xec

typedef enum EBML_TYPE {
  //
  // Unhandled type
  //
  EBML_TYPE_NONE           = 0,

  //
  // Data types
  // 
  EBML_TYPE_UINT32         = 1,
  EBML_TYPE_UINT64         = 2,
  EBML_TYPE_DOUBLE         = 3,
  EBML_TYPE_UTF8           = 4,
  EBML_TYPE_SUBTYPES       = 5,
  EBML_TYPE_DATE           = 6,
  EBML_TYPE_BINARY         = 7,
  EBML_TYPE_BINARY_PARTIAL = 8,

  //
  // Special handler directive types
  //
  EBML_TYPE_PASS           = 9,
  EBML_TYPE_END            = 10,

  //
  // Type count
  //
  EBML_TYPE_LAST           = 11

} EBML_TYPE_T;

typedef struct EBML_LEVEL {
  FILE_OFFSET_T start;
  FILE_OFFSET_T length;
} EBML_LEVEL_T;

//
// We are trying to stay away form C99 struct/union initialization using {{ .ui64= }}
// since this may not be supported on some MSVC compilers.  Static init of a union
// with double and int types may not be working so we use this stupid non-unionized
// struct along with some init macros
//
typedef struct EBML_DATA_VAL {
  double             dbl;
  uint32_t           ui64;
  void              *p;
} EBML_DATA_VAL_T;

#define INIT_DBL(x)      { (x), 0, NULL }
#define INIT_UI64(x)     { 0, (x), NULL }
#define INIT_UI32        INIT_UI64
#define INIT_BIN(x)      { 0, 0, (x) }
#define INIT_STR(x)      INIT_BIN(x)
#define INIT_NONE     INIT_UI64(0)


typedef struct EBML_SYNTAX {
  uint32_t                   id;
  EBML_TYPE_T                type;
  unsigned int               offset;
  const struct EBML_SYNTAX  *pchild;
  EBML_DATA_VAL_T            dflt;
  unsigned int               dynsize;
  unsigned int               strsz;
} EBML_SYNTAX_T;

#define EBML_MAX_NUMBER             0xffffffffffffffULL // 56 bits
#define EBML_MAX_LEVELS             16
#define EBML_BUF_SZ                 8198
#define EBML_MAX_BLOB_SZ            0xffff
#define EBML_BLOB_PARTIAL_DATA_SZ   16

typedef struct EBML_BLOB_PARTIAL {
  unsigned int                  size;
  FILE_OFFSET_T                 fileOffset;
  unsigned char                 buf[EBML_BLOB_PARTIAL_DATA_SZ];
} EBML_BLOB_PARTIAL_T;

typedef struct EBML_BLOB {
  unsigned int                  size;
  int                           nondynalloc;
  FILE_OFFSET_T                 fileOffset;
  void                         *p;
} EBML_BLOB_T;

typedef struct EBML_ARR {
  unsigned int                  count;
  int                           nondynalloc;
  void                         *parr;
} EBML_ARR_T;

typedef struct EBML_BYTE_STREAM {
  BYTE_STREAM_T           bs;
  FILE_STREAM_T          *pfs;
  FILE_OFFSET_T           offset;
  unsigned int            bufsz;
  int                     nolog;
} EBML_BYTE_STREAM_T;


unsigned char *ebml_parse_get_bytes(EBML_BYTE_STREAM_T *mbs, unsigned int len);
int ebml_parse_skip_bytes(EBML_BYTE_STREAM_T *mbs, unsigned int len);
int ebml_parse_get_byte(EBML_BYTE_STREAM_T *mbs);
int ebml_parse_get_num(EBML_BYTE_STREAM_T *mbs, unsigned int max, uint64_t *pval);
int ebml_parse_get_id(EBML_BYTE_STREAM_T *mbs, uint32_t *pval);
int ebml_parse_get_length(EBML_BYTE_STREAM_T *mbs, uint64_t *pval);
int ebml_parse_get_uint(EBML_BYTE_STREAM_T *mbs, unsigned int len, uint64_t *pval);
int ebml_parse_get_double(EBML_BYTE_STREAM_T *mbs, unsigned int len, double *pval);
int ebml_parse_get_binary(EBML_BYTE_STREAM_T *mbs, unsigned int len, void **ppval);
int ebml_parse_get_string(EBML_BYTE_STREAM_T *mbs, unsigned int len, unsigned char **ppval);
void ebml_free_data(const EBML_SYNTAX_T *psyntax, void *pDataParent);


int ebml_write_num(EBML_BYTE_STREAM_T *mbs, uint64_t ui64, unsigned int numBytes);
int ebml_write_id(EBML_BYTE_STREAM_T *mbs, uint32_t ui32);
int ebml_write_uint(EBML_BYTE_STREAM_T *mbs, uint32_t id, uint64_t ui64);
int ebml_write_double(EBML_BYTE_STREAM_T *mbs, uint32_t id, double dbl);
int ebml_write_binary(EBML_BYTE_STREAM_T *mbs, uint32_t id, const void *pval, unsigned int len);
int ebml_write_string(EBML_BYTE_STREAM_T *mbs, uint32_t id, const char *pval);
int ebml_write_void(EBML_BYTE_STREAM_T *mbs, unsigned int len);
int ebml_write_flush(EBML_BYTE_STREAM_T *mbs);


#endif // __EBML_H__

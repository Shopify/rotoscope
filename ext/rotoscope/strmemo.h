#ifndef _INC_ROTOSCOPE_STRMEMO_H_
#define _INC_ROTOSCOPE_STRMEMO_H_

#include "uthash.h"

typedef struct {
  unsigned long key;
  UT_hash_handle hh;
} rs_strmemo_t;

bool rs_strmemo_uniq(rs_strmemo_t **calls, char *entry);
void rs_strmemo_free(rs_strmemo_t *calls);

#endif

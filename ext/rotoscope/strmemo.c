#include <ruby.h>
#include <stdbool.h>

#include "strmemo.h"
#include "uthash.h"

static unsigned long hash_djb2(unsigned char *str) {
  unsigned long hash = 5381;
  int c;
  while ((c = *str++)) hash = ((hash << 5) + hash) + c;
  return hash;
}

bool rs_strmemo_uniq(rs_strmemo_t **calls, char *entry) {
  rs_strmemo_t *c = NULL;
  unsigned long hashkey = hash_djb2((unsigned char *)entry);

  HASH_FIND_INT(*calls, &hashkey, c);
  if (c != NULL) return false;

  c = ALLOC(rs_strmemo_t);
  c->key = hashkey;
  HASH_ADD_INT(*calls, key, c);
  return true;
}

void rs_strmemo_free(rs_strmemo_t *calls) {
  rs_strmemo_t *current_call, *tmp;
  HASH_ITER(hh, calls, current_call, tmp) {
    HASH_DEL(calls, current_call);
    xfree(current_call);
  }
}

#ifndef SWOOSH_DATA_H_FILE
#define SWOOSH_DATA_H_FILE

#include <cstdint>

#include "network.h"
#include "util.h"

#define SWOOSH_DATA_ALWAYS_VALID ((uint64_t) -1)

enum {
  REQUEST_HEAD = 0,
  REQUEST_BODY = 1,
};

enum {
  SWOOSH_DATA_TEXT = NET_MAKE_MAGIC('T', 'e', 'x', 't'),
  SWOOSH_DATA_FILE = NET_MAKE_MAGIC('F', 'i', 'l', 'e'),
  SWOOSH_DATA_DIR  = NET_MAKE_MAGIC('D', 'i', 'r', '/'),
};

#endif /* SOOSH_DATA_H_FILE */

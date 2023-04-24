#ifndef SWOOSH_DATA_H_FILE
#define SWOOSH_DATA_H_FILE

#include "network.h"

enum {
  SWOOSH_DATA_REQUEST_HEAD = 0,
  SWOOSH_DATA_REQUEST_BODY = 1,
};

enum {
  SWOOSH_DATA_TEXT = NET_MAKE_MAGIC('T', 'e', 'x', 't'),
  SWOOSH_DATA_FILE = NET_MAKE_MAGIC('F', 'i', 'l', 'e'),
  SWOOSH_DATA_DIR  = NET_MAKE_MAGIC('D', 'i', 'r', '/'),
};

#endif /* SOOSH_DATA_H_FILE */

#ifndef SHWOOSH_DATA_STORE_H_FILE
#define SHWOOSH_DATA_STORE_H_FILE

#include <cstdint>
#include <map>
#include <mutex>

#include "swoosh_data.h"

class SwooshDataStore
{
protected:
  std::map<uint32_t, SwooshData*> store;
  std::mutex storeMutex;

public:
  void Store(uint32_t id, SwooshData *data);
  SwooshData *Acquire(uint32_t id, uint64_t cur_time);
  bool Release(uint32_t id);
  void RemoveExpired(uint64_t cur_time);
};

#endif /* SHWOOSH_DATA_STORE_H_FILE */

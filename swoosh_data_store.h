#ifndef SHWOOSH_DATA_STORE_H_FILE
#define SHWOOSH_DATA_STORE_H_FILE

#include <cstdint>
#include <map>
#include <mutex>

#include "swoosh_local_data.h"

class SwooshDataStore
{
protected:
  bool running;
  std::map<uint32_t, SwooshLocalData*> store;
  std::mutex storeMutex;

public:
  SwooshDataStore() : running(true) {}

  void Stop();
  void Store(SwooshLocalData *data);
  SwooshLocalData *Acquire(uint32_t id, uint64_t cur_time);
  bool Release(uint32_t id);
  void RemoveExpired(uint64_t cur_time);
};

#endif /* SHWOOSH_DATA_STORE_H_FILE */

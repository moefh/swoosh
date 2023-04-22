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
  void Set(uint32_t id, SwooshData *data) {
    std::lock_guard<std::mutex> guard(storeMutex);
    store[id] = data;
  }

  SwooshData *Get(uint32_t id) {
    std::lock_guard<std::mutex> guard(storeMutex);
    
    auto it = store.find(id);
    if (it != store.end()) {
      return it->second;
    }
    return nullptr;
  }

  SwooshData *Remove(uint32_t id) {
    std::lock_guard<std::mutex> guard(storeMutex);

    auto it = store.find(id);
    if (it != store.end()) {
      SwooshData *ret = it->second;
      store.erase(it);
      return ret;
    }
    return nullptr;
  }
};

#endif /* SHWOOSH_DATA_STORE_H_FILE */

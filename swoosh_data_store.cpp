#include "swoosh_data_store.h"

void SwooshDataStore::Store(uint32_t id, SwooshData *data) {
  std::lock_guard<std::mutex> guard(storeMutex);
  DebugLog("STORING MESSAGE ID %u\n", id);
  store[id] = data;
}

SwooshData *SwooshDataStore::Acquire(uint32_t id, uint64_t cur_time) {
  std::lock_guard<std::mutex> guard(storeMutex);

  auto it = store.find(id);
  if (it != store.end()) {
    SwooshData *data = it->second;
    if (data->IsExpiredAt(cur_time)) {
      DebugLog("MESSAGE ID %u IS EXPIRED (valid until %llu, cur_time is %llu, diff is %llu)\n", id, data->GetValidUntil()/1000, cur_time/1000, (cur_time - data->GetValidUntil())/1000);
      return nullptr;
    }
    data->AddRef();
    return data;
  }
  return nullptr;
}

bool SwooshDataStore::Release(uint32_t id) {
  std::lock_guard<std::mutex> guard(storeMutex);

  auto it = store.find(id);
  if (it != store.end()) {
    SwooshData *data = it->second;
    data->RemoveRef();
    return true;
  }
  return false;
}

void SwooshDataStore::RemoveExpired(uint64_t cur_time) {
  std::lock_guard<std::mutex> guard(storeMutex);

  for (auto it = store.begin(); it != store.end(); ) {
    SwooshData *data = it->second;
    if (!data->hasRefs() && data->IsExpiredAt(cur_time)) {
      DebugLog("DELETING EXPIRED MESSAGE ID %u, beacon is %p\n", it->first, data->GetBeacon());
      delete data;
      it = store.erase(it);
    } else {
      ++it;
    }
  }
}

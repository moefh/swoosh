#include "targetver.h"
#include "swoosh_data_store.h"

void SwooshDataStore::Stop(void)
{
  std::lock_guard<std::mutex> guard(storeMutex);

  running = false;
}

void SwooshDataStore::Store(SwooshLocalData *data)
{
  std::lock_guard<std::mutex> guard(storeMutex);
  store[data->GetMessageId()] = data;
}

SwooshLocalData *SwooshDataStore::Acquire(uint32_t id, uint64_t cur_time)
{
  std::lock_guard<std::mutex> guard(storeMutex);

  auto it = store.find(id);
  if (it != store.end()) {
    SwooshLocalData *data = it->second;
    if (data->IsExpiredAt(cur_time)) {
      return nullptr;
    }
    data->AddRef();
    return data;
  }
  return nullptr;
}

bool SwooshDataStore::Release(uint32_t id)
{
  std::lock_guard<std::mutex> guard(storeMutex);

  auto it = store.find(id);
  if (it != store.end()) {
    SwooshLocalData *data = it->second;
    data->RemoveRef();
    return true;
  }
  return false;
}

void SwooshDataStore::RemoveExpired(uint64_t cur_time)
{
  std::lock_guard<std::mutex> guard(storeMutex);

  if (!running) {
    return;
  }

  for (auto it = store.begin(); it != store.end(); ) {
    SwooshLocalData *data = it->second;
    if (!data->hasRefs() && data->IsExpiredAt(cur_time)) {
      delete data;
      it = store.erase(it);
    } else {
      ++it;
    }
  }
}

#ifndef SWOOSH_REMOTE_DATA_H_FILE
#define SWOOSH_REMOTE_DATA_H_FILE

#include "swoosh_data.h"

#include <string>
#include <functional>

// ==========================================================================
// SwooshRemoteData
// ==========================================================================
class SwooshRemoteData
{
  friend class SwooshNode;

protected:
  net_msg_beacon *beacon;
  bool is_good;

  static SwooshRemoteData *ReceiveData(net_msg_beacon *beacon);
  static std::string *ReceiveString(net_socket *sock, size_t max_size);

  virtual bool Download(std::string local_path, std::function<void(double)> progress) = 0;

public:
  SwooshRemoteData(net_msg_beacon *beacon) : beacon(beacon), is_good(false) {}
  virtual ~SwooshRemoteData() {
    net_free_beacon(beacon);
  }

  net_msg_beacon *GetBeacon() { return beacon; }

  bool IsGood() { return is_good; };
};

// ==========================================================================
// SwooshRemoteTextData
// ==========================================================================
class SwooshRemoteTextData : public SwooshRemoteData
{
protected:
  std::string text;

  virtual bool Download(std::string local_path, std::function<void(double)> progress);

public:
  SwooshRemoteTextData(net_msg_beacon *beacon, net_socket *sock);
  virtual ~SwooshRemoteTextData() = default;

  virtual std::string &GetText() { return text; }
};

// ==========================================================================
// SwooshRemoteFileData
// ==========================================================================
class SwooshRemoteFileData : public SwooshRemoteData
{
protected:
  std::string file_name;
  uint32_t file_size;

  virtual bool Download(std::string local_path, std::function<void(double)> progress);

public:
  SwooshRemoteFileData(net_msg_beacon *beacon, net_socket *sock);
  virtual ~SwooshRemoteFileData() = default;

  virtual std::string &GetFileName() { return file_name; }
  virtual uint32_t GetFileSize() { return file_size; }
};

#endif /* SWOOSH_REMOTE_DATA_H_FILE */

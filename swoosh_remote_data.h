#ifndef SWOOSH_REMOTE_DATA_H_FILE
#define SWOOSH_REMOTE_DATA_H_FILE

#include "swoosh_data.h"

#include <string>
#include <functional>

class SwooshRemoteData
{
  friend class SwooshNode;

protected:
  net_msg_beacon *beacon;

  static SwooshRemoteData *ReceiveData(net_msg_beacon *beacon, uint32_t type_id, net_socket *sock);
  virtual bool Download(std::string local_path, std::function<void(double)> progress) = 0;
  std::string *ReceiveString(net_socket *sock, size_t max_size);

public:
  SwooshRemoteData(net_msg_beacon *beacon) : beacon(beacon) {}
  virtual ~SwooshRemoteData() {
    net_free_beacon(beacon);
  }

  net_msg_beacon *GetBeacon() { return beacon; }

  virtual bool IsGood() = 0;
};

class SwooshRemoteTextData : public SwooshRemoteData
{
protected:
  bool is_good;
  std::string text;

  virtual bool Download(std::string local_path, std::function<void(double)> progress);

public:
  SwooshRemoteTextData(net_msg_beacon *beacon, net_socket *sock);

  virtual ~SwooshRemoteTextData() {}

  virtual bool IsGood() { return is_good; };
  virtual std::string &GetText() { return text; }

};

class SwooshRemoteFileData : public SwooshRemoteData
{
protected:
  bool is_good;
  std::string file_name;
  uint32_t file_size;

  virtual bool Download(std::string local_path, std::function<void(double)> progress);

public:
  SwooshRemoteFileData(net_msg_beacon *beacon, net_socket *sock);

  virtual ~SwooshRemoteFileData() {}

  virtual bool IsGood() { return is_good; };
  virtual std::string &GetFileName() { return file_name; }
  virtual uint32_t GetFileSize() { return file_size; }
};

#endif /* SWOOSH_REMOTE_DATA_H_FILE */

#ifndef SWOOSH_LOCAL_DATA_H_FILE
#define SWOOSH_LOCAL_DATA_H_FILE

#include "swoosh_data.h"

#include <cstdint>
#include <string>

#include <wx/filefn.h>

#include "network.h"

class SwooshLocalData
{
  friend class SwooshNode;

protected:
  uint64_t message_id;
  uint64_t valid_until;
  int num_refs;

  virtual int SendContentHead(net_socket *sock) = 0;
  virtual int SendContentBody(net_socket *sock) = 0;

  void SetMessageId(uint32_t message_id) { this->message_id = message_id; }
  int SendString(net_socket *sock, const std::string &str);

public:
  SwooshLocalData(uint64_t valid_until) : message_id(0), valid_until(valid_until), num_refs(0) {}

  virtual ~SwooshLocalData() = default;

  uint32_t GetMessageId() { return message_id; }

  void AddRef() { num_refs++; }
  void RemoveRef() { num_refs--; }
  bool hasRefs() { return num_refs != 0; }

  uint64_t GetValidUntil() { return valid_until; }
  bool IsExpiredAt(uint64_t time) { return (valid_until != SWOOSH_DATA_ALWAYS_VALID) && (valid_until < time); }
};

class SwooshLocalTextData : public SwooshLocalData
{
protected:
  std::string text;

  virtual int SendContentHead(net_socket *sock);
  virtual int SendContentBody(net_socket *sock);

public:
  SwooshLocalTextData(uint64_t valid_until, const std::string &str)
    : SwooshLocalData(valid_until), text(str) {
  }

  virtual ~SwooshLocalTextData() {}

  virtual std::string &GetText() { return text; }
};

class SwooshLocalFileData : public SwooshLocalData
{
protected:
  std::string file_name;
  uint32_t file_size;

  virtual int SendContentHead(net_socket *sock);
  virtual int SendContentBody(net_socket *sock);

public:
  SwooshLocalFileData(uint64_t valid_until, const std::string &file_name);

  virtual ~SwooshLocalFileData() {}

  virtual std::string &GetFileName() { return file_name; }
  virtual uint32_t GetFileSize() { return file_size; }
};

#endif /* SWOOSH_LOCAL_DATA_H_FILE */

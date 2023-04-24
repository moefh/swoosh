#include "swoosh_local_data.h"

#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <fstream>

#include "swoosh_app.h"

int SwooshLocalData::SendString(net_socket *sock, const std::string &str)
{
  // send string length
  uint32_t data_size = (uint32_t) str.size();
  if (net_send_u32(sock, data_size) < 0) {
    DebugLog("ERROR: can't send string size\n");
    return -1;
  }

  // send string text
  if (net_send_data(sock, str.data(), data_size) < 0) {
    DebugLog("ERROR: can't send string data\n");
    return -1;
  }

  return 0;
}

int SwooshLocalTextData::SendContentHead(net_socket *sock)
{
  // for text messages, head is the same as body
  return SendContentBody(sock);
}

int SwooshLocalTextData::SendContentBody(net_socket *sock)
{
  // send data type
  if (net_send_u32(sock, SWOOSH_DATA_TEXT) != 0) return -1;

  // send data
  if (SendString(sock, text) != 0) {
    DebugLog("ERROR: can't send message text\n");
    return -1;
  }

  //DebugLog("[SENDER] done sending string\n");

  return 0;
}

SwooshLocalFileData::SwooshLocalFileData(uint64_t valid_until, const std::string &file_name)
  : SwooshLocalData(valid_until), file_name(file_name), file_size(0)
{
  ReadFileSize(file_name, &file_size);
};

int SwooshLocalFileData::SendContentHead(net_socket *sock)
{
  // send data type
  if (net_send_u32(sock, SWOOSH_DATA_FILE) != 0) return -1;

  // send file name
  if (SendString(sock, GetPathFilename(file_name)) != 0) {
    DebugLog("ERROR: can't send file name\n");
    return -1;
  }

  // send file size
  if (net_send_u32(sock, file_size) != 0) {
    DebugLog("ERROR: can't send file size\n");
    return -1;
  }

  return 0;
}

int SwooshLocalFileData::SendContentBody(net_socket *sock)
{
  uint32_t data_size = 0;
  if (ReadFileSize(file_name, &data_size) != 0) {
    DebugLog("ERROR: can't read file size for '%s'\n", file_name.c_str());
    return -1;
  }

  // send file size
  if (net_send_u32(sock, data_size) != 0) {
    DebugLog("ERROR: can't send file size\n");
    return -1;
  }

  // send file data
  std::ifstream file(file_name, std::ios::binary);
  if (!file.good()) {
    DebugLog("ERROR: can't open file '%s'\n", file_name.c_str());
    return -1;
  }

  uint32_t size_left = data_size;
  while (size_left > 0) {
    char data[4096];
    uint32_t chunk_size = (size_left > sizeof(data)) ? sizeof(data) : size_left;
    file.read(data, chunk_size);
    if (!file.good()) {
      DebugLog("ERROR: can't read file '%s'\n", file_name.c_str());
      return -1;
    }
    if (net_send_data(sock, data, chunk_size) != 0) {
      DebugLog("ERROR: can't send file data\n");
      return -1;
    }
    size_left -= chunk_size;
  }

  return 0;
}

#include "swoosh_data.h"

#include <vector>
#include <fstream>

#include "swoosh_app.h"
#include "swoosh_node.h"
#include "util.h"

#define MAX_TEXT_SIZE      (512*1024*1024)
#define MAX_FILENAME_SIZE  (128)

SwooshData *SwooshData::ReceiveData(net_msg_beacon *beacon, uint32_t data_type_id, net_socket *sock)
{
  switch (data_type_id) {
  case SWOOSH_DATA_TEXT: return new SwooshTextData(beacon, sock);
  case SWOOSH_DATA_FILE: return new SwooshFileData(beacon, sock);

  default:
    DebugLog("ERROR: unknown message type id: %u (0x%x)\n", data_type_id, data_type_id);
    return nullptr;
  }
}

std::string *SwooshData::ReceiveString(net_socket *sock, size_t max_size)
{
  // read length
  uint32_t data_len = 0;
  if (net_recv_u32(sock, &data_len) < 0) {
    DebugLog("ERROR: can't receive string size\n");
    return nullptr;
  }
  if (data_len > max_size) {
    DebugLog("ERROR: invalid string size: %u (max is %u)\n", data_len, max_size);
    return nullptr;
  }

  // read data
  std::vector<char> data(data_len);
  int ret = net_recv_data(sock, data.data(), data_len);
  if (ret < 0) {
    DebugLog("ERROR: can't receive string data (len=%llu)\n", data_len);
    return nullptr;
  }

  // convert to string
  return new std::string(data.begin(), data.end());
}

int SwooshData::SendString(net_socket *sock, const std::string &str)
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

SwooshTextData::SwooshTextData(net_msg_beacon *beacon, net_socket *sock)
  : SwooshData(beacon), is_good(false), text("")
{
  // request head
  if (net_send_u32(sock, REQUEST_HEAD) != 0) {
    DebugLog("ERROR: can't send request type\n");
    return;
  }

  // read message text
  auto text_ptr = ReceiveString(sock, MAX_TEXT_SIZE);
  if (text_ptr == nullptr) {
    DebugLog("ERROR: can't read message text\n");
    return;
  }
  text = *text_ptr;

  is_good = true;
}

bool SwooshTextData::Download(std::string local_path, std::function<void(double)> progress)
{
  progress(0.0);
  DebugLog("ERROR: downloading text messages is not implemented!\n");
  return false;
}

int SwooshTextData::SendContentHead(net_socket *sock)
{
  // for text messages, head is the same as body
  return SendContentBody(sock);
}

int SwooshTextData::SendContentBody(net_socket *sock)
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


SwooshFileData::SwooshFileData(uint64_t valid_until, const std::string &file_name)
  : SwooshData(valid_until), is_good(true), file_name(file_name), file_size(0)
{
  ReadFileSize(file_name, &file_size);
};

SwooshFileData::SwooshFileData(net_msg_beacon *beacon, net_socket *sock)
  : SwooshData(beacon), is_good(false), file_name(""), file_size(0)
{
  // request info
  if (net_send_u32(sock, REQUEST_HEAD) != 0) {
    DebugLog("ERROR: can't send request type\n");
    return;
  }

  // read file name
  auto file_name_ptr = ReceiveString(sock, MAX_FILENAME_SIZE);
  if (file_name_ptr == nullptr) {
    DebugLog("ERROR: can't read file name\n");
    return;
  }
  file_name = *file_name_ptr;

  // read file size
  if (net_recv_u32(sock, &file_size) < 0) {
    DebugLog("ERROR: can't read file size\n");
    return;
  }

  is_good = true;
}

bool SwooshFileData::Download(std::string local_path, std::function<void(double)> progress)
{
  progress(0.0);

  net_socket *sock = net_connect_to_beacon(beacon);
  if (sock == nullptr) {
    DebugLog("ERROR: can't connect to sender\n");
    return false;
  }

  bool success = false;

  // request file
  uint32_t message_id = net_get_beacon_message_id(beacon);
  if (net_send_u32(sock, message_id) != 0) {
    DebugLog("ERROR: can't send request message_id\n");
    goto end;
  }

  // request body
  if (net_send_u32(sock, REQUEST_BODY) != 0) {
    DebugLog("ERROR: can't send request type\n");
    goto end;
  }

  // read file size
  uint32_t file_size;
  if (net_recv_u32(sock, &file_size) < 0) {
    DebugLog("ERROR: can't read file size\n");
    goto end;
  }

  {
    std::ofstream file(local_path, std::ios::binary);
    if (!file.good()) {
      DebugLog("ERROR: can't open download file '%s'\n", local_path.c_str());
      goto end;
    }

    uint32_t size_left = file_size;
    while (size_left > 0) {
      char data[4096];
      uint32_t chunk_size = (size_left > sizeof(data)) ? sizeof(data) : size_left;
      if (net_recv_data(sock, data, chunk_size) != 0) {
        DebugLog("ERROR: can't read file data\n");
        goto end;
      }
      file.write(data, chunk_size);
      if (!file.good()) {
        DebugLog("ERROR: can't write to file '%s'\n", local_path.c_str());
        goto end;
      }
      progress(1.0 - (double) size_left / file_size);
      size_left -= chunk_size;
    }
  }

  success = true;
  progress(1.0);
end:
  net_close_socket(sock);
  return success;
}

int SwooshFileData::SendContentHead(net_socket *sock)
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

int SwooshFileData::SendContentBody(net_socket *sock)
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

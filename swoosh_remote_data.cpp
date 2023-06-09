#include "targetver.h"
#include "swoosh_remote_data.h"

#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <fstream>
#include <wx/filefn.h>

#include "util.h"

#define MAX_TEXT_SIZE      (1024*1024)
#define MAX_FILENAME_SIZE  (128)

// ==========================================================================
// SwooshRemoteData
// ==========================================================================

SwooshRemoteData *SwooshRemoteData::ReceiveData(net_msg_beacon *beacon)
{
  SwooshRemoteData *data = nullptr;
  struct net_socket *sock = net_connect_to_beacon(beacon);
  if (!sock) {
    return nullptr;
  }

  // send request for message with our ID
  uint32_t message_id = net_get_beacon_message_id(beacon);
  if (net_send_u32(sock, message_id) != 0) {
    DebugLog("ERROR: can't send message id in request\n");
    goto end;
  }

  // send request for message information
  if (net_send_u32(sock, SWOOSH_DATA_REQUEST_HEAD) != 0) {
    DebugLog("ERROR: can't send info in request\n");
    goto end;
  }

  // read data type
  uint32_t data_type_id;
  if (net_recv_u32(sock, &data_type_id) != 0) {
    DebugLog("ERROR: can't read message type\n");
    goto end;
  }

  switch (data_type_id) {
  case SWOOSH_DATA_TEXT: data = new SwooshRemoteTextData(beacon, sock); break;
  case SWOOSH_DATA_FILE: data = new SwooshRemoteFileData(beacon, sock); break;
  case SWOOSH_DATA_DIR:  data = new SwooshRemoteDirData(beacon, sock); break;

  default:
    DebugLog("ERROR: unknown data type id: %u (0x%x)\n", data_type_id, data_type_id);
    break;
  }

end:
  net_close_socket(sock);
  return data;
}

std::string *SwooshRemoteData::ReceiveString(net_socket *sock, size_t max_size)
{
  // read length
  uint32_t data_len = 0;
  if (net_recv_u32(sock, &data_len) != 0) {
    DebugLog("ERROR: can't receive string size\n");
    return nullptr;
  }
  if (data_len > max_size) {
    DebugLog("ERROR: invalid string size: %u (max is %u)\n", data_len, max_size);
    return nullptr;
  }

  // read data
  std::vector<char> data(data_len);
  if (net_recv_data(sock, data.data(), data_len) != 0) {
    DebugLog("ERROR: can't receive string data (len=%llu)\n", data_len);
    return nullptr;
  }

  // convert to string
  return new std::string(data.begin(), data.end());
}

int SwooshRemoteData::ReceiveFile(net_socket *sock, const std::string &local_path, std::function<void(double)> progress)
{
  // read file size
  uint32_t file_size;
  if (net_recv_u32(sock, &file_size) != 0) {
    DebugLog("ERROR: can't read file size\n");
    return -1;
  }

  std::ofstream file(local_path, std::ios::binary);
  if (!file.good()) {
    DebugLog("ERROR: can't open download file '%s'\n", local_path.c_str());
    return -1;
  }

  uint32_t size_left = file_size;
  while (size_left > 0) {
    char data[4096];
    uint32_t chunk_size = (size_left > sizeof(data)) ? sizeof(data) : size_left;
    if (net_recv_data(sock, data, chunk_size) != 0) {
      DebugLog("ERROR: can't read file data\n");
      return -1;
    }
    file.write(data, chunk_size);
    if (!file.good()) {
      DebugLog("ERROR: can't write to file '%s'\n", local_path.c_str());
      return -1;
    }
    progress(1.0 - (double) size_left / file_size);
    size_left -= chunk_size;
  }

  return 0;
}

// ==========================================================================
// SwooshRemoteTextData
// ==========================================================================

SwooshRemoteTextData::SwooshRemoteTextData(net_msg_beacon *beacon, net_socket *sock)
  : SwooshRemoteData(beacon), text("")
{
  // request head
  if (net_send_u32(sock, SWOOSH_DATA_REQUEST_HEAD) != 0) {
    DebugLog("ERROR: can't send request type\n");
    return;
  }

  // read message text
  auto text_ptr = ReceiveString(sock, MAX_TEXT_SIZE);
  if (text_ptr == nullptr) {
    DebugLog("ERROR: can't read message text\n");
    return;
  }
  text = std::move(*text_ptr);
  delete text_ptr;

  is_good = true;
}

bool SwooshRemoteTextData::Download(std::string local_path, std::function<void(double)> progress)
{
  progress(0.0);
  DebugLog("ERROR: downloading text messages is not implemented!\n");
  return false;
}

// ==========================================================================
// SwooshRemoteFileData
// ==========================================================================

SwooshRemoteFileData::SwooshRemoteFileData(net_msg_beacon *beacon, net_socket *sock)
  : SwooshRemotePermanentData(beacon), file_name(""), file_size(0)
{
  // request info
  if (net_send_u32(sock, SWOOSH_DATA_REQUEST_HEAD) != 0) {
    DebugLog("ERROR: can't send request type\n");
    return;
  }

  // read file name
  auto file_name_ptr = ReceiveString(sock, MAX_FILENAME_SIZE);
  if (file_name_ptr == nullptr) {
    DebugLog("ERROR: can't read file name\n");
    return;
  }
  file_name = std::move(*file_name_ptr);
  delete file_name_ptr;

  // read file size
  if (net_recv_u32(sock, &file_size) != 0) {
    DebugLog("ERROR: can't read file size\n");
    return;
  }

  is_good = true;
}

bool SwooshRemoteFileData::Download(std::string local_path, std::function<void(double)> progress)
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
  if (net_send_u32(sock, SWOOSH_DATA_REQUEST_BODY) != 0) {
    DebugLog("ERROR: can't send request type\n");
    goto end;
  }

  // download file
  if (ReceiveFile(sock, local_path, progress) != 0) {
    goto end;
  }

  success = true;
  progress(1.0);
end:
  net_close_socket(sock);
  return success;
}

// ==========================================================================
// SwooshRemoteDirData
// ==========================================================================

SwooshRemoteDirData::SwooshRemoteDirData(net_msg_beacon *beacon, net_socket *sock)
  : SwooshRemotePermanentData(beacon), dir_name(""), tree_size(0)
{
  // request info
  if (net_send_u32(sock, SWOOSH_DATA_REQUEST_HEAD) != 0) {
    DebugLog("ERROR: can't send request type\n");
    return;
  }

  // read num items
  if (net_recv_u32(sock, &tree_size) != 0) {
    DebugLog("ERROR: can't read tree size\n");
    return;
  }

  // read dir name
  auto dir_name_ptr = ReceiveString(sock, MAX_FILENAME_SIZE);
  if (dir_name_ptr == nullptr) {
    DebugLog("ERROR: can't read dir name\n");
    return;
  }
  dir_name = std::move(*dir_name_ptr);
  delete dir_name_ptr;

  is_good = true;
}

bool SwooshRemoteDirData::Download(std::string local_path, std::function<void(double)> progress)
{
  progress(0.0);

  net_socket *sock = net_connect_to_beacon(beacon);
  if (sock == nullptr) {
    DebugLog("ERROR: can't connect to sender\n");
    return false;
  }

  bool success = false;

  // request dir
  uint32_t message_id = net_get_beacon_message_id(beacon);
  if (net_send_u32(sock, message_id) != 0) {
    DebugLog("ERROR: can't send request message_id\n");
    goto end;
  }

  // request body
  if (net_send_u32(sock, SWOOSH_DATA_REQUEST_BODY) != 0) {
    DebugLog("ERROR: can't send request type\n");
    goto end;
  }

  // read num dirs
  uint32_t num_dirs;
  if (net_recv_u32(sock, &num_dirs) != 0) {
    DebugLog("ERROR: can't read num dirs\n");
    goto end;
  }

  // read num files
  uint32_t num_files;
  if (net_recv_u32(sock, &num_files) != 0) {
    DebugLog("ERROR: can't read num dirs\n");
    goto end;
  }

  // download dirs
  for (uint32_t i = 0; i < num_dirs; i++) {
    progress(double(i) / (num_dirs + num_files));

    auto dir_name_ptr = ReceiveString(sock, MAX_FILENAME_SIZE);
    if (dir_name_ptr == nullptr) {
      DebugLog("ERROR: can't read dir name\n");
      goto end;
    }
    std::string dir_name = *dir_name_ptr;
    delete dir_name_ptr;

    if (! isGoodLocalFileName(dir_name)) {
      DebugLog("ERROR: refusing to receive directory with invalid name\n");
      goto end;
    }
    wxMkDir(local_path + "/" + dir_name, wxS_DIR_DEFAULT);
  }

  // download files
  for (uint32_t i = 0; i < num_files; i++) {
    progress(double(i + num_dirs) / (num_dirs + num_files));

    auto file_name_ptr = ReceiveString(sock, MAX_FILENAME_SIZE);
    if (file_name_ptr == nullptr) {
      DebugLog("ERROR: can't read dir name\n");
      goto end;
    }
    std::string file_name = *file_name_ptr;
    delete file_name_ptr;

    if (! isGoodLocalFileName(file_name)) {
      DebugLog("ERROR: refusing to receive file with invalid name\n");
      goto end;
    }

    auto file_path = local_path + "/" + file_name;
    if (ReceiveFile(sock, file_path, [](double) {}) != 0) {
      goto end;
    }
  }

  success = true;
  progress(1.0);
end:
  net_close_socket(sock);
  return success;
}

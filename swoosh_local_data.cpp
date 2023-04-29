#include "swoosh_frame.h"
#include "swoosh_local_data.h"

#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <fstream>
#include <wx/dir.h>

#include "swoosh_app.h"
#include "util.h"

// ==========================================================================
// SwooshLocalData
// ==========================================================================

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

int SwooshLocalData::SendFile(net_socket *sock, const std::string &file_name)
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

// ==========================================================================
// SwooshLocalTextData
// ==========================================================================

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

// ==========================================================================
// SwooshLocalFileData
// ==========================================================================

SwooshLocalFileData::SwooshLocalFileData(uint32_t message_id, const std::string &file_name)
  : SwooshLocalPermanentData(message_id), file_name(file_name), file_size(0)
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
  return SendFile(sock, file_name);
}

// ==========================================================================
// SwooshLocalDirData
// ==========================================================================

class DirTree : public wxDirTraverser
{
protected:
  int num_dirs;
  int num_files;
  std::vector<std::string> *dirs;
  std::vector<std::string> *files;
  const std::string *root;

  std::string GetPathFragment(const std::string &path) {
    if (!root) return "";
    if (path.size() <= root->size()) return "";
    if (path.substr(0, root->size()) == *root && (path[root->size()] == '/' || path[root->size()] == '\\')) {
      auto ret = path.substr(root->size() + 1);
      for (size_t i = 0; i < ret.size(); i++) {
        if (ret[i] == '\\') {
          ret[i] = '/';
        }
      }
      return ret;
    }
    return path;
  }

public:
  DirTree(std::vector<std::string> *dirs, std::vector<std::string> *files)
    : num_dirs(0), num_files(0), dirs(dirs), files(files), root(nullptr) {
  }

  DirTree() : num_dirs(0), num_files(0), dirs(nullptr), files(nullptr), root(nullptr) {}

  bool Sweep(const std::string &dir_name) {
    wxDir dir;
    if (!dir.Open(dir_name)) {
      return false;
    }
    num_dirs = 0;
    num_files = 0;
    if (dirs) dirs->clear();
    if (files) files->clear();
    root = &dir_name;
    dir.Traverse(*this, "", wxDIR_FILES|wxDIR_DIRS|wxDIR_HIDDEN|wxDIR_NO_FOLLOW);
    root = nullptr;
    dir.Close();
    return true;
  }

  int GetNumDirs() { return num_dirs; }
  int GetNumFiles() { return num_files; }

  virtual wxDirTraverseResult OnFile(const wxString &filename)
  {
    num_files++;
    if (files) files->push_back(GetPathFragment(filename.ToStdString()));
    return wxDIR_CONTINUE;
  }

  virtual wxDirTraverseResult OnDir(const wxString &dirname)
  {
    num_dirs++;
    if (dirs) dirs->push_back(GetPathFragment(dirname.ToStdString()));
    return wxDIR_CONTINUE;
  }
};

SwooshLocalDirData::SwooshLocalDirData(uint32_t message_id, const std::string &dir_name)
  : SwooshLocalPermanentData(message_id), dir_name(dir_name)
{
  DirTree tree;
  if (!tree.Sweep(dir_name)) {
    tree_size = 0;
  } else {
    tree_size = tree.GetNumDirs() + tree.GetNumFiles();
  }
};

int SwooshLocalDirData::SendContentHead(net_socket *sock)
{
  // send data type
  if (net_send_u32(sock, SWOOSH_DATA_DIR) != 0) return -1;

  // send file size
  if (net_send_u32(sock, tree_size) != 0) {
    DebugLog("ERROR: can't send tree size\n");
    return -1;
  }

  // send file name
  if (SendString(sock, GetPathFilename(dir_name)) != 0) {
    DebugLog("ERROR: can't send dir name\n");
    return -1;
  }

  return 0;
}

int SwooshLocalDirData::SendContentBody(net_socket *sock)
{
  std::vector<std::string> dirs, files;
  DirTree tree(&dirs, &files);
  if (!tree.Sweep(dir_name)) {
    DebugLog("ERROR: can't open directory '%s'\n", dir_name.c_str());
    return -1;
  }
  
  // send number of directories
  if (net_send_u32(sock, tree.GetNumDirs()) != 0) {
    DebugLog("ERROR: can't send num dirs\n");
    return -1;
  }

  // send number of files
  if (net_send_u32(sock, tree.GetNumFiles()) != 0) {
    DebugLog("ERROR: can't send num files\n");
    return -1;
  }

  // send directory names
  for (const auto &dir : dirs) {
    if (SendString(sock, dir) != 0) {
      DebugLog("ERROR: can't send dir name\n");
      return -1;
    }
  }

  // send files
  for (const auto &file : files) {
    if (SendString(sock, file) != 0) {
      DebugLog("ERROR: can't send file name\n");
      return -1;
    }
    if (SendFile(sock, dir_name + "/" + file) != 0) {
      return -1;
    }
  }

  return 0;
}

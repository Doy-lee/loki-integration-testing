#ifndef LOKI_OS_H
#define LOKI_OS_H

void  os_kill_process  (FILE *process); // TODO(loki): This doesn't work. But we don't use it right now, so thats ok
FILE *os_launch_process(char const *cmd_line);
void  os_sleep_s       (int seconds);
void  os_sleep_ms      (int ms);

struct os_file_info
{
  usize    size;
  uint64_t create_time_in_s;
  uint64_t last_write_time_in_s;
  uint64_t last_access_time_in_s;
};
bool  os_file_delete    (char const *path);
bool  os_file_dir_delete(char const *path);
bool  os_file_dir_make  (char const *path);
bool  os_file_exists    (char const *path, os_file_info *info = nullptr);

#if defined(LOKI_OS_IMPLEMENTATION)

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <Windows.h>
#else
  #include <sys/types.h>  // mkdir mode typedefs
  #include <sys/stat.h>   // unlink, semaphore
  #include <sys/unistd.h> // rmdir
  #include <fcntl.h>      // semaphore
  #include <semaphore.h>
  #include <ftw.h>        // nftw
#endif

#include <chrono>
#include <thread>

void os_kill_process(FILE *process)
{
#ifdef _WIN32
  _pclose(process);
#else
  pclose(process);
#endif
}

FILE *os_launch_process(char const *cmd_line)
{
  FILE *result = nullptr;
#ifdef _WIN32
  result = _popen(cmd_line, "r");
#else
  result = popen(cmd_line, "r");
#endif
  return result;
}

void os_sleep_s(int seconds)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(seconds * 1000));
}

void os_sleep_ms(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool os_file_delete(char const *path)
{
#if defined(_WIN32)
    bool result = DeleteFileA(path);
#else
    bool result = (unlink(path) == 0);
#endif
    return result;
}

bool os_file_dir_delete(char const *path)
{
#if defined(_WIN32)
#error "Please implement"
#else

  auto const file_delete_callback = [](const char *path, const struct stat *file_stat, int type_flag, struct FTW *file_tree_walker) -> int {
    int result = remove(path);
    if (result) perror(path);
    return result;
  };

  bool result = (nftw(path, file_delete_callback, 128, FTW_DEPTH | FTW_PHYS) == 0);
#endif
  return result;
}

bool os_file_dir_make(char const *path)
{
#if defined(_WIN32)
  // TODO(doyle): Handle error and this is super incomplete. Cannot create directories recursively
  CreateDirectoryA(path, nullptr /*lpSecurityAttributes*/);
  return true;
#else
  bool result = (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0);
  return result;
#endif
}

bool os_file_exists(char const *path, os_file_info *info)
{
#if defined(_WIN32)
  auto const FileTimeToSeconds = [](FILETIME const *time) -> int64_t {
    ULARGE_INTEGER time_large_int = {};
    time_large_int.LowPart        = time->dwLowDateTime;
    time_large_int.HighPart       = time->dwHighDateTime;

    uint64_t result = (time_large_int.QuadPart / 10000000ULL) - 11644473600ULL;
    return result;
  };

  WIN32_FILE_ATTRIBUTE_DATA attrib_data = {};
  if (GetFileAttributesExA(path, GetFileExInfoStandard, &attrib_data))
  {
    if (info)
    {
      info->create_time_in_s      = FileTimeToSeconds(&attrib_data.ftCreationTime);
      info->last_access_time_in_s = FileTimeToSeconds(&attrib_data.ftLastAccessTime);
      info->last_write_time_in_s  = FileTimeToSeconds(&attrib_data.ftLastWriteTime);

      // TODO(doyle): What if usize is < Quad.part?
      LARGE_INTEGER large_int = {};
      large_int.HighPart      = attrib_data.nFileSizeHigh;
      large_int.LowPart       = attrib_data.nFileSizeLow;
      info->size              = (usize)large_int.QuadPart;
    }
    return true;
  }
  return false;

#else
  struct stat file_stat = {};
  if (stat(path, &file_stat))
  {
    return false;
  }

  if (info)
  {
    info->size                  = file_stat.st_size;
    info->create_time_in_s      = 0; // TODO(doyle): Fill out
    info->last_write_time_in_s  = file_stat.st_mtime;
    info->last_access_time_in_s = file_stat.st_atime;
  }
  return true;
#endif
}

#endif // LOKI_OS_IMPLEMENTATION
#endif // LOKI_OS_H

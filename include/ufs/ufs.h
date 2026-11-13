/*!
 * µFS - Lightweight filesystem utilities for C++
 *
 * Copyright (c) 2026 Anders Lind
 * All rights reserved.
 *
 * Required Notice: Copyright (c) 2026 Anders Lind (https://truehumandesign.se).
 *
 * This software is available under dual licensing:
 *
 * 1. Noncommercial License:
 *    PolyForm Noncommercial License 1.0.0.
 *    Free for all uses permitted by that license, including
 *    modification and redistribution under its terms.
 *
 * 2. Commercial License:
 *    Commercial use is available under a separate license
 *    granted by the copyright holder.
 *    Contact: https://truehumandesign.se/
 *
 * License: https://polyformproject.org/licenses/noncommercial/1.0.0
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#ifdef _WIN32
  #include <windows.h>
  #include <tchar.h>
#elif defined(__linux__)
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <dirent.h>
  #include <errno.h>
  #include <fcntl.h>
  #include <stdlib.h>
  #include <unistd.h>
#endif

#ifndef UFS_NAMESPACE
namespace ufs {
#else
namespace UFS_NAMESPACE {
#endif

#ifdef _WIN32
using native_error_t = DWORD;

enum : native_error_t {
  file_error_success = ERROR_SUCCESS,
  file_error_cancelled = ERROR_CANCELLED,
  file_error_file_exists = ERROR_FILE_EXISTS,
  file_error_file_not_found = ERROR_FILE_NOT_FOUND,
  file_error_path_not_found = ERROR_PATH_NOT_FOUND,
  file_error_insufficient_buffer = ERROR_INSUFFICIENT_BUFFER,
  file_error_out_of_memory = ERROR_OUTOFMEMORY,
  file_error_file_too_large = ERROR_FILE_TOO_LARGE,
  file_error_unidentified_error = ERROR_UNIDENTIFIED_ERROR,
  file_error_read_fault = ERROR_READ_FAULT,
  file_error_write_fault = ERROR_WRITE_FAULT
};
#else
using native_error_t = int;

enum : native_error_t {
  file_error_success = 0,
  file_error_cancelled = ECANCELED,
  file_error_file_exists = EEXIST,
  file_error_file_not_found = ENOENT,
  file_error_path_not_found = ENOENT,
  file_error_insufficient_buffer = ERANGE,
  file_error_out_of_memory = ENOMEM,
  file_error_file_too_large = EOVERFLOW,
  file_error_unidentified_error = EIO,
  file_error_read_fault = EIO,
  file_error_write_fault = EIO
};
#endif

struct result_t {
  native_error_t code = 0;

  constexpr explicit operator bool() const noexcept {
    return code == file_error_success;
  }

  constexpr bool operator==(native_error_t error) const noexcept {
    return code == error;
  }
};

struct file_info_t {
  uint64_t size;
  uint64_t modified_time; // Nanoseconds since January 1, 1601 UTC until July 21, 2185 UTC.
  bool is_folder;
  bool is_regular_file;
};
#ifdef _WIN32
inline result_t get_file_info(const TCHAR* filename, file_info_t& info) noexcept {
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!::GetFileAttributesEx(filename, GetFileExInfoStandard, &data)) return { ::GetLastError() };
  info.size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  info.modified_time = 100 * ((static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime);
  info.is_folder = (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == FILE_ATTRIBUTE_DIRECTORY;
  info.is_regular_file = (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) == 0;
  return { ERROR_SUCCESS };
}

inline bool path_exists(const TCHAR* path) noexcept {
  return ::GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
}

inline bool is_folder(const TCHAR* filename) noexcept {
  DWORD attr = ::GetFileAttributes(filename);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == FILE_ATTRIBUTE_DIRECTORY;
}

inline bool is_regular_file(const TCHAR* filename) noexcept {
  DWORD attr = ::GetFileAttributes(filename);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) == 0;
}

inline bool is_symbolic_link(const TCHAR* filename) noexcept {
  WIN32_FIND_DATA data;
  HANDLE handle = ::FindFirstFile(filename, &data);
  if (handle == INVALID_HANDLE_VALUE) return false;
  std::ignore = ::FindClose(handle);
  return (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && data.dwReserved0 == IO_REPARSE_TAG_SYMLINK;
}

inline result_t delete_empty_folder(const TCHAR* path) noexcept {
  return { ::RemoveDirectory(path) != 0 ? ERROR_SUCCESS : ::GetLastError() };
}

inline result_t delete_file(const TCHAR* filename) noexcept {
  return { ::DeleteFile(filename) != 0 ? ERROR_SUCCESS : ::GetLastError() };
}

inline result_t move_file(const TCHAR* source, const TCHAR* target) noexcept {
  return { ::MoveFileEx(source, target, MOVEFILE_COPY_ALLOWED) != 0 ? ERROR_SUCCESS : ::GetLastError() };
}

inline result_t copy_file(const TCHAR* source, const TCHAR* target) noexcept {
  return { ::CopyFile(source, target, TRUE) != 0 ? ERROR_SUCCESS : ::GetLastError() };
}

template <typename Traits = std::char_traits<TCHAR>, typename Allocator = std::allocator<TCHAR>>
inline result_t get_current_folder(std::basic_string<TCHAR, Traits, Allocator>& out) noexcept {
  out.clear();

  DWORD required = ::GetCurrentDirectory(0, nullptr);
  if (required == 0) return { ::GetLastError() };

  try {
    std::vector<TCHAR, Allocator> buffer((std::min)((std::numeric_limits<DWORD>::max)() - 16, required) + 16);
    DWORD written = ::GetCurrentDirectory(static_cast<DWORD>(buffer.size()), buffer.data());
    if (written == 0) return { ::GetLastError() };
    if (written >= buffer.size()) return { ERROR_INSUFFICIENT_BUFFER };

    out = std::basic_string<TCHAR, Traits, Allocator>(buffer.data(), written);
    return { ERROR_SUCCESS };
  } catch (const std::bad_alloc&) {
    return { ERROR_OUTOFMEMORY };
  } catch (...) {
    return { ERROR_UNIDENTIFIED_ERROR };
  }
}

template <typename Traits = std::char_traits<TCHAR>, typename Allocator = std::allocator<TCHAR>>
inline result_t get_temp_folder(std::basic_string<TCHAR, Traits, Allocator>& out) noexcept {
  out.clear();

  DWORD required = ::GetTempPath(0, nullptr);
  if (required == 0) return { ::GetLastError() };

  try {
    std::vector<TCHAR, Allocator> buffer((std::min)((std::numeric_limits<DWORD>::max)() - 16, required) + 16);
    DWORD written = ::GetTempPath(static_cast<DWORD>(buffer.size()), buffer.data());
    if (written == 0) return { ::GetLastError() };
    if (written >= buffer.size()) return { ERROR_INSUFFICIENT_BUFFER };

    out = std::basic_string<TCHAR, Traits, Allocator>(buffer.data(), written);
    return { ERROR_SUCCESS };
  } catch (const std::bad_alloc&) {
    return { ERROR_OUTOFMEMORY };
  } catch (...) {
    return { ERROR_UNIDENTIFIED_ERROR };
  }
}

inline result_t create_folder(const TCHAR* path, bool* created = nullptr) noexcept {
  if (::CreateDirectory(path, nullptr)) {
    if (created) *created = true;
  } else {
    native_error_t error = ::GetLastError();
    if (error != ERROR_ALREADY_EXISTS) return { error };

    DWORD attr = ::GetFileAttributes(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return { ::GetLastError() };
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) return { ERROR_FILE_EXISTS };

    if (created) *created = false;
  }
  return { ERROR_SUCCESS };
}

inline result_t create_folders(std::span<const TCHAR* const> paths) noexcept {
  for (const TCHAR* path : paths) {
    if (result_t result = create_folder(path); !result) return result;
  }
  return { ERROR_SUCCESS };
}

template <typename Traits = std::char_traits<TCHAR>, typename Allocator = std::allocator<TCHAR>>
inline result_t create_folders(const TCHAR* root, std::span<const TCHAR* const> names) noexcept {
  try {
    std::basic_string<TCHAR, Traits, Allocator> path(root);
    if (!path.empty() && path.back() != _T('\\') && path.back() != _T('/')) path.push_back(_T('\\'));
    const size_t prefix = path.size();
    for (const TCHAR* name : names) {
      path.resize(prefix);
      if (result_t result = create_folder(path.append(name).c_str()); !result) return result;
    }
    return { ERROR_SUCCESS };
  } catch (const std::bad_alloc&) {
    return { ERROR_OUTOFMEMORY };
  } catch (...) {
    return { ERROR_UNIDENTIFIED_ERROR };
  }
}

template <typename Allocator = std::allocator<char>>
inline result_t read_file(const TCHAR* filename, std::vector<char, Allocator>& out, bool nullTerminate = false) noexcept {
  out.clear();

  HANDLE file = ::CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return { ::GetLastError() };

  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(file, &fileSize)) {
    native_error_t error = ::GetLastError();
    std::ignore = ::CloseHandle(file);
    return { error };
  }
  if (fileSize.QuadPart < 0) {
    std::ignore = ::CloseHandle(file);
    return { ERROR_INVALID_DATA };
  }

  const uint64_t size = static_cast<uint64_t>(fileSize.QuadPart);
  const size_t extra = nullTerminate ? 1 : 0;

  if (size > out.max_size() - extra) {
    std::ignore = ::CloseHandle(file);
    return { ERROR_FILE_TOO_LARGE };
  }

  try {
    out.resize(static_cast<size_t>(size) + extra);
  } catch (const std::bad_alloc&) {
    std::ignore = ::CloseHandle(file);
    return { ERROR_OUTOFMEMORY };
  } catch (...) {
    std::ignore = ::CloseHandle(file);
    return { ERROR_UNIDENTIFIED_ERROR };
  }

  size_t remaining = static_cast<size_t>(size);
  char* dst = out.data();

  while (remaining > 0) {
    constexpr size_t kMaxChunkSize = 0x40000000;
    DWORD chunkSize = static_cast<DWORD>((std::min)(remaining, kMaxChunkSize));

    DWORD bytesRead = 0;
    if (!::ReadFile(file, dst, chunkSize, &bytesRead, nullptr)) {
      native_error_t error = ::GetLastError();
      std::ignore = ::CloseHandle(file);
      out.clear();
      return { error };
    }
    if (bytesRead == 0) {
      std::ignore = ::CloseHandle(file);
      out.clear();
      return { ERROR_READ_FAULT };
    }

    dst += bytesRead;
    remaining -= bytesRead;
  }

  if (nullTerminate) out[static_cast<size_t>(size)] = '\0';

  std::ignore = ::CloseHandle(file);
  return { ERROR_SUCCESS };
}

inline result_t write_file(const TCHAR* filename, std::span<const char> data) noexcept {
  HANDLE file = ::CreateFile(filename, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return { ::GetLastError() };

  size_t remaining = data.size();
  const char* src = data.data();

  while (remaining > 0) {
    constexpr size_t kMaxChunkSize = 0x40000000;
    DWORD chunkSize = static_cast<DWORD>((std::min)(remaining, kMaxChunkSize));

    DWORD written = 0;
    if (!::WriteFile(file, src, chunkSize, &written, nullptr)) {
      native_error_t error = ::GetLastError();
      std::ignore = ::CloseHandle(file);
      std::ignore = ::DeleteFile(filename);
      return { error };
    }
    if (written == 0) {
      std::ignore = ::CloseHandle(file);
      std::ignore = ::DeleteFile(filename);
      return { ERROR_WRITE_FAULT };
    }
    src += written;
    remaining -= written;
  }

  if (!::CloseHandle(file)) {
    native_error_t error = ::GetLastError();
    std::ignore = ::DeleteFile(filename);
    return { error };
  }
  return { ERROR_SUCCESS };
}

inline result_t write_file_append(const TCHAR* filename, std::span<const char> data) noexcept {
  HANDLE file = ::CreateFile(filename, FILE_APPEND_DATA, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return { ::GetLastError() };

  size_t remaining = data.size();
  const char* src = data.data();

  while (remaining > 0) {
    constexpr size_t kMaxChunkSize = 0x40000000;
    DWORD chunkSize = static_cast<DWORD>((std::min)(remaining, kMaxChunkSize));

    DWORD written = 0;
    if (!::WriteFile(file, src, chunkSize, &written, nullptr)) {
      native_error_t error = ::GetLastError();
      std::ignore = ::CloseHandle(file);
      return { error };
    }
    if (written == 0) {
      std::ignore = ::CloseHandle(file);
      return { ERROR_WRITE_FAULT };
    }
    src += written;
    remaining -= written;
  }

  if (!::CloseHandle(file)) return { ::GetLastError() };
  return { ERROR_SUCCESS };
}

template <typename FolderFunc, typename Traits = std::char_traits<TCHAR>, typename Allocator = std::allocator<TCHAR>>
inline result_t visit_folders(const TCHAR* path, FolderFunc onFolder) noexcept {
  native_error_t error;
  try {
    std::basic_string<TCHAR, Traits, Allocator> search(path);
    if (!search.empty() && search.back() != _T('\\') && search.back() != _T('/')) search.push_back(_T('\\'));
    std::basic_string<TCHAR, Traits, Allocator> rootPath(search);
    search.push_back(_T('*'));

    WIN32_FIND_DATA data;
    HANDLE handle = ::FindFirstFileEx(search.c_str(), FindExInfoBasic, &data, FindExSearchLimitToDirectories, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE) return { ::GetLastError() };

    try {
      do {
        const TCHAR* name = data.cFileName;
        if (_tcscmp(name, _T(".")) == 0 || _tcscmp(name, _T("..")) == 0) continue;

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && !onFolder(rootPath.c_str(), name, &data)) {
          std::ignore = ::FindClose(handle);
          return { ERROR_CANCELLED };
        }
      } while (::FindNextFile(handle, &data));
    } catch (const std::bad_alloc&) {
      std::ignore = ::FindClose(handle);
      return { ERROR_OUTOFMEMORY };
    } catch (...) {
      std::ignore = ::FindClose(handle);
      return { ERROR_UNIDENTIFIED_ERROR };
    }
    error = ::GetLastError();
    std::ignore = ::FindClose(handle);
  } catch (const std::bad_alloc&) {
    return { ERROR_OUTOFMEMORY };
  } catch (...) {
    return { ERROR_UNIDENTIFIED_ERROR };
  }

  if (error != ERROR_NO_MORE_FILES) return { error };
  return { ERROR_SUCCESS };
}

template <typename FileFunc, typename Traits = std::char_traits<TCHAR>, typename Allocator = std::allocator<TCHAR>>
inline result_t visit_files(const TCHAR* path, FileFunc onFile, bool skipFolders = true) noexcept {
  native_error_t error;
  try {
    std::basic_string<TCHAR, Traits, Allocator> search(path);
    if (!search.empty() && search.back() != _T('\\') && search.back() != _T('/')) search.push_back(_T('\\'));
    std::basic_string<TCHAR, Traits, Allocator> rootPath(search);
    search.push_back(_T('*'));

    WIN32_FIND_DATA data;
    HANDLE handle = ::FindFirstFileEx(search.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE) return { ::GetLastError() };

    try {
      do {
        const TCHAR* name = data.cFileName;
        if (_tcscmp(name, _T(".")) == 0 || _tcscmp(name, _T("..")) == 0) continue;

        if ((!skipFolders || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) && !onFile(rootPath.c_str(), name, &data)) {
          std::ignore = ::FindClose(handle);
          return { ERROR_CANCELLED };
        }
      } while (::FindNextFile(handle, &data));
    } catch (const std::bad_alloc&) {
      std::ignore = ::FindClose(handle);
      return { ERROR_OUTOFMEMORY };
    } catch (...) {
      std::ignore = ::FindClose(handle);
      return { ERROR_UNIDENTIFIED_ERROR };
    }
    error = ::GetLastError();
    std::ignore = ::FindClose(handle);
  } catch (const std::bad_alloc&) {
    return { ERROR_OUTOFMEMORY };
  } catch (...) {
    return { ERROR_UNIDENTIFIED_ERROR };
  }

  if (error != ERROR_NO_MORE_FILES) return { error };
  return { ERROR_SUCCESS };
}
#else
inline result_t get_file_info(const char* filename, file_info_t& info) noexcept {
  struct stat st;
  if (::lstat(filename, &st) != 0) return { errno };

  constexpr uint64_t kEpochOffset = 11644473600000000000ull;
  info.size = st.st_size >= 0 ? static_cast<uint64_t>(st.st_size) : 0;
  info.modified_time = kEpochOffset + uint64_t(st.st_mtim.tv_sec) * 1000000000ull + st.st_mtim.tv_nsec;
  info.is_folder = S_ISDIR(st.st_mode);
  info.is_regular_file = S_ISREG(st.st_mode);
  return { 0 };
}

inline bool path_exists(const char* path) noexcept {
  struct stat st;
  return ::lstat(path, &st) == 0;
}

inline bool is_folder(const char* path) noexcept {
  struct stat st;
  return ::lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

inline bool is_regular_file(const char* path) noexcept {
  struct stat st;
  return ::lstat(path, &st) == 0 && S_ISREG(st.st_mode);
}

inline bool is_symbolic_link(const char* path) noexcept {
  struct stat st;
  return ::lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

inline result_t delete_empty_folder(const char* path) noexcept {
  return { ::rmdir(path) == 0 ? 0 : errno };
}

inline result_t delete_file(const char* filename) noexcept {
  return { ::unlink(filename) == 0 ? 0 : errno };
}

inline result_t move_file(const char* source, const char* target) noexcept {
  return { ::renameat2(AT_FDCWD, source, AT_FDCWD, target, RENAME_NOREPLACE) == 0 ? 0 : errno };
}

inline result_t copy_file(const char* source, const char* target, int mode = 0644) noexcept {
  int src = ::open(source, O_RDONLY);
  if (src < 0) return { errno };

  int dst = ::open(target, O_WRONLY | O_CREAT | O_EXCL, mode);
  if (dst < 0) {
    native_error_t error = errno;
    std::ignore = ::close(src);
    return { error };
  }

  char buffer[64 * 1024];

  int read_retries = 0;
  while (true) {
    ssize_t bytesRead = ::read(src, buffer, sizeof(buffer));
    if (bytesRead == 0) break;
    if (bytesRead < 0) {
      native_error_t error = errno;
      if (error == EINTR && read_retries++ < 5) continue;
      std::ignore = ::close(src);
      std::ignore = ::close(dst);
      std::ignore = ::unlink(target);
      return { error };
    }

    read_retries = 0;
    size_t remaining = static_cast<size_t>(bytesRead);
    const char* data = buffer;

    int write_retries = 0;
    while (remaining > 0) {
      ssize_t written = ::write(dst, data, remaining);
      if (written <= 0) {
        native_error_t error = written < 0 ? errno : EIO;
        if (error == EINTR && write_retries++ < 5) continue;
        std::ignore = ::close(src);
        std::ignore = ::close(dst);
        std::ignore = ::unlink(target);
        return { error };
      }

      write_retries = 0;
      data += written;
      remaining -= static_cast<size_t>(written);
    }
  }

  std::ignore = ::close(src);
  if (::close(dst) != 0) {
    native_error_t error = errno;
    std::ignore = ::unlink(target);
    return { error };
  }
  return { 0 };
}

inline result_t create_folder(const char* path, bool* created = nullptr, int mode = 0755) noexcept {
  if (::mkdir(path, mode) == 0) {
    if (created) *created = true;
  } else {
    native_error_t error = errno;
    if (error != EEXIST) return { error };

    struct stat st;
    if (::stat(path, &st) != 0) return { errno };
    if (!S_ISDIR(st.st_mode)) return { EEXIST };

    if (created) *created = false;
  }
  return { 0 };
}

inline result_t create_folders(std::span<const char* const> paths, int mode = 0755) noexcept {
  for (const char* path : paths) {
    if (result_t result = create_folder(path, nullptr, mode); !result) return result;
  }
  return { 0 };
}

template <typename Traits = std::char_traits<char>, typename Allocator = std::allocator<char>>
inline result_t create_folders(const char* root, std::span<const char* const> names, int mode = 0755) noexcept {
  try {
    std::basic_string<char, Traits, Allocator> path(root);
    if (!path.empty() && path.back() != '/') path.push_back('/');
    const size_t prefix = path.size();
    for (const char* name : names) {
      path.resize(prefix);
      path += name;
      if (result_t result = create_folder(path.c_str(), nullptr, mode); !result) return result;
    }
    return { 0 };
  } catch (const std::bad_alloc&) {
    return { ENOMEM };
  } catch (...) {
    return { EIO };
  }
}

template <typename Traits = std::char_traits<char>, typename Allocator = std::allocator<char>>
inline result_t get_current_folder(std::basic_string<char, Traits, Allocator>& out) noexcept {
  out.clear();
  try {
    std::vector<char, Allocator> buffer(256);
    while (true) {
      if (::getcwd(buffer.data(), buffer.size())) {
        out.assign(buffer.data());
        break;
      }
      native_error_t error = errno;
      if (error != ERANGE) return { error };
      constexpr auto kMaxBufferSize = buffer.max_size();
      if (buffer.size() >= kMaxBufferSize) return { ERANGE };
      buffer.resize((buffer.size() <= kMaxBufferSize / 2) ? buffer.size() * 2 : kMaxBufferSize);
    }
    return { 0 };
  } catch (const std::bad_alloc&) {
    return { ENOMEM };
  } catch (...) {
    return { EIO };
  }
}

template <typename Traits = std::char_traits<char>, typename Allocator = std::allocator<char>>
inline result_t get_temp_folder(std::basic_string<char, Traits, Allocator>& out) noexcept {
  out.clear();

  const char* path = ::getenv("TMPDIR");
  if (!path || !*path) path = "/tmp";

  try {
    out = path;
    return { 0 };
  } catch (const std::bad_alloc&) {
    return { ENOMEM };
  } catch (...) {
    return { EIO };
  }
}

template <typename Allocator = std::allocator<char>>
inline result_t read_file(const char* filename, std::vector<char, Allocator>& out, bool nullTerminate = false) noexcept {
  out.clear();

  int fd = ::open(filename, O_RDONLY);
  if (fd < 0) return { errno };

  struct stat st;
  if (::fstat(fd, &st) != 0) {
    native_error_t error = errno;
    std::ignore = ::close(fd);
    return { error };
  }
  if (st.st_size < 0) {
    std::ignore = ::close(fd);
    return { EINVAL };
  }

  const uint64_t size = static_cast<uint64_t>(st.st_size);
  const size_t extra = nullTerminate ? 1 : 0;

  if (size > out.max_size() - extra) {
    std::ignore = ::close(fd);
    return { EOVERFLOW };
  }

  try {
    out.resize(static_cast<size_t>(size) + extra);
  } catch (const std::bad_alloc&) {
    std::ignore = ::close(fd);
    return { ENOMEM };
  } catch (...) {
    std::ignore = ::close(fd);
    return { EIO };
  }

  size_t remaining = static_cast<size_t>(size);
  char* dst = out.data();

  int retries = 0;
  while (remaining > 0) {
    constexpr size_t kMaxChunkSize = 0x40000000;
    const size_t chunkSize = (std::min)(remaining, kMaxChunkSize);

    ssize_t bytesRead = ::read(fd, dst, chunkSize);
    if (bytesRead < 0 && errno == EINTR && retries++ < 5) continue;

    if (bytesRead <= 0) {
      native_error_t error = (bytesRead < 0) ? errno : EIO;
      std::ignore = ::close(fd);
      out.clear();
      return { error };
    }

    retries = 0;
    dst += bytesRead;
    remaining -= static_cast<size_t>(bytesRead);
  }

  if (nullTerminate) out[static_cast<size_t>(size)] = '\0';

  std::ignore = ::close(fd);
  return { 0 };
}

inline result_t write_file(const char* filename, std::span<const char> data, int mode = 0644) noexcept {
  int fd = ::open(filename, O_WRONLY | O_CREAT | O_EXCL, mode);
  if (fd < 0) return { errno };

  size_t remaining = data.size();
  const char* src = data.data();

  int retries = 0;
  while (remaining > 0) {
    constexpr size_t kMaxChunkSize = 0x40000000;
    size_t chunkSize = (std::min)(remaining, kMaxChunkSize);

    ssize_t written = ::write(fd, src, chunkSize);
    if (written < 0 && errno == EINTR && retries++ < 5) continue;

    if (written <= 0) {
      native_error_t error = (written < 0) ? errno : EIO;
      std::ignore = ::close(fd);
      std::ignore = ::unlink(filename);
      return { error };
    }

    retries = 0;
    src += written;
    remaining -= static_cast<size_t>(written);
  }

  if (::close(fd) != 0) {
    native_error_t error = errno;
    std::ignore = ::unlink(filename); 
    return { error };
  }
  return { 0 };
}

inline result_t write_file_append(const char* filename, std::span<const char> data) noexcept {
  int fd = ::open(filename, O_WRONLY | O_APPEND);
  if (fd < 0) return { errno };

  size_t remaining = data.size();
  const char* src = data.data();

  int retries = 0;
  while (remaining > 0) {
    constexpr size_t kMaxChunkSize = 0x40000000;
    size_t chunkSize = (std::min)(remaining, kMaxChunkSize);

    ssize_t written = ::write(fd, src, chunkSize);
    if (written < 0 && errno == EINTR && retries++ < 5) continue;

    if (written <= 0) {
      native_error_t error = (written < 0) ? errno : EIO;
      std::ignore = ::close(fd);
      return { error };
    }

    retries = 0;
    src += written;
    remaining -= static_cast<size_t>(written);
  }

  if (::close(fd) != 0) return { errno };
  return { 0 };
}

template <typename FolderFunc, typename Traits = std::char_traits<char>, typename Allocator = std::allocator<char>>
inline result_t visit_folders(const char* path, FolderFunc onFolder) noexcept {
  DIR* dir = ::opendir(path);
  if (!dir) return { errno };

  native_error_t error = 0;
  try {
    std::basic_string<char, Traits, Allocator> rootPath(path);
    if (!rootPath.empty() && rootPath.back() != '/') rootPath.push_back('/');
    while (true) {
      errno = 0;
      struct dirent* entry = ::readdir(dir);
      if (!entry) {
        error = errno;
        break;
      }

      const char* name = entry->d_name;
      if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) continue;

      struct stat st;
      if (::fstatat(::dirfd(dir), name, &st, 0) != 0 && ::fstatat(::dirfd(dir), name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        error = errno;
        break;
      }

      if (S_ISDIR(st.st_mode) && !onFolder(rootPath.c_str(), name, &st)) {
        error = ECANCELED;
        break;
      }
    }
  } catch (const std::bad_alloc&) {
    error = ENOMEM;
  } catch (...) {
    error = EIO;
  }
  std::ignore = ::closedir(dir);
  return { error };
}

template <typename FileFunc, typename Traits = std::char_traits<char>, typename Allocator = std::allocator<char>>
inline result_t visit_files(const char* path, FileFunc onFile, bool skipFolders = true) noexcept {
  DIR* dir = ::opendir(path);
  if (!dir) return { errno };

  native_error_t error = 0;
  try {
    std::basic_string<char, Traits, Allocator> rootPath(path);
    if (!rootPath.empty() && rootPath.back() != '/') rootPath.push_back('/');
    while (true) {
      errno = 0;
      struct dirent* entry = ::readdir(dir);
      if (!entry) {
        error = errno;
        break;
      }

      const char* name = entry->d_name;
      if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) continue;

      struct stat st;
      if (::fstatat(::dirfd(dir), name, &st, 0) != 0 && ::fstatat(::dirfd(dir), name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        error = errno;
        break;
      }

      if ((!skipFolders || !S_ISDIR(st.st_mode)) && !onFile(rootPath.c_str(), name, &st)) {
        error = ECANCELED;
        break;
      }
    }
  } catch (const std::bad_alloc&) {
    error = ENOMEM;
  } catch (...) {
    error = EIO;
  }
  std::ignore = ::closedir(dir);
  return { error };
}
#endif
} // namespace ufs

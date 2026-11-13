# µFS

Lightweight, header-only filesystem utilities for Windows and Linux, written in modern C++.

## Overview

**µFS** is a small, cross-platform filesystem utility library built directly on the native Win32 and POSIX APIs.

Designed with simplicity and minimal overhead in mind, µFS provides common file and directory operations without external dependencies or complex abstractions.

## Features

- **Cross-platform** - Windows (Win32) and Linux (POSIX).
- **Single header** - All functionality is contained in `ufs.h`.
- **Minimal dependencies** - Only the C++ standard library and native OS APIs.
- **File I/O** - Read binary files, create new files, and append to existing files.
- **File operations** - Copy, move, and delete files.
- **Filesystem queries** - Check paths, file types, sizes, and modification times.
- **Directory management** - Create folders, remove empty folders, and query common directories.
- **Directory enumeration** - Visit folders and files using callbacks.
- **Error handling** - Simple return values and error codes; no exceptions propagated to callers.

## Requirements

- C++20 or later
- Windows or Linux
- No external libraries

## Usage

Include the header:

```cpp
#include "ufs.h"
```

Read a file into memory:

```cpp
std::vector<char> data;
if (ufs::read_file("input.png", data)) {
  // process data...
}
```

Create a new file (fails if it already exists):

```cpp
if (!ufs::write_file("output.png", data)) {
  // handle error...
}
```

Append to an existing file:

```cpp
if (!ufs::write_file("output.png", ihdr)) {
  // handle error...
}
if (!ufs::write_file_append("output.png", idat)) {
  // handle error...
}
if (!ufs::write_file_append("output.png", iend)) {
  // handle error...
}
```

Query a file:

```cpp
ufs::file_info_t info;
if (ufs::get_file_info("~/.bashrc", info)) {
  // info.size
  // info.modified_time
  // info.is_folder
  // info.is_regular_file
}
```

Enumerate files:

```cpp
ufs::visit_files("~/Desktop", [](const char* path, const char* name, void* data) {
  // process file...
  return true;
});
```

Directory enumeration is nonrecursive. Returning `false` from a callback stops enumeration.

## Configuration

The default namespace is `ufs`, but it can be customized before including the header:

```cpp
#define UFS_NAMESPACE myfs
#include "ufs.h"
```

Windows paths use `TCHAR`, supporting both ANSI and Unicode builds. Linux paths use `char`.

## License

See the license terms included in `ufs.h`.

# zipfile-deflate64
[![PyPI](https://img.shields.io/pypi/v/zipfile-deflate64)](https://pypi.org/project/zipfile-deflate64/)

Extract Deflate64 ZIP archives with Python's `zipfile` API.

## Installation
```bash
pip install zipfile-deflate64
```

Python 3.6, 3.7, 3.8, and 3.9 are supported,
with [manylinux2014](https://github.com/pypa/manylinux), macOS and Windows wheels published to PyPI.

## Usage
Anywhere in a Python codebase:
```python
import zipfile_deflate64  # This has the side effect of patching the zipfile module to support Deflate64
```

Alternatively, `zipfile_deflate64` re-exports the `zipfile` API, as a convenience:
```python
import zipfile_deflate64 as zipfile

zipfile.ZipFile(...)
...
```

## Design Rationale
### The Problem
Recent versions of Microsoft Windows Explorer
[use Deflate64 compression when creating ZIP files larger than 2GB](https://github.com/dotnet/runtime/issues/17802#issuecomment-231808916).
With the ubiquity of Windows and the ease of using "Sent to compressed folder", a majority of newly-created large
ZIP files use Deflate64 compression.

However, **support for Deflate64 in the open-source ecosystem is very poor**!
Most ZIP libraries have declined to implement Deflate64,
citing [its proprietary nature](https://en.wikipedia.org/wiki/Deflate#Deflate64/Enhanced_Deflate).

In the .NET ecosystem, the [`ZipArchive` API supports decompression only](https://github.com/dotnet/corefx/pull/11264).
In Java, the [Apache Commons Compress APIs support both compression and decompression](https://commons.apache.org/proper/commons-compress/examples.html#Archivers_and_Compressors).

The 7-Zip project probably provides the best general-purpose support for compressing and decompressing
Deflate64, but there are several obstacles to general usability:
* [7-Zip itself](https://www.7-zip.org/) is a Windows-only GUI application
  * 7-Zip is still issuing new releases, but has declined to implement certain new compression formats,
    so the [mcmilk/7-Zip-zstd](https://github.com/mcmilk/7-Zip-zstd) fork is notable.
* [p7zip, the POSIX-compatible CLI version](http://p7zip.sourceforge.net/) (which does include Deflate64),
  [has not had a release since 2016 and is likely unmaintained](https://github.com/jinfeihan57/p7zip/issues/114#issuecomment-761551564).
* p7zip does not build an API for external software to invoke for decompression.
* p7zip seems to now be living on as the [jinfeihan57/p7zip](https://github.com/jinfeihan57/p7zip) fork,
  which is packaged by Arch Linux, amongst others.
  * This seems to be active, and now can be built with CMake, but there's no support for building an external API.
* Many re-implementations of 7-Zip, such as [py7zr](https://github.com/miurahr/py7zr) for Python, do not support
  Deflate64.

In the Python ecosystem in particular, there have been several unfulfilled requests (
[[1]](https://github.com/UCL-ShippingGroup/pyrate/issues/33)
[[2]](https://www.reddit.com/r/learnpython/comments/iqr6eb/zip_files_with_compression_type_9_deflate64/)
[[3]](https://stackoverflow.com/a/12809847)
) for Deflate64 decompression support.

### A Solution
The best hope seems to be the [infback9](https://github.com/madler/zlib/tree/master/contrib/infback9) extension
to zlib. This was developed by Mark Adler, the original author of zlib, and is kept in the source repository of zlib,
but it is not officially supported and contains no build tooling and is not distributed with zlib packages.
Additionally, infback9 provides only low-level support for working with Deflate64 bitstreams, with no support for
the ZIP archive format (which is out of scope for zlib).

infback9's C-language API is relatively simple, but requires a non-trivial struct and function pointers for
initialization and some explicit memory management operations (resizing allocated buffers and proving a
Python-friendly `malloc`) to operate efficiently, so wrapping it with only
[ctypes](https://docs.python.org/3/library/ctypes.html) seems to be inadequate.

To manage ZIP archive extraction operations, the Python standard library
[zipfile](https://docs.python.org/3/library/zipfile.html) module provides the essential features and is already
ubiquitous in availability and usage. However, zipfile is difficult to extend, as it hardcodes many conditionals for
compression formats and does not provide capabilities for easily augmenting or replacing parts of it. Monkey-patching
can overcome some of these problems, and the promise of a drop-in, API-compatible patch to a standard library module
outweighed the engineering benefits of basing a solution off a more naturally extensible third-party ZIP manipulation
package.

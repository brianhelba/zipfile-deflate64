import zipfile
import inspect

from . import deflate64
from ._patcher import patch

try:
    import codecs7z
except ImportError:
    codecs7z = None

# Since none of the public API of zipfile needs to be patched, we don't have to worry about
# ensuring that this is prior to other code importing things from zipfile.

# This is already defined in zipfile.compressor_names, for error-handling purposes
zipfile.ZIP_DEFLATED64 = 9  # type: ignore[attr-defined]
zipfile.compressor_names[zipfile.ZIP_DEFLATED64] = 'deflate64'
zipfile.DEFLATED64_VERSION = 21

@patch(zipfile, '_check_compression')
def deflate64_check_compression(compression: int) -> None:
    if compression == zipfile.ZIP_DEFLATED64:  # type: ignore[attr-defined]
        pass
    else:
        patch.originals['_check_compression'](compression)


@patch(zipfile, '_get_decompressor')
def deflate64_get_decompressor(compress_type: int):
    if compress_type == zipfile.ZIP_DEFLATED64:  # type: ignore[attr-defined]
        return deflate64.Deflate64()
    else:
        return patch.originals['_get_decompressor'](compress_type)


@patch(zipfile.ZipExtFile, '__init__')
def deflate64_ZipExtFile_init(self, *args, **kwarg):  # noqa: N802
    patch.originals['__init__'](self, *args, **kwarg)
    if self._compress_type == zipfile.ZIP_DEFLATED64:
        self.MIN_READ_SIZE = 64 * 2 ** 10

if 'compresslevel' in inspect.signature(zipfile._get_compressor).parameters:
    @patch(zipfile, '_get_compressor')
    def deflate64_get_compressor(compress_type, compresslevel=None):
        if compress_type == zipfile.ZIP_DEFLATED64:
            if compresslevel is None:
                compresslevel = 6
            assert codecs7z is not None
            return codecs7z.deflate64_compressobj(compresslevel)
        else:
            return patch.originals['_get_compressor'](compress_type, compresslevel=compresslevel)
else:
    @patch(zipfile, '_get_compressor')
    def deflate64_get_compressor(compress_type, compresslevel=None):
        if compress_type == zipfile.ZIP_DEFLATED64:
            if compresslevel is None:
                compresslevel = 6
            assert codecs7z is not None
            return codecs7z.deflate64_compressobj(compresslevel)
        else:
            return patch.originals['_get_compressor'](compress_type)


@patch(zipfile.ZipInfo, 'FileHeader')
def deflate64_FileHeader(self, zip64=None):
    if self.compress_type == zipfile.ZIP_DEFLATED64:
        self.create_version = max(self.create_version, zipfile.DEFLATED64_VERSION)
        self.extract_version = max(self.extract_version, zipfile.DEFLATED64_VERSION)
    return patch.originals['FileHeader'](self, zip64=zip64)

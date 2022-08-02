import inflate64


class Decompressor:
    def __init__(self):
        self._decompressor = inflate64.Inflater()

    def decompress(self, data):
        return self._decompressor.inflate(data)

    @property
    def eof(self):
        return self._decompressor.eof


class Compressor:
    def __init__(self):
        self._eof = False
        self._compressor = inflate64.Deflater()

    def compress(self, data):
        return self._compressor.deflate(data)

    def flush(self):
        self._eof = True
        return self._compressor.flush()

    @property
    def eof(self):
        return self._eof

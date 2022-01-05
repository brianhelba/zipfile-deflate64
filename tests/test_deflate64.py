import re

import pytest

from zipfile_deflate64.deflate64 import Deflate64


@pytest.fixture
def deflate64():
    return Deflate64()


def test_instantiate():
    deflate64 = Deflate64()
    assert isinstance(deflate64, Deflate64)


@pytest.mark.parametrize(
    'content_file_name',
    [
        '10_lines.deflate64',
        '100_lines.deflate64',
        '10k_lines.deflate64',
        '100k_lines.deflate64',
    ],
)
def test_decompress_content(data_dir, deflate64, content_file_name):
    with open(data_dir / content_file_name, 'rb') as compressed_content_stream:
        decompressed_content = deflate64.decompress(compressed_content_stream.read())

    assert re.match(rb'^(?:Sample content \d+\.\n)+$', decompressed_content)


def test_decompress_output_type(data_dir, deflate64):
    with open(data_dir / '10_lines.deflate64', 'rb') as compressed_content_stream:
        decompressed_content = deflate64.decompress(compressed_content_stream.read())
    assert isinstance(decompressed_content, bytes)


def test_decompress_repeated(data_dir, deflate64):
    """Ensure that Deflate64.decompress can be called repeatedly on a compressed stream."""
    read_size = 64 * 2 ** 10
    decompressed_content = b''
    with open(data_dir / '100k_lines.deflate64', 'rb') as compressed_content_stream:
        while True:
            compressed_content = compressed_content_stream.read(read_size)
            if not compressed_content:
                break
            decompressed_content += deflate64.decompress(compressed_content)

    assert len(decompressed_content) == 2188890
    assert re.match(rb'^(?:Sample content \d+\.\n)+$', decompressed_content)


def test_decompress_empty(deflate64):
    decompressed_content = deflate64.decompress(b'')
    assert decompressed_content == b''


def test_decompress_invalid(deflate64):
    with pytest.raises(ValueError, match=r'^Bad Deflate64 data: '):
        deflate64.decompress(b'garbage')

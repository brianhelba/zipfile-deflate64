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

    assert re.match(r'^(?:Sample content \d+\.\n)+$', decompressed_content.decode())


def test_decompress_output_type(data_dir, deflate64):
    with open(data_dir / '10_lines.deflate64', 'rb') as compressed_content_stream:
        decompressed_content = deflate64.decompress(compressed_content_stream.read())
    assert isinstance(decompressed_content, bytes)


def test_decompress_empty(deflate64):
    decompressed_content = deflate64.decompress(b'')
    assert decompressed_content == b''


def test_decompress_invalid(deflate64):
    with pytest.raises(ValueError, match=r'^Bad Deflate64 data: '):
        deflate64.decompress(b'garbage')

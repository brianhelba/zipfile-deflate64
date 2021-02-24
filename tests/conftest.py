import pathlib

import pytest


@pytest.fixture
def data_dir():
    return pathlib.Path(__file__).parent / 'data'

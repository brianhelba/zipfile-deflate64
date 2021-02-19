from pathlib import Path

from setuptools import Extension, setup

readme_file = Path(__file__).parent / 'README.md'
with readme_file.open() as f:
    long_description = f.read()

setup(
    name='zipfile-deflate64',
    version='0.0.1',
    description='Extract DEFLATE64 ZIP archives with Python\'s zipfile API.',
    long_description=long_description,
    long_description_content_type='text/markdown',
    license='Apache 2.0',
    url='https://github.com/brianhelba/zipfile-deflate64',
    project_urls={
        'Bug Reports': 'https://github.com/brianhelba/zipfile-deflate64/issues',
        'Source': 'https://github.com/brianhelba/zipfile-deflate64',
    },
    author='Kitware, Inc.',
    author_email='kitware@kitware.com',
    keywords='zip zipfile deflate deflate64',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python',
        'Topic :: System :: Archiving',
        'Topic :: System :: Archiving :: Compression',
    ],
    python_requires='>=3.6',
)

import sys

from importlib_metadata import entry_points

try:
    from setuptools import setup
except ImportError:
    print('Error: this package requires setuptools to install; try `python3 -m pip install setuptools`', file=sys.stderr)
    sys.exit(1)

try:
    import bcc
except ImportError:
    print('Error: this package requires BCC Python bindings; see https://github.com/iovisor/bcc/blob/master/INSTALL.md')

setup(
    name='simple-lock',
    version='0.1.0',
    url='https://github.com/YangHanlin/simple-lock',
    author='Yang Hanlin',
    author_email='mattoncis@hotmail.com',
    packages=[
        'simple_lock',
    ],
    package_data={
        'simple_lock': [
            'resources/**',
        ],
    },
    entry_points={
        'console_scripts': [
            'simple-lock=simple_lock.cli:main'
        ],
    },
    install_requires=[
        # No dependency
    ],
)

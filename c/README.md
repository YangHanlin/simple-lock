# simple-lock (C)

## Installation

1. Ensure `zlib` and `libelf` are installed:
   ```bash
   sudo apt install -y libz-dev libelf-dev
   ```
2. Clone and build:
   ```bash
   git clone https://github.com/YangHanlin/simple-lock.git --recursive
   mkdir -p simple-lock/c/build
   cd simple-lock/c/build
   cmake ..
   make
   ```
3. Install:
   ```bash
   sudo cp ./simple-lock /usr/local/bin/
   ```

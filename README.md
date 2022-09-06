# simple-lock

## Installation

1. Install [BCC](https://github.com/iovisor/bcc) and its Python bindings;
2. Run command:
   ```
   sudo python3 -m pip install git+https://github.com/YangHanlin/simple-lock.git@main
   ```

## Usage

1. Before first run, use `sudo simple-lock setup` to make it remember the USB device to be used as a key;
2. Run `sudo simple-lock monitor` to start the daemon that monitors additions/removals of USB devices and unlocks/locks the device accordingly.

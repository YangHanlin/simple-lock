import time
from threading import Thread

from . import common


def main(context: dict):
    current_device_id = common.get_config('device_id')
    if current_device_id:
        print(f'Currently configured device ID is \'{current_device_id}\'')
    else:
        print('Currently device ID is not configured')
    watcher = common.DeviceWatcher()
    def event_listener(event):
        device_id = event.device_id.decode()
        common.set_config('device_id', device_id)
        print(f'Configured new device ID \'{device_id}\'')
        Thread(target=lambda: watcher.stop()).start()
    watcher.event_listeners.append(event_listener)
    watcher.start()
    print('Specify a new USB device by plugging or un-plugging it; press Ctrl + C to exit')
    try:
        while True:
            time.sleep(1)
            if not watcher.running:
                break
    except KeyboardInterrupt:
        print('Exiting; currently configured device ID is not changed')
        watcher.stop()
        exit()


if __name__ == '__main__':
    main({})

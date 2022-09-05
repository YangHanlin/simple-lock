import os

from . import common


def main(context: dict):
    device_id = common.get_config('device_id')
    if not device_id:
        print('Cannot find configured device ID; please run `sudo simple-lock setup` to set up before first run')
        exit(1)
    watcher = common.DeviceWatcher()
    def event_listener(event):
        if event.device_id.decode() != device_id:
            return
        if event.type == common.DeviceEventType.DEVICE_ATTACHED.value:
            os.system('loginctl unlock-session')
        elif event.type == common.DeviceEventType.DEVICE_DETACHED.value:
            os.system('loginctl lock-session')
    watcher.event_listeners.append(event_listener)
    watcher.start()
    print('Daemon started; press Ctrl + C to exit')
    try:
        while True:
            input()
    except KeyboardInterrupt:
        watcher.stop()
        print('Daemon stopped')
        exit()


if __name__ == '__main__':
    main({})

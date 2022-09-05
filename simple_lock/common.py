from enum import Enum
from threading import Thread
from bcc import BPF
import json
import os
import time
from typing import Callable, Optional, TextIO

DEFAULT_CONFIG_PATH = os.path.expanduser('~/.simple-lock-config.json')
DEFAULT_COMPANION_SOURCE_PATH = os.path.normpath(os.path.dirname(__file__) + '/resources/simple-lock-companion.bpf.c')


def _open_config_file(path: Optional[str] = None, *args, **kwargs) -> TextIO:
    path_nonnull = path or DEFAULT_CONFIG_PATH
    if not os.path.exists(path_nonnull):
        with open(path_nonnull, 'w') as f:
            json.dump({}, f)
    return open(path_nonnull, *args, **kwargs)


def load_config(path: Optional[str] = None) -> dict:
   with _open_config_file(path, 'r') as f:
        return json.load(f)


def save_config(config: dict, path: Optional[str] = None) -> None:
    with _open_config_file(path, 'w') as f:
        return json.dump(config, f)


def get_config(key: str, default = None, path: Optional[str] = None):
    config = load_config(path)
    return config.get(key, default)


# FIXME: Concurrency issues
def set_config(key: str, value, path: Optional[str] = None) -> None:
    config = load_config(path)
    config[key] = value
    save_config(config, path)


class DeviceEventType(Enum):
    DEVICE_ATTACHED = 1
    DEVICE_DETACHED = 2


class DeviceWatcher:
    def __init__(self, companion_source_path: str = DEFAULT_COMPANION_SOURCE_PATH) -> None:
        self._companion_source_path = companion_source_path
        self._running = False
        self._thread = None
        self.event_listeners: list[Callable] = []

    @property
    def running(self) -> bool:
        return self._running

    def start(self):
        if self._running or self._thread is not None:
            raise RuntimeError('current watcher has been started or is stopping')
        bpf = BPF(src_file=self._companion_source_path)
        def event_handler(cpu, data, size):
            event = bpf['device_events'].event(data)
            for event_listener in self.event_listeners:
                event_listener(event)
        bpf['device_events'].open_perf_buffer(event_handler)
        def run_async():
            while self._running:
                bpf.perf_buffer_poll(100)
        self._thread = Thread(target=run_async)
        self._running = True
        self._thread.start()

    def stop(self):
        if not self._running or self._thread is None:
            raise RuntimeError('current watcher has been stopped or is starting')
        self._running = False
        for _ in range(30):
            time.sleep(1)
            if not self._thread.is_alive():
                self._thread = None
                return
        raise RuntimeError('failed to gracefully stop current watcher; consider to terminate the process')

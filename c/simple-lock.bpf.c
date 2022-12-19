#include "vmlinux.h"

#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "simple-lock.h"

#ifdef to_usb_device
#undef to_usb_device
#endif /* to_usb_device */

#define to_usb_device(d) container_of(d, struct usb_device, dev)

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} ring_buffer SEC(".maps");

static inline void populate_common_fields(struct device_event *event) {
    event->pid = bpf_get_current_pid_tgid();
    event->ts = bpf_ktime_get_ns();
    bpf_get_current_comm(&(event->comm), sizeof(event->comm));
}

static inline void copy_string_with_limit(const char *source, char *destination,
                                          int limit) {
    int i = 0;
    for (; i < limit - 1 && source[i]; ++i) {
        destination[i] = source[i];
    }
    destination[i] = '\0';
}

SEC("kprobe/usb_probe_device")
int BPF_KPROBE(on_device_attached, struct device *device) {
    struct usb_device *usb = to_usb_device(device);
    char *serial = BPF_CORE_READ(usb, serial);
    struct device_event event = {
        .pid = 0,
        .ts = 0,
        .comm = {0},
        .type = 0,
        .device_id = {0},
    };

    populate_common_fields(&event);
    event.type = DEVICE_ATTACHED;
    copy_string_with_limit(serial, event.device_id, DEVICE_ID_LENGTH);

    bpf_ringbuf_submit(&event, 0);
    bpf_printk("Attached a USB device with ID '%s'\n", event.device_id);

    return 0;
}

SEC("kprobe/usb_unbind_device")
int BPF_KPROBE(on_device_detached, struct device *device) {
    struct usb_device *usb = to_usb_device(device);
    char *serial = BPF_CORE_READ(usb, serial);
    struct device_event event = {
        .pid = 0,
        .ts = 0,
        .comm = {0},
        .type = 0,
        .device_id = {0},
    };

    populate_common_fields(&event);
    event.type = DEVICE_DETACHED;
    copy_string_with_limit(serial, event.device_id, DEVICE_ID_LENGTH);

    bpf_ringbuf_submit(&event, 0);
    bpf_printk("Detached a USB device with ID '%s'", event.device_id);

    return 0;
}

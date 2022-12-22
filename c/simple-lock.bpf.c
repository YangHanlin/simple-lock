#include "tools/vmlinux/vmlinux.h"

#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "simple-lock.h"

#ifdef to_usb_device
#undef to_usb_device
#endif /* to_usb_device */

#define to_usb_device(d) container_of(d, struct usb_device, dev)

char LICENSE[] SEC("license") = "Dual MIT/GPL";

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} channel SEC(".maps");

static inline struct device_event *allocate_device_event() {
    struct device_event *event =
        bpf_ringbuf_reserve(&channel, sizeof(struct device_event), 0);

    if (event == NULL) {
        bpf_printk("Unable to allocate memory for device event\n");
        return NULL;
    }

    return event;
}

static inline void fill_device_event(struct device_event *event,
                                     enum device_event_type type,
                                     struct usb_device *device) {
    event->pid = bpf_get_current_pid_tgid();
    event->ts = bpf_ktime_get_ns();
    bpf_get_current_comm(&(event->comm), sizeof(event->comm));
    event->type = type;
    bpf_probe_read_kernel_str(&event->device_id, DEVICE_ID_LENGTH,
                              BPF_CORE_READ(device, serial));
}

static inline void submit_device_event(struct device_event *event) {
    bpf_ringbuf_submit(event, 0);

    int event_type = 0;
    bpf_probe_read(&event_type, sizeof(event_type), &event->type);
    char event_device_id[DEVICE_ID_LENGTH] = {0};
    bpf_probe_read_str(event_device_id, DEVICE_ID_LENGTH, event->device_id);

    bpf_printk("%s a USB device with ID '%s'\n",
               event_type == DEVICE_ATTACHED ? "Attached" : "Detached",
               event_device_id);
}

SEC("kprobe/usb_probe_device")
int BPF_KPROBE(on_device_attached, struct device *device) {
    struct device_event *event = allocate_device_event();
    if (event == NULL) {
        return 0;
    }
    fill_device_event(event, DEVICE_ATTACHED, to_usb_device(device));
    submit_device_event(event);
    return 0;
}

SEC("kprobe/usb_unbind_device")
int BPF_KPROBE(on_device_detached, struct device *device) {
    struct device_event *event = allocate_device_event();
    if (event == NULL) {
        return 0;
    }
    fill_device_event(event, DEVICE_DETACHED, to_usb_device(device));
    submit_device_event(event);
    return 0;
}

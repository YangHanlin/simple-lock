/**
 * Companion BPF program that detects additions and removals of USB devices.
 * For use with BCC only.
 */

#include <linux/sched.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/stddef.h>

/**
 * Overwrites the original container_of and to_usb_device to remove checks
 * that fails to pass the BPF verifier.
 */
#ifdef container_of
    #undef container_of
#endif /* container_of */

#define container_of(ptr, type, member) ({ \
	void *__mptr = (void *)(ptr); \
	((type *)(__mptr - offsetof(type, member))); })

#ifdef to_usb_device
    #undef to_usb_device
#endif /* to_usb_device */

#define	to_usb_device(d) container_of(d, struct usb_device, dev)

/**
 * Definitions used for BPF_PERF_OUTPUT
 */
#define DEVICE_ID_LENGTH 64

enum device_event_type {
    DEVICE_ATTACHED = 1,
    DEVICE_DETACHED,
};

struct device_event {
    u32 pid;
    u64 ts;
    char comm[TASK_COMM_LEN];

    enum device_event_type type;
    char device_id[DEVICE_ID_LENGTH];
};

BPF_PERF_OUTPUT(device_events);

/**
 * Helper functions
 */
static inline void populate_common_fields(struct device_event *event) {
    event->pid = bpf_get_current_pid_tgid();
    event->ts = bpf_ktime_get_ns();
    bpf_get_current_comm(&(event->comm), sizeof(event->comm));
}

static inline void copy_string_with_limit(const char *source, char *destination, int limit) {
    int i = 0;
    for (; i < limit - 1; ++i) {
        if (source[i] == '\0') {
            break;
        }
        destination[i] = source[i];
    }
    destination[i] = '\0';
}

/**
 * Hooks
 */
int kprobe__usb_probe_device(struct pt_regs *ctx, struct device *device) {
    struct usb_device *usb = to_usb_device(device);
    struct device_event event = {
        .pid = 0,
        .ts = 0,
        .comm = { 0 },
        .type = 0,
        .device_id = { 0 },
    };

    populate_common_fields(&event);
    event.type = DEVICE_ATTACHED;
    copy_string_with_limit((char *)(usb->serial), event.device_id, DEVICE_ID_LENGTH);

    device_events.perf_submit(ctx, &event, sizeof(event));
    bpf_trace_printk("Attached a USB device with ID '%s'\n", event.device_id);

    return 0;
}

int kprobe__usb_unbind_device(struct pt_regs *ctx, struct device *device) {
    struct usb_device *usb = to_usb_device(device);
    struct device_event event = {
        .pid = 0,
        .ts = 0,
        .comm = { 0 },
        .type = 0,
        .device_id = { 0 },
    };

    populate_common_fields(&event);
    event.type = DEVICE_DETACHED;
    copy_string_with_limit((char *)(usb->serial), event.device_id, DEVICE_ID_LENGTH);

    device_events.perf_submit(ctx, &event, sizeof(event));
    bpf_trace_printk("Detached a USB device with ID '%s'\n", event.device_id);

    return 0;
}

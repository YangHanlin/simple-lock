#ifndef __SIMPLE_LOCK_H
#define __SIMPLE_LOCK_H

#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 16
#endif

#define DEVICE_ID_LENGTH 64

enum device_event_type {
    DEVICE_ATTACHED = 1,
    DEVICE_DETACHED,
};

struct device_event {
    unsigned long pid;
    unsigned long long ts;
    char comm[TASK_COMM_LEN];

    enum device_event_type type;
    char device_id[DEVICE_ID_LENGTH];
};

#endif /* __SIMPLE_LOCK_H */

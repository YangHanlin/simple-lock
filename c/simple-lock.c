#include <argp.h>
#include <assert.h>
#include <cjson/cJSON.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "simple-lock.h"

#include "simple-lock.skel.h"

#define MODE_LENGTH 8

#define PATH_LENGTH 256

#define CONFIG_CONTENT_LENGTH 65536

const char *argp_program_version = "simple-lock 0.0";

const char *argp_program_bug_address = "<mattoncis@hotmail.com>";

const struct argp_option argp_options[] = {
    {
        .name = "MODE",
        .flags = OPTION_DOC | OPTION_NO_USAGE,
        .doc = "Run mode of this application; must be `setup` or `monitor`",
    },
    {
        .name = "debug",
        .key = 'd',
        .flags = OPTION_ARG_OPTIONAL,
        .doc = "Whether to enable debug logging",
    },
    {0},
};

static error_t argp_parse_hook(int key, char *argument,
                               struct argp_state *state);

static const struct argp argp = {
    .parser = argp_parse_hook,
    .options = argp_options,
    .args_doc = "MODE",
    .doc = "Documentation WIP",
};

static struct config {
    bool debug;
    char mode[MODE_LENGTH];
    char device_id[DEVICE_ID_LENGTH];
} config = {false, {0}, {0}};

static volatile bool running = false;

static int libbpf_print_hook(enum libbpf_print_level level, const char *format,
                             va_list arguments);

static void handle_signal(int signal);

static int handle_setup_event(void *context, void *data, size_t data_size);

static int handle_monitor_event(void *context, void *data, size_t data_size);

static int load_config();

static int save_config();

int main(int argc, char **argv) {
    int error_code = 0;
    struct simple_lock_bpf *skeleton = NULL;
    struct ring_buffer *channel = NULL;

    error_code = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if (error_code) {
        goto cleanup;
    }

    error_code = load_config();
    if (error_code) {
        goto cleanup;
    }

    if (strcmp(config.mode, "monitor") == 0 && strlen(config.device_id) == 0) {
        fprintf(
            stderr,
            "This application has not been set up; please run `sudo %s setup` "
            "to set up\n",
            argv[0]);
        error_code = 1;
        goto cleanup;
    }

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
    libbpf_set_print(libbpf_print_hook);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    skeleton = simple_lock_bpf__open();
    if (!skeleton) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        error_code = 1;
        goto cleanup;
    }

    error_code = simple_lock_bpf__load(skeleton);
    if (error_code) {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        goto cleanup;
    }

    error_code = simple_lock_bpf__attach(skeleton);
    if (error_code) {
        fprintf(stderr, "Failed to attach BPF programs\n");
        goto cleanup;
    }

    if (strcmp(config.mode, "setup") == 0) {
        channel = ring_buffer__new(bpf_map__fd(skeleton->maps.channel),
                                   handle_setup_event, NULL, NULL);
    } else if (strcmp(config.mode, "monitor") == 0) {
        channel = ring_buffer__new(bpf_map__fd(skeleton->maps.channel),
                                   handle_monitor_event, NULL, NULL);
    } else {
        assert(false);
    }
    if (!channel) {
        error_code = 1;
        fprintf(stderr, "Failed to create channel\n");
    }

    running = true;
    fprintf(stderr,
            "Started; please plug/unplug the USB device or press Ctrl + C to "
            "exit\n");
    while (running) {
        error_code = ring_buffer__poll(channel, 100);
        if (error_code == -EINTR) {
            error_code = 0;
            goto cleanup;
        } else if (error_code < 0) {
            fprintf(stderr, "Error polling channel: %d\n", error_code);
            goto cleanup;
        }
    }

cleanup:
    if (channel != NULL) {
        ring_buffer__free(channel);
    }
    if (skeleton != NULL) {
        simple_lock_bpf__destroy(skeleton);
    }

    return error_code < 0 ? -error_code : error_code;
}

static error_t argp_parse_hook(int key, char *argument,
                               struct argp_state *state) {
    switch (key) {
        case 'd':
            config.debug = true;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num > 1) {
                fprintf(stderr, "Too many arguments\n");
                argp_usage(state);
            } else if (strcmp(argument, "setup") != 0 &&
                       strcmp(argument, "monitor") != 0) {
                fprintf(stderr, "Invalid mode: %s\n", argument);
                argp_usage(state);
            } else {
                strcpy(config.mode, argument);
            }
            break;
        case ARGP_KEY_END:
            if (state->arg_num < 1) {
                fprintf(stderr, "Too few arguments\n");
                argp_usage(state);
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static int libbpf_print_hook(enum libbpf_print_level level, const char *format,
                             va_list arguments) {
    if (!config.debug) {
        return 0;
    }
    return vfprintf(stderr, format, arguments);
}

static void handle_signal(int signal) {
    fprintf(stderr, "Exiting\n");
    running = false;
}

static int handle_setup_event(void *context, void *data, size_t data_size) {
    struct device_event *event = data;

    if (config.debug) {
        fprintf(stderr,
                "Got event: { .pid = %ld, .ts = %lld, .comm = %s, .type = %d, "
                ".device_id = %s }\n",
                event->pid, event->ts, event->comm, event->type,
                event->device_id);
    }

    strcpy(config.device_id, event->device_id);
    int error_code = save_config();
    if (error_code) {
        fprintf(stderr, "Failed to save configuration; please retry\n");
        return error_code;
    }

    fprintf(stderr, "Successfully updated configured device ID to %s",
            config.device_id);
    running = false;

    return 0;
}

static int handle_monitor_event(void *context, void *data, size_t data_size) {
    struct device_event *event = data;

    if (config.debug) {
        fprintf(stderr,
                "Got event: { .pid = %ld, .ts = %lld, .comm = %s, .type = %d, "
                ".device_id = %s }\n",
                event->pid, event->ts, event->comm, event->type,
                event->device_id);
    }

    if (strcmp(config.device_id, event->device_id) == 0) {
        if (event->type = DEVICE_ATTACHED) {
            system("loginctl lock-session");
        } else if (event->type = DEVICE_DETACHED) {
            system("loginctl unlock-session");
        } else {
            assert(false);
        }
    }

    return 0;
}

static int load_config() {
    char config_path[PATH_LENGTH] = {0};
    strcat(config_path, getenv("HOME"));
    strcat(config_path, "/.simple-lock-config.json");

    if (access(config_path, F_OK) != 0) {
        FILE *config_file = fopen(config_path, "w");
        fprintf(config_file, "{}");
        fclose(config_file);
    }

    FILE *config_file = fopen(config_path, "r");
    if (config_file == NULL) {
        fprintf(stderr, "Cannot open config file %s to read\n", config_path);
        return 1;
    }

    char config_content[CONFIG_CONTENT_LENGTH] = {0};

    int c;
    for (int i = 0;
         i < CONFIG_CONTENT_LENGTH - 1 && (c = getc(config_file)) != EOF; ++i) {
        config_content[i] = c;
    }

    cJSON *config_json = cJSON_Parse(config_content);

    cJSON *device_id_json =
        cJSON_GetObjectItemCaseSensitive(config_json, "device_id");
    if (device_id_json != NULL) {
        strcpy(config.device_id, device_id_json->valuestring);
    }

    cJSON_Delete(config_json);

    fclose(config_file);

    return 0;
}

static int save_config() {
    char config_path[PATH_LENGTH] = {0};
    strcat(config_path, getenv("HOME"));
    strcat(config_path, "/.simple-lock-config.json");

    FILE *config_file = fopen(config_path, "w");
    if (config_file == NULL) {
        fprintf(stderr, "Cannot open config file %s to write\n", config_path);
        return 1;
    }

    cJSON *config_json = cJSON_CreateObject();

    cJSON_AddStringToObject(config_json, "device_id", config.device_id);

    char *config_content = cJSON_Print(config_json);
    fprintf(config_file, "%s", config_content);
    free(config_content);

    cJSON_Delete(config_json);

    fclose(config_file);

    return 0;
}

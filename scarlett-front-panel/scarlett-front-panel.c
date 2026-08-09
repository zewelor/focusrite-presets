// SPDX-License-Identifier: MIT
// One-shot front-panel brightness helper for Scarlett 4i4 4th Gen firmware 2417+.
//
// This reproduces the narrow configuration transactions used by the upstream
// scarlett2 kernel driver. It deliberately supports only reading and changing
// front-panel brightness and the idle sleep timeout; it does not implement a
// generic USB console.

#include <endian.h>
#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOCUSRITE_VID 0x1235
#define SCARLETT_4I4_GEN4_PID 0x821a
#define USB_REQUEST_OUT 0x21 /* class | interface | host-to-device */
#define USB_REQUEST_IN  0xa1 /* class | interface | device-to-host */
#define USB_CMD_INIT 0
#define USB_CMD_REQUEST 2
#define USB_CMD_RESPONSE 3

#define SCARLETT_INIT_1 0x00000000
#define SCARLETT_INIT_2 0x00000002
#define SCARLETT_GET_DATA 0x00800000
#define SCARLETT_SET_DATA 0x00800001
#define SCARLETT_DATA_CMD 0x00800002

#define FP_BRIGHTNESS_OFFSET 0x3a9
#define FP_SLEEP_TIME_OFFSET 0x3ac
#define PARAMETER_BUFFER 0x130
#define FP_BRIGHTNESS_ACTIVATE 36
#define FP_SLEEP_TIME_ACTIVATE 38
#define MAX_FP_SLEEP_TIME 86400

enum brightness {
    HIGH = 0,
    MEDIUM = 1,
    LOW = 2,
};

struct __attribute__((packed)) packet_header {
    uint32_t command;
    uint16_t size;
    uint16_t sequence;
    uint32_t error;
    uint32_t padding;
};

static uint16_t sequence = 1;
static uint16_t focusrite_control_interface;
static uint8_t focusrite_notification_endpoint;

static void die_libusb(const char *operation, int error) {
    fprintf(stderr, "%s: %s\n", operation, libusb_error_name(error));
    exit(EXIT_FAILURE);
}

static void find_focusrite_control_interface(libusb_device_handle *device) {
    libusb_device *usb_device = libusb_get_device(device);
    struct libusb_config_descriptor *configuration = NULL;
    int result = libusb_get_active_config_descriptor(usb_device, &configuration);
    if (result < 0)
        die_libusb("reading active USB configuration", result);

    for (uint8_t i = 0; i < configuration->bNumInterfaces; i++) {
        const struct libusb_interface *interface = &configuration->interface[i];
        if (interface->num_altsetting == 0)
            continue;

        const struct libusb_interface_descriptor *descriptor =
            &interface->altsetting[0];
        if (descriptor->bInterfaceClass == LIBUSB_CLASS_VENDOR_SPEC &&
            descriptor->bInterfaceSubClass == 1) {
            for (uint8_t endpoint_index = 0;
                 endpoint_index < descriptor->bNumEndpoints;
                 endpoint_index++) {
                const struct libusb_endpoint_descriptor *endpoint =
                    &descriptor->endpoint[endpoint_index];
                if ((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                        LIBUSB_ENDPOINT_IN &&
                    (endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) ==
                        LIBUSB_TRANSFER_TYPE_INTERRUPT) {
                    focusrite_control_interface = descriptor->bInterfaceNumber;
                    focusrite_notification_endpoint =
                        endpoint->bEndpointAddress;
                    libusb_free_config_descriptor(configuration);
                    return;
                }
            }
        }
    }

    libusb_free_config_descriptor(configuration);
    fprintf(stderr, "Focusrite Control USB interface not found\n");
    exit(EXIT_FAILURE);
}

static void refuse_active_kernel_driver(libusb_device_handle *device) {
    libusb_device *usb_device = libusb_get_device(device);
    struct libusb_config_descriptor *configuration = NULL;
    int result = libusb_get_active_config_descriptor(usb_device, &configuration);
    if (result < 0)
        die_libusb("reading active USB configuration", result);

    for (uint8_t i = 0; i < configuration->bNumInterfaces; i++) {
        const struct libusb_interface *interface = &configuration->interface[i];
        if (interface->num_altsetting == 0)
            continue;

        const uint8_t interface_number =
            interface->altsetting[0].bInterfaceNumber;
        result = libusb_kernel_driver_active(device, interface_number);
        if (result == 1) {
            libusb_free_config_descriptor(configuration);
            fprintf(stderr,
                    "kernel USB driver is still active on interface %u; "
                    "run this helper through ./run-exclusive\n",
                    interface_number);
            exit(EXIT_FAILURE);
        }
        if (result < 0 && result != LIBUSB_ERROR_NOT_SUPPORTED) {
            libusb_free_config_descriptor(configuration);
            die_libusb("checking for an active kernel USB driver", result);
        }
    }

    libusb_free_config_descriptor(configuration);
}

static void validate_response(const uint8_t *response, size_t response_size,
                              uint32_t command, uint16_t request_sequence,
                              size_t data_size) {
    if (response_size < sizeof(struct packet_header)) {
        fprintf(stderr, "truncated response: got %zu bytes, need at least %zu\n",
                response_size, sizeof(struct packet_header));
        exit(EXIT_FAILURE);
    }

    const struct packet_header *header = (const struct packet_header *)response;
    if (response_size != sizeof(struct packet_header) + data_size) {
        fprintf(stderr,
                "unexpected response size: got %zu, expected %zu "
                "(cmd=%#x seq=%u declared_size=%u error=%u padding=%u)\n",
                response_size, sizeof(struct packet_header) + data_size,
                le32toh(header->command), le16toh(header->sequence),
                le16toh(header->size), le32toh(header->error),
                le32toh(header->padding));
        exit(EXIT_FAILURE);
    }

    const uint16_t response_sequence = le16toh(header->sequence);
    const int sequence_matches =
        response_sequence == request_sequence ||
        (request_sequence == 1 && response_sequence == 0);

    if (le32toh(header->command) != command || !sequence_matches ||
        le16toh(header->size) != data_size ||
        le32toh(header->error) != 0 || le32toh(header->padding) != 0) {
        fprintf(stderr,
                "device rejected or mismatched response "
                "(cmd=%#x seq=%u size=%u error=%u padding=%u)\n",
                le32toh(header->command), le16toh(header->sequence),
                le16toh(header->size), le32toh(header->error),
                le32toh(header->padding));
        exit(EXIT_FAILURE);
    }
}

static void drain_notifications(libusb_device_handle *device) {
    uint8_t notification[64];

    for (int attempt = 0; attempt < 32; attempt++) {
        int transferred = 0;
        int result = libusb_interrupt_transfer(
            device, focusrite_notification_endpoint, notification,
            sizeof(notification), &transferred, 20);
        if (result == LIBUSB_ERROR_TIMEOUT)
            return;
        if (result < 0)
            die_libusb("draining old notifications", result);
    }

    fprintf(stderr, "notification endpoint did not become idle\n");
    exit(EXIT_FAILURE);
}

static void wait_for_acknowledgement(libusb_device_handle *device) {
    uint8_t notification[64];

    for (int attempt = 0; attempt < 32; attempt++) {
        int notification_size = 0;
        int result = libusb_interrupt_transfer(
            device, focusrite_notification_endpoint, notification,
            sizeof(notification), &notification_size, 1000);
        if (result < 0)
            die_libusb("waiting for command acknowledgement", result);
        if (notification_size < (int)sizeof(uint32_t)) {
            fprintf(stderr, "short notification: %d bytes\n",
                    notification_size);
            exit(EXIT_FAILURE);
        }

        uint32_t notification_flags;
        memcpy(&notification_flags, notification, sizeof(notification_flags));
        if (le32toh(notification_flags) & 1)
            return;
    }

    fprintf(stderr, "command acknowledgement was not received\n");
    exit(EXIT_FAILURE);
}

static void command(libusb_device_handle *device, uint32_t command_id,
                    const void *request_data, size_t request_size,
                    void *response_data, size_t response_size) {
    uint8_t request[sizeof(struct packet_header) + 16];
    uint8_t response[sizeof(struct packet_header) + 128];

    if (request_size > sizeof(request) - sizeof(struct packet_header) ||
        response_size > sizeof(response) - sizeof(struct packet_header)) {
        fprintf(stderr, "internal request or response buffer too small\n");
        exit(EXIT_FAILURE);
    }

    const uint16_t request_sequence = sequence++;
    memset(request, 0, sizeof(request));
    memset(response, 0, sizeof(response));
    struct packet_header *request_header = (struct packet_header *)request;

    request_header->command = htole32(command_id);
    request_header->size = htole16(request_size);
    request_header->sequence = htole16(request_sequence);
    if (request_size)
        memcpy(request + sizeof(*request_header), request_data, request_size);

    int result = libusb_control_transfer(
        device, USB_REQUEST_OUT, USB_CMD_REQUEST, 0,
        focusrite_control_interface, request,
        sizeof(*request_header) + request_size, 1000);
    if (result < 0)
        die_libusb("sending command", result);
    if ((size_t)result != sizeof(*request_header) + request_size) {
        fprintf(stderr, "short command write: %d bytes\n", result);
        exit(EXIT_FAILURE);
    }

    wait_for_acknowledgement(device);

    result = libusb_control_transfer(
        device, USB_REQUEST_IN, USB_CMD_RESPONSE, 0,
        focusrite_control_interface, response,
        sizeof(struct packet_header) + response_size, 1000);
    if (result < 0)
        die_libusb("receiving command response", result);

    validate_response(response, result, command_id, request_sequence,
                      response_size);
    if (response_size)
        memcpy(response_data, response + sizeof(struct packet_header),
               response_size);
}

static uint32_t initialise_protocol(libusb_device_handle *device) {
    uint8_t step0[24];
    int result = libusb_control_transfer(
        device, USB_REQUEST_IN, USB_CMD_INIT, 0,
        focusrite_control_interface, step0, sizeof(step0), 1000);
    if (result < 0)
        die_libusb("initialising Focusrite protocol (step 0)", result);
    if ((size_t)result != sizeof(step0)) {
        fprintf(stderr,
                "short Focusrite protocol step-0 response: got %d, "
                "expected %zu bytes\n",
                result, sizeof(step0));
        exit(EXIT_FAILURE);
    }

    drain_notifications(device);

    sequence = 1;
    command(device, SCARLETT_INIT_1, NULL, 0, NULL, 0);

    uint8_t step2[84];
    sequence = 1;
    command(device, SCARLETT_INIT_2, NULL, 0, step2, sizeof(step2));

    uint32_t firmware_version;
    memcpy(&firmware_version, step2 + 8, sizeof(firmware_version));
    firmware_version = le32toh(firmware_version);
    printf("Focusrite firmware: %u\n", firmware_version);
    if (firmware_version < 2417) {
        fprintf(stderr,
                "firmware %u does not expose the front-panel brightness "
                "setting (2417 or later required)\n",
                firmware_version);
        exit(EXIT_FAILURE);
    }

    return firmware_version;
}

static void get_data(libusb_device_handle *device, uint32_t offset,
                     void *data, uint32_t size) {
    struct __attribute__((packed)) {
        uint32_t offset;
        uint32_t size;
    } request = {
        .offset = htole32(offset),
        .size = htole32(size),
    };

    command(device, SCARLETT_GET_DATA, &request, sizeof(request), data, size);
}

static uint8_t get_brightness(libusb_device_handle *device) {
    uint8_t brightness;
    get_data(device, FP_BRIGHTNESS_OFFSET, &brightness, sizeof(brightness));
    return brightness;
}

static uint32_t get_sleep_time(libusb_device_handle *device) {
    uint32_t sleep_time;
    get_data(device, FP_SLEEP_TIME_OFFSET, &sleep_time, sizeof(sleep_time));
    return le32toh(sleep_time);
}

static void set_data_byte(libusb_device_handle *device, uint32_t offset,
                          uint8_t value) {
    uint8_t request[9];
    const uint32_t little_offset = htole32(offset);
    const uint32_t little_size = htole32(1);

    memcpy(request, &little_offset, sizeof(little_offset));
    memcpy(request + 4, &little_size, sizeof(little_size));
    request[8] = value;
    command(device, SCARLETT_SET_DATA, request, sizeof(request), NULL, 0);
}

static void set_data_u32(libusb_device_handle *device, uint32_t offset,
                         uint32_t value) {
    uint8_t request[12];
    const uint32_t little_offset = htole32(offset);
    const uint32_t little_size = htole32(sizeof(uint32_t));
    const uint32_t little_value = htole32(value);

    memcpy(request, &little_offset, sizeof(little_offset));
    memcpy(request + 4, &little_size, sizeof(little_size));
    memcpy(request + 8, &little_value, sizeof(little_value));
    command(device, SCARLETT_SET_DATA, request, sizeof(request), NULL, 0);
}

static void set_brightness(libusb_device_handle *device, uint8_t brightness) {
    const uint32_t activate = htole32(FP_BRIGHTNESS_ACTIVATE);

    set_data_byte(device, PARAMETER_BUFFER + 1, 0); // configuration index
    set_data_byte(device, PARAMETER_BUFFER, brightness);
    command(device, SCARLETT_DATA_CMD, &activate, sizeof(activate), NULL, 0);
}

static void set_sleep_time(libusb_device_handle *device, uint32_t seconds) {
    const uint32_t activate = htole32(FP_SLEEP_TIME_ACTIVATE);

    set_data_u32(device, FP_SLEEP_TIME_OFFSET, seconds);
    command(device, SCARLETT_DATA_CMD, &activate, sizeof(activate), NULL, 0);
}

static const char *brightness_name(uint8_t value) {
    switch (value) {
    case HIGH: return "High";
    case MEDIUM: return "Medium";
    case LOW: return "Low";
    default: return "unknown";
    }
}

static int parse_brightness(const char *text, uint8_t *value) {
    if (strcmp(text, "high") == 0)
        *value = HIGH;
    else if (strcmp(text, "medium") == 0)
        *value = MEDIUM;
    else if (strcmp(text, "low") == 0)
        *value = LOW;
    else
        return -1;

    return 0;
}

static void print_usage(FILE *stream, const char *program) {
    fprintf(stream,
            "usage: %s [--brightness high|medium|low] "
            "[--sleep SECONDS]\n",
            program);
}

int main(int argc, char **argv) {
    int change_brightness = 0;
    int change_sleep_time = 0;
    uint8_t requested_brightness = HIGH;
    uint32_t requested_sleep_time = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(stdout, argv[0]);
            return EXIT_SUCCESS;
        }

        if (strcmp(argv[i], "--brightness") == 0) {
            if (change_brightness || ++i >= argc ||
                parse_brightness(argv[i], &requested_brightness) < 0) {
                fprintf(stderr,
                        "brightness must be one of: high, medium, low\n");
                print_usage(stderr, argv[0]);
                return EXIT_FAILURE;
            }
            change_brightness = 1;
            continue;
        }

        if (strcmp(argv[i], "--sleep") == 0) {
            if (change_sleep_time || ++i >= argc) {
                print_usage(stderr, argv[0]);
                return EXIT_FAILURE;
            }

            char *end = NULL;
            unsigned long value = strtoul(argv[i], &end, 10);
            if (!argv[i][0] || *end || value > MAX_FP_SLEEP_TIME) {
                fprintf(stderr,
                        "sleep time must be an integer from 0 to %u seconds\n",
                        MAX_FP_SLEEP_TIME);
                return EXIT_FAILURE;
            }
            requested_sleep_time = value;
            change_sleep_time = 1;
            continue;
        }

        fprintf(stderr, "unknown argument: %s\n", argv[i]);
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    libusb_context *context = NULL;
    int result = libusb_init(&context);
    if (result < 0)
        die_libusb("initialising libusb", result);

    libusb_device_handle *device = libusb_open_device_with_vid_pid(
        context, FOCUSRITE_VID, SCARLETT_4I4_GEN4_PID);
    if (!device) {
        fprintf(stderr, "Scarlett 4i4 4th Gen (1235:821a) not found or inaccessible\n");
        libusb_exit(context);
        return EXIT_FAILURE;
    }

    refuse_active_kernel_driver(device);
    find_focusrite_control_interface(device);
    result = libusb_claim_interface(device, focusrite_control_interface);
    if (result < 0)
        die_libusb("claiming Focusrite Control interface", result);

    initialise_protocol(device);

    if (!change_brightness && !change_sleep_time) {
        uint16_t master_volume;
        get_data(device, 0x32, &master_volume, sizeof(master_volume));
        printf("Protocol probe (master volume at 0x32): %#x\n",
               le16toh(master_volume));
    }

    uint8_t before = get_brightness(device);
    uint32_t sleep_before = get_sleep_time(device);
    printf("Front-panel brightness: %u (%s)\n", before, brightness_name(before));
    printf("Front-panel sleep time: %u seconds\n", sleep_before);
    if (before > LOW) {
        fprintf(stderr, "unexpected brightness value; refusing to write\n");
        libusb_release_interface(device, focusrite_control_interface);
        libusb_close(device);
        libusb_exit(context);
        return EXIT_FAILURE;
    }

    if (change_brightness) {
        if (before != requested_brightness)
            set_brightness(device, requested_brightness);

        uint8_t after = get_brightness(device);
        printf("Front-panel brightness verified: %u (%s)\n", after,
               brightness_name(after));
        if (after != requested_brightness) {
            fprintf(stderr, "brightness write did not take effect\n");
            libusb_release_interface(device, focusrite_control_interface);
            libusb_close(device);
            libusb_exit(context);
            return EXIT_FAILURE;
        }
    }

    if (change_sleep_time) {
        if (sleep_before != requested_sleep_time)
            set_sleep_time(device, requested_sleep_time);

        uint32_t sleep_after = get_sleep_time(device);
        printf("Front-panel sleep time verified: %u seconds\n",
               sleep_after);
        if (sleep_after != requested_sleep_time) {
            fprintf(stderr, "sleep-time write did not take effect\n");
            libusb_release_interface(device, focusrite_control_interface);
            libusb_close(device);
            libusb_exit(context);
            return EXIT_FAILURE;
        }
    }

    result = libusb_release_interface(device, focusrite_control_interface);
    if (result < 0)
        die_libusb("releasing Focusrite Control interface", result);
    libusb_close(device);
    libusb_exit(context);
    return EXIT_SUCCESS;
}

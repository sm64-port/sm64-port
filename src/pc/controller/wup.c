/*
 *
 clang-format -style=gnu -dump-config > .clang-format
 clang-format -style=gnu ~/code/sm64-port/src/pc/controller/wup.c
 clang-format -style=mozilla ~/code/sm64-port/src/pc/controller/wup.c
 clang-format -style=chromium ~/code/sm64-port/src/pc/controller/wup.c

 clang-format -style=microsoft ~/code/sm64-port/src/pc/controller/wup.c -i
 clang-format -style=file:$HOME/code/sm64-port/.clang-format ~/code/sm64-port/src/pc/controller/wup.c -i

 */

#if !defined(__MINGW32__) && !defined(__BSD__) && !defined(TARGET_WEB)
// See LICENSE for license

#define _XOPEN_SOURCE 600

#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

//#include <libudev.h>
#include <libusb.h>
#include <pthread.h>

#include "macros.h"
#include "../configfile.h"

#if (!defined(LIBUSBX_API_VERSION) || LIBUSBX_API_VERSION < 0x01000102)                                \
    && (!defined(LIBUSB_API_VERSION) || LIBUSB_API_VERSION < 0x01000102)
#error libusb(x) 1.0.16 or higher is required
#endif

#define EP_IN 0x81
#define EP_OUT 0x02

#define STATE_NORMAL 0x10
#define STATE_WAVEBIRD 0x20

const int BUTTON_OFFSET_VALUES[16] = {
    BTN_START,
    BTN_TR2,
    BTN_TR,
    BTN_TL,
    -1,
    -1,
    -1,
    -1,
    BTN_SOUTH,
    BTN_WEST,
    BTN_EAST,
    BTN_NORTH,
    BTN_DPAD_LEFT,
    BTN_DPAD_RIGHT,
    BTN_DPAD_DOWN,
    BTN_DPAD_UP,
};

const int AXIS_OFFSET_VALUES[6] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ };

struct ports {
    bool connected;
    bool extra_power;
    unsigned char type;
    uint16_t buttons;
    uint8_t axis[6];
};

struct adapter {
    volatile bool quitting;
    struct libusb_device *device;
    struct libusb_device_handle *handle;
    pthread_t thread;
    unsigned char rumble[5];
    struct ports controllers[4];
    struct adapter *next;
};

static bool raw_mode;

static volatile int quitting;

static struct adapter adapters;

static const char *uinput_path;

bool wants_saturate = true; // hack for difference in need for saturation

bool wup_get_controller_input(uint16_t *buttons, uint8_t axis[6], bool *do_saturate) {

    // Assuming only 1 controller attached, should be a better way to control this
    *do_saturate = wants_saturate;

    struct adapter *adapter = adapters.next;
    if (adapter != NULL) {
        *buttons = adapter->controllers[0].buttons;
        memcpy(axis, adapter->controllers[0].axis, 6);
        return true;
    } else {
        return false;
    }
}

static unsigned char connected_type(unsigned char status) {
    unsigned char type = status & (STATE_NORMAL | STATE_WAVEBIRD);
    switch (type) {
        case STATE_NORMAL:
        case STATE_WAVEBIRD:
            return type;
        default:
            return 0;
    }
}

static void handle_payload(int i, struct ports *port, unsigned char *payload) {
    unsigned char status = payload[0];
    unsigned char type = connected_type(status);

    wants_saturate = true;

    if (type != 0 && !port->connected) {
        // uinput_create(i, port, type);
        port->type = type;
        port->connected = true;
    } else if (type == 0 && port->connected) {
        // uinput_destroy(i, port);
        port->connected = false;
    }

    if (!port->connected)
        return;

    port->extra_power = ((status & 0x04) != 0);

    if (type != port->type) {
        fprintf(stderr, "controller on port %d changed controller type???", i + 1);
        port->type = type;
    }

    uint16_t btns = (uint16_t) payload[1] << 8 | (uint16_t) payload[2];
    port->buttons = btns;
    // printf("Btns: %04x\n", btns);

    // printf("Axis:");
    for (int j = 0; j < 6; j++) {
        unsigned char value = payload[j + 3];
        port->axis[j] = value;
        // printf(" %02x", value);
    }
    // puts("");
}

static void handle_hyperkin_payload(int i, struct ports *port, unsigned char *payload) {
    /*"""
    References:
    https://digitalcommons.calpoly.edu/cgi/viewcontent.cgi?article=1185&context=eesp
    ipfs.io/ipfs/bafybeighb4imouukdmn4w77i5j72oovprbvkgstujlerlxiryf44zb3iny

    http://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro26/26-02.html#04.1
         //
         //http://n64devkit.square7.ch/pro-man/pro26/26-02.htm

    The Control Stick's coordinate positions stick_x and stick_y are signed
    characters with the range of -128 ~ 127. However, for the actual program we
    recommend using values within the ranges shown below:

    Left/right X axis   +/- 61
    Up/down Y axis      +/- 63
    X axis incline      +/- 45
    Y axis incline      +/- 47

    +--------------------+-------------------+-------------+------------+
    | Payload (unsigned) |  Payload (signed) | OSContValue |  Note      |
    +--------------------+-------------------+-------------+------------+
    |           127      |             0     |      0      |  Deadzone  |
    +--------------------+-------------------+-------------+------------+
    |           100      |           -27     |      0      |  Deadzone  |
    +--------------------+-------------------+-------------+------------+
    |            99      |           -28     |      1      |  Minimum   |
    +--------------------+-------------------+-------------+------------+
    |            80      |           -47     |     40      |  Incline   |
    +--------------------+-------------------+-------------+------------+
    |            64      |           -63     |     80      |  Maximum   |
    +--------------------+-------------------+-------------+------------+
    |             0      |          -127     |     80      |  Saturated |
    +--------------------+-------------------+-------------+------------+


    Stick Value Ranges for Differenct Contrllers

        On an OEM N64 controller, the following ranges were measured:
            left, neutral, right = 13, 128, 243
            top, neutral, bottom = 12, 127, 254

        On an Hori MiniPad, the following ranges were measured:
            left, neutral, right =  2, 102, 245
            top, neutral, bottom =  2, 127, 254


    +-------------------+----------------+----------------+----------------+
    |     Direction     | Payload (Gray) | Payload (Blue) | Payload (Pink) |
    +-------------------+----------------+----------------+----------------+
    |       Y-Up-Max    |            24  |            12  |              0 |
    +-------------------+----------------+----------------+----------------+
    |       Y-Down-Max  |            240 |            252 |            250 |
    +-------------------+----------------+----------------+----------------+
    |       X-Left-Max  |             18 |             11 |              0 |
    +-------------------+----------------+----------------+----------------+
    |       X-Right-Max |            234 |            246 |            255 |
    +-------------------+----------------+----------------+----------------+
    |       X-Neutral   |       ~102-128 |       ~102-154 |       ~102-128 |
    +-------------------+----------------+----------------+----------------+
    |       y-Neutral   |       ~102-153 |       ~102-153 |       ~102-127 |
    +-------------------+----------------+----------------+----------------+

    Gray and Blue are OEM controllers.  Pink is the HoriMiniPad.


    This function is written to support the original N64 controller plugged
    in using a hyperkin adapter. We do this by simply coercing the payload
    into an adapter buttons and axis that controller_wup.c will expect.

    https://www.hyperkin.com/controller-adapter-for-n64r-controller-compatible-with-nintendo-switchr-pc-hyperkin.html

    Reminder, you will need custom udev rules to make this work:
    in /etc/udev/rules.d/80-wup028.rules

        SUBSYSTEM=="usb", ATTRS{idVendor}=="20d6", ATTRS{idProduct}=="a710", MODE="0666"
        SUBSYSTEM=="usb_device", ATTRS{idVendor}=="20d6", ATTRS{idProduct}=="a710", MODE="0666"

    Dont forget to `sudo udevadm control --reload-rules` and unplug/replug the
    controller in after!

    Technical Details:
        An example payload from libusb with no (i.e. neutral) inputs is as follows:

        payload[0] = 0,  # main buttons
        payload[1] = 0,  # start button
        payload[2] = 8,  # d-pad
        payload[3] = 128,  # stick-lr
        payload[4] = 127,  # stick-ud
        payload[5] = 128,  # c-buttons
        payload[6] = 127,  # c-buttons
        payload[7] = 0,    # unused

        When a button on the N64 controller is pressed the following changes occur:

        Button  =    data    ==  neutral -> pressed
        Z       = payload[0] ==   0  ->  64
        R       = payload[0] ==   0  ->  32
        L       = payload[0] ==   0  ->  16
        A       = payload[0] ==   0  ->   4
        B       = payload[0] ==   0  ->   2
        start   = payload[1] ==   0  ->   2
        c-right = payload[5] == 128  -> 255
        c-left  = payload[5] == 128  ->   0
        c-up    = payload[6] == 127  ->   0
        c-down  = payload[6] == 127  -> 255
        d-up    = payload[2] ==   8  ->   0
        d-down  = payload[2] ==   8  ->   4
        d-left  = payload[2] ==   8  ->   6
        d-right = payload[2] ==   8  ->   2

        When the stick on the N64 controller is tilted the following changes occur

        Stick Tilt Left  = payload[3] = ~128 -> ~0
        Stick Tilt Right = payload[3] = ~128 -> ~255
        Stick Tilt Up    = payload[4] = ~127 -> ~0
        Stick Tilt Down  = payload[4] = ~127 -> ~255

        The neutral position and ranges of the stick seems to depend on the controller itself.

        On an OEM N64 controller, the following ranges were measured:
            left, neutral, right = 13, 128, 243
            top, neutral, bottom = 12, 127, 254

        On an Hori MiniPad, the following ranges were measured:
            left, neutral, right =  2, 102, 245
            top, neutral, bottom =  2, 127, 254

        To integrate this logic, we seek to make the output of this function behave
        as expected by the controller_wup.c file.  This requires us to map the
        buttons and axis into port->buttons and port->axis in a way that upstream
        logic expects. The following Python program captures the expected bits
        associated with each button in the desired adapter output, and what is
        given to us in our payload. This Python logic is used to generate the C
        code that ultimately performs this task.

        The python is moved to update_wup_logic.py
      """*/

    wants_saturate = false;

    unsigned char type = STATE_NORMAL; // hard coded, unsure if there is a better way to set

    if (type != 0 && !port->connected) {
        port->type = type;
        port->connected = true;
    } else if (type == 0 && port->connected) {
        port->connected = false;
    }

    if (!port->connected)
        return;

    port->extra_power = 0; // hard coded, unsure if there is a better way to set

    if (type != port->type) {
        fprintf(stderr, "controller on port %d changed controller type???", i + 1);
        port->type = type;
    }

    // Generated code to map the payload to port->buttons
    uint16_t btns = ((uint16_t) (payload[0] & 0x40) >> 3 | // map Z to 0x08
                     (uint16_t) (payload[0] & 0x20) >> 3 | // map R to 0x04
                     (uint16_t) (payload[0] & 0x10) << 8 | // map L to 0x1000
                     (uint16_t) (payload[0] & 0x04) << 6 | // map A to 0x100
                     (uint16_t) (payload[0] & 0x02) << 8 | // map B to 0x200
                     (uint16_t) (payload[1] & 0x02) >> 1   // map START to 0x01
    );
    port->buttons = btns;

    /*float deadZone256 = 256 * (((float) configDeadZone) / 32768);*/
    float deadZone256 = 31;

    // Source payload magnitudes to be mapped
    float LR_src1 = 128 - 102;
    float LR_src2 = 128 -  57;
    float LR_src3 = 128 - deadZone256;
    float LR_src4 = 128 + deadZone256;
    /*float LR_src3 = 128 - 31;*/
    /*float LR_src4 = 128 + 31;*/
    float LR_src5 = 128 +  57;
    float LR_src6 = 128 + 103;

    float UD_src1 = 128 - 101;
    float UD_src2 = 128 -  57;
    float UD_src3 = 128 - deadZone256;
    float UD_src4 = 128 + deadZone256;
    /*float UD_src3 = 128 - 31;*/
    /*float UD_src4 = 128 + 31;*/
    float UD_src5 = 128 +  57;
    float UD_src6 = 128 + 101;

    printf("configDeadZone = %d\n", configDeadZone);
    printf("LR_src3 = %f\n", LR_src3);
    printf("UD_src3 = %f\n", UD_src3);

    // Destination adapater magnitudes to map to
    float LR_dst1 = 128 - 80;
    float LR_dst2 = 128 - 40;
    float LR_dst3 = 128 -  0;
    float LR_dst4 = 128 +  0;
    float LR_dst5 = 128 + 40;
    float LR_dst6 = 128 + 80;

    float UD_dst1 = 128 + 80;
    float UD_dst2 = 128 + 40;
    float UD_dst3 = 128 +  0;
    float UD_dst4 = 128 +  0;
    float UD_dst5 = 128 - 40;
    float UD_dst6 = 128 - 80;

    // Default to a zero position
    uint8_t stick_lr_val = (uint8_t) LR_dst3;
    uint8_t stick_ud_val = (uint8_t) UD_dst3;

    // Handle STICK_LEFT_RIGHT
    if ( payload[3] < LR_src1 )
    {
        stick_lr_val = LR_dst1;  // Handle STICK_LEFT_RIGHT extreme low
    }
    else if ( payload[3] < LR_src2 )
    {
        stick_lr_val = (uint8_t) (LR_dst1 + (-LR_dst1 + LR_dst2)*(-LR_src1 + ((float) payload[3]))/(-LR_src1 + LR_src2));  // Handle STICK_LEFT_RIGHT ramp
    }
    else if ( payload[3] < LR_src3 )
    {
        stick_lr_val = (uint8_t) (LR_dst2 + (-LR_dst2 + LR_dst3)*(-LR_src2 + ((float) payload[3]))/(-LR_src2 + LR_src3));  // Handle STICK_LEFT_RIGHT ramp
    }
    else if ( payload[3] < LR_src4 )
    {
        stick_lr_val = (uint8_t) (LR_dst3 + (-LR_dst3 + LR_dst4)*(-LR_src3 + ((float) payload[3]))/(-LR_src3 + LR_src4));  // Handle STICK_LEFT_RIGHT ramp
    }
    else if ( payload[3] < LR_src5 )
    {
        stick_lr_val = (uint8_t) (LR_dst4 + (-LR_dst4 + LR_dst5)*(-LR_src4 + ((float) payload[3]))/(-LR_src4 + LR_src5));  // Handle STICK_LEFT_RIGHT ramp
    }
    else if ( payload[3] < LR_src6 )
    {
        stick_lr_val = (uint8_t) (LR_dst5 + (-LR_dst5 + LR_dst6)*(-LR_src5 + ((float) payload[3]))/(-LR_src5 + LR_src6));  // Handle STICK_LEFT_RIGHT ramp
    }
    else
    {
        stick_lr_val = LR_dst6;  // Handle STICK_LEFT_RIGHT extreme high
    }

    // Handle STICK_UP_DOWN
    if ( payload[4] < UD_src1 )
    {
        stick_ud_val = UD_dst1;  // Handle STICK_UP_DOWN extreme low
    }
    else if ( payload[4] < UD_src2 )
    {
        stick_ud_val = (uint8_t) (UD_dst1 + (-UD_dst1 + UD_dst2)*(-UD_src1 + ((float) payload[4]))/(-UD_src1 + UD_src2));  // Handle STICK_UP_DOWN ramp
    }
    else if ( payload[4] < UD_src3 )
    {
        stick_ud_val = (uint8_t) (UD_dst2 + (-UD_dst2 + UD_dst3)*(-UD_src2 + ((float) payload[4]))/(-UD_src2 + UD_src3));  // Handle STICK_UP_DOWN ramp
    }
    else if ( payload[4] < UD_src4 )
    {
        stick_ud_val = (uint8_t) (UD_dst3 + (-UD_dst3 + UD_dst4)*(-UD_src3 + ((float) payload[4]))/(-UD_src3 + UD_src4));  // Handle STICK_UP_DOWN ramp
    }
    else if ( payload[4] < UD_src5 )
    {
        stick_ud_val = (uint8_t) (UD_dst4 + (-UD_dst4 + UD_dst5)*(-UD_src4 + ((float) payload[4]))/(-UD_src4 + UD_src5));  // Handle STICK_UP_DOWN ramp
    }
    else if ( payload[4] < UD_src6 )
    {
        stick_ud_val = (uint8_t) (UD_dst5 + (-UD_dst5 + UD_dst6)*(-UD_src5 + ((float) payload[4]))/(-UD_src5 + UD_src6));  // Handle STICK_UP_DOWN ramp
    }
    else
    {
        stick_ud_val = UD_dst6;  // Handle STICK_UP_DOWN extreme high
    }

    port->axis[0] = stick_lr_val;
    port->axis[1] = stick_ud_val;

    port->axis[2] = payload[5];       // map C-lr to axis 2
    port->axis[3] = 255 - payload[6]; // map C-ud to axis 3
}

static int64_t to_ms(struct timespec *t) {
    return t->tv_sec * 1000 + t->tv_nsec / 1000000;
}

static void *adapter_thread(void *data) {
    struct adapter *a = (struct adapter *) data;

    int bytes_transferred;
    unsigned char payload[1] = { 0x13 };

    int transfer_ret =
        libusb_interrupt_transfer(a->handle, EP_OUT, payload, sizeof(payload), &bytes_transferred, 0);

    if (transfer_ret != LIBUSB_SUCCESS) {
        fprintf(stderr, "libusb_interrupt_transfer: %s\n", libusb_error_name(transfer_ret));
        return NULL;
    }
    if (bytes_transferred != sizeof(payload)) {
        fprintf(stderr, "libusb_interrupt_transfer %d/%lu bytes transferred.\n", bytes_transferred,
                sizeof(payload));
        return NULL;
    }

    while (!a->quitting) {
        // struct timespec time_before = { 0 }, time_after = { 0 };
        unsigned char payload[37];
        int size = 0;
        // clock_gettime(CLOCK_MONOTONIC, &time_before);
        int transfer_ret =
            libusb_interrupt_transfer(a->handle, EP_IN, payload, sizeof(payload), &size, 0);
        // clock_gettime(CLOCK_MONOTONIC, &time_after);
        // printf("Time taken: %d\n", (int)(to_ms(&time_after) - to_ms(&time_before)));
        if (transfer_ret != LIBUSB_SUCCESS) {
            fprintf(stderr, "libusb_interrupt_transfer error %d\n", transfer_ret);
            a->quitting = true;
            break;
        }

        // printf("size %d\n", size);
        // for (int i = 0; i < size; i ++ ){
        //     printf("pl[%d] = %d, ", i, payload[i]);
        // }
        // printf("\n");

        if (size == 8) {
            // hack in our special handling for hyperkin n64
            // no rumble support.
            for (int i = 0; i < 4; i++) {
                handle_hyperkin_payload(i, &a->controllers[i], payload);
            }
            continue;
        }

        if (size != 37 || payload[0] != 0x21)
            continue;

        unsigned char *controller = &payload[1];

        unsigned char rumble[5] = { 0x11, 0, 0, 0, 0 };
        // struct timespec current_time = { 0 };
        // clock_gettime(CLOCK_REALTIME, &current_time);
        // printf("Time: %d %d\n", (int)current_time.tv_sec, (int)current_time.tv_nsec);
        for (int i = 0; i < 4; i++, controller += 9) {
            handle_payload(i, &a->controllers[i], controller);
            rumble[i + 1] = 0;
            /*if (a->controllers[i].extra_power && a->controllers[i].type == STATE_NORMAL)
            {
               for (int j = 0; j < MAX_FF_EVENTS; j++)
               {
                  struct ff_event *e = &a->controllers[i].ff_events[j];
                  if (e->in_use)
                  {
                     if (ts_lessthan(&e->start_time, &current_time) && ts_greaterthan(&e->end_time,
            &current_time)) rumble[i+1] = 1; else update_ff_start_stop(e, &current_time);
                  }
               }
            }*/
        }

        if (memcmp(rumble, a->rumble, sizeof(rumble)) != 0) {
            memcpy(a->rumble, rumble, sizeof(rumble));
            transfer_ret =
                libusb_interrupt_transfer(a->handle, EP_OUT, a->rumble, sizeof(a->rumble), &size, 0);
            if (transfer_ret != 0) {
                fprintf(stderr, "libusb_interrupt_transfer error %d\n", transfer_ret);
                a->quitting = true;
                break;
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        /*if (a->controllers[i].connected)
           uinput_destroy(i, &a->controllers[i]);*/
    }

    return NULL;
}

static void add_adapter(struct libusb_device *dev) {
    struct adapter *a = calloc(1, sizeof(struct adapter));
    if (a == NULL) {
        fprintf(stderr, "FATAL: calloc() failed");
        exit(-1);
    }
    a->device = dev;

    int open_status = libusb_open(a->device, &a->handle);
    if (open_status != LIBUSB_SUCCESS) {
        fprintf(stderr, "Error %d opening device 0x%p\n", open_status, a->device);
        return;
    } else {
        printf("Opened WUP adapter\n");
        /*unsigned char dev_desc[512] = {0};*/
        /*int ret = libusb_get_string_descriptor_ascii(&a->handle, 0, dev_desc, sizeof(dev_desc));*/
        /*if (libusb_error.LIBUSB_SUCCESS == ret){*/
        /*if (ret == 0){                          */
        /*    printf("%d\n", ret);                */
        /*    printf("%s\n", dev_desc);           */
        /*}                                       */
    }

    if (libusb_kernel_driver_active(a->handle, 0) == 1) {
        fprintf(stderr, "Detaching kernel driver\n");
        if (libusb_detach_kernel_driver(a->handle, 0)) {
            fprintf(stderr, "Error detaching handle 0x%p from kernel\n", a->handle);
            return;
        }
    }

    struct adapter *old_head = adapters.next;
    adapters.next = a;
    a->next = old_head;

    pthread_create(&a->thread, NULL, adapter_thread, a);

    fprintf(stderr, "adapter 0x%p connected\n", a->device);
}

static void remove_adapter(struct libusb_device *dev) {
    struct adapter *a = &adapters;
    while (a->next != NULL) {
        if (a->next->device == dev) {
            a->next->quitting = true;
            pthread_join(a->next->thread, NULL);
            fprintf(stderr, "adapter 0x%p disconnected\n", a->next->device);
            libusb_close(a->next->handle);
            struct adapter *new_next = a->next->next;
            free(a->next);
            a->next = new_next;
            return;
        }

        a = a->next;
    }
}

static int LIBUSB_CALL hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                                        libusb_hotplug_event event, void *user_data) {
    (void) ctx;
    (void) user_data;
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        add_adapter(dev);
    } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        remove_adapter(dev);
    }

    return 0;
}

void *wup_start(UNUSED void *a) {
    libusb_init(NULL);

    struct libusb_device **devices;

    int count = libusb_get_device_list(NULL, &devices);
    printf("WUP num devices = %d\n", count);

    for (int i = 0; i < count; i++) {
        struct libusb_device_descriptor desc;

        libusb_get_device_descriptor(devices[i], &desc);
        if (desc.idVendor == 0x057e && desc.idProduct == 0x0337) {
            printf("Recognized WUP Vendor:Device = %04x:%04x\n", desc.idVendor, desc.idProduct);
            add_adapter(devices[i]);
        }
        // BDA N64 Hyperkin Adapter
        else if (desc.idVendor == 0x20d6 && desc.idProduct == 0xa710) {
            printf("Recognized WUP Vendor:Device = %04x:%04x\n", desc.idVendor, desc.idProduct);
            add_adapter(devices[i]);
        } else {
            // printf("Unrecognized WUP Vendor:Device = %04x:%04x\n", desc.idVendor, desc.idProduct);
        }
    }

    if (count > 0)
        libusb_free_device_list(devices, 1);

    libusb_hotplug_callback_handle callback;

    int hotplug_capability = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
    if (hotplug_capability) {
        int hotplug_ret = libusb_hotplug_register_callback(
            NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
            LIBUSB_HOTPLUG_NO_FLAGS, 0x057e, 0x0337, LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, NULL,
            &callback);

        if (hotplug_ret != LIBUSB_SUCCESS) {
            fprintf(stderr, "cannot register hotplug callback, hotplugging not enabled\n");
            hotplug_capability = 0;
        }
    }

    // pump events until shutdown & all helper threads finish cleaning up
    while (!quitting)
        libusb_handle_events_completed(NULL, (int *) &quitting);

    while (adapters.next)
        remove_adapter(adapters.next->device);

    if (hotplug_capability)
        libusb_hotplug_deregister_callback(NULL, callback);

    libusb_exit(NULL);
    return (void *) 0;
}
#endif

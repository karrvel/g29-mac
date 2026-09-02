/*
 * lgwheel.c — userspace Logitech wheel force-feedback control for macOS
 *
 * macOS ships no driver for Logitech's racing wheels: G HUB on the Mac does not
 * cover them, and the wheels speak Logitech's proprietary FFB protocol rather
 * than the USB-HID PID class that Apple's ForceFeedback.framework understands.
 * Linux solves this in the kernel with hid-lg4ff / new-lg4ff.
 *
 * This is the same wire protocol, sent from userspace via IOHIDDeviceSetReport.
 * Command bytes are transcribed from berarma/new-lg4ff (hid-lg4ff.c):
 *
 *   mode switch to G29 native   f8 0a 00 00 00 00 00  then  f8 09 05 01 01 00 00
 *   set rotation range          f8 81 <lo> <hi> 00 00 00
 *   constant force, slot 0      11 00 <force> 00 00 00 00   (0x80 = neutral)
 *   stop slot 0                 13 00 00 00 00 00 00
 *   autocentre off              f5 00 00 00 00 00 00
 *   autocentre set              fe 0d <k1> <k2> <clip> 00 00  then 14 00 ...
 *
 * Build:
 *   clang -O2 -o lgwheel lgwheel.c -framework IOKit -framework CoreFoundation
 *
 * Usage:
 *   ./lgwheel --detect                 list Logitech wheels and their mode
 *   ./lgwheel --native                 switch a G29 out of compatibility mode
 *   ./lgwheel --range 900              set rotation range in degrees (40-900)
 *   ./lgwheel --autocentre 60          constant self-centring spring, 0-100
 *   ./lgwheel --force -70 --hold 2     hold a constant torque, -100..100
 *   ./lgwheel --stop                   kill all forces
 *   ./lgwheel --test                   full torque test sequence
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDValue.h>

#define LOGITECH_VID 0x046d

struct wheel_id { uint32_t pid; const char *name; int native; };

/* PIDs from linux hid-ids.h. native==0 means the wheel is in a backward
 * compatibility mode and is pretending to be an older model. */
static const struct wheel_id KNOWN[] = {
    { 0xc24f, "G29 Driving Force Racing Wheel",     1 },
    { 0xc260, "G29 (PS4 native mode)",              1 },
    { 0xc262, "G920 Driving Force Racing Wheel",    1 },
    { 0xc266, "G923 Racing Wheel (PC/PS)",          1 },
    { 0xc267, "G923 Racing Wheel (PlayStation)",    1 },
    { 0xc26e, "G923 Racing Wheel (Xbox)",           1 },
    { 0xc294, "Driving Force / Formula EX  <-- COMPATIBILITY MODE", 0 },
    { 0xc295, "MOMO Force",                         1 },
    { 0xc298, "Driving Force Pro",                  1 },
    { 0xc299, "G25 Racing Wheel",                   1 },
    { 0xc29a, "Driving Force GT",                   1 },
    { 0xc29b, "G27 Racing Wheel",                   1 },
    { 0xca03, "MOMO Racing",                        1 },
    { 0, NULL, 0 }
};

static const struct wheel_id *lookup(uint32_t pid) {
    for (int i = 0; KNOWN[i].name; i++) if (KNOWN[i].pid == pid) return &KNOWN[i];
    return NULL;
}

static long dev_num_prop(IOHIDDeviceRef d, CFStringRef key) {
    long v = 0;
    CFTypeRef r = IOHIDDeviceGetProperty(d, key);
    if (r && CFGetTypeID(r) == CFNumberGetTypeID())
        CFNumberGetValue((CFNumberRef)r, kCFNumberLongType, &v);
    return v;
}

static void dev_str_prop(IOHIDDeviceRef d, CFStringRef key, char *buf, size_t n) {
    buf[0] = '\0';
    CFTypeRef r = IOHIDDeviceGetProperty(d, key);
    if (r && CFGetTypeID(r) == CFStringGetTypeID())
        CFStringGetCString((CFStringRef)r, buf, (CFIndex)n, kCFStringEncodingUTF8);
}

/* ---- the wire ---- */

/* The wheel's output report is longer than the 7 command bytes (16 bytes on the
 * G29's joystick interface). Linux's hid_hw_request pads to the descriptor
 * length for us; over IOHIDDeviceSetReport we must pad it ourselves, or the USB
 * endpoint answers with a STALL (0xe0005000). */
static size_t g_out_len = 16;

static int send_cmd(IOHIDDeviceRef dev, const uint8_t c[7], const char *what) {
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, c, 7);
    size_t len = (g_out_len >= 7 && g_out_len <= sizeof(buf)) ? g_out_len : 7;

    IOReturn r = IOHIDDeviceSetReport(dev, kIOHIDReportTypeOutput, 0, buf, len);
    printf("   %-22s  %02x %02x %02x %02x %02x %02x %02x  (%zub)  %s\n",
           what, c[0], c[1], c[2], c[3], c[4], c[5], c[6], len,
           (r == kIOReturnSuccess) ? "ok" : "FAILED");
    if (r != kIOReturnSuccess) {
        if (r == kIOReturnNotPermitted || r == kIOReturnNotPrivileged)
            printf("      -> not permitted. Grant your terminal Input Monitoring in\n"
                   "         System Settings > Privacy & Security > Input Monitoring.\n");
        else if (r == 0xe0005000)
            printf("      -> USB pipe STALL: the wheel rejected this report length (%zu).\n", len);
        else
            printf("      -> IOReturn 0x%08x\n", r);
        return -1;
    }
    return 0;
}

static void cmd_native(IOHIDDeviceRef dev) {
    const uint8_t revert[7] = { 0xf8, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t g29[7]    = { 0xf8, 0x09, 0x05, 0x01, 0x01, 0x00, 0x00 };
    printf("-- switching wheel to G29 native mode (it will re-enumerate)\n");
    send_cmd(dev, revert, "revert-on-reset");
    send_cmd(dev, g29,    "switch-to-G29");
    printf("   The wheel should now disconnect and come back as 046d:c24f.\n"
           "   Re-run --detect in a few seconds to confirm.\n");
}

static void cmd_range(IOHIDDeviceRef dev, int deg) {
    if (deg < 40) deg = 40;
    if (deg > 900) deg = 900;
    uint8_t c[7] = { 0xf8, 0x81, (uint8_t)(deg & 0xff), (uint8_t)((deg >> 8) & 0xff), 0, 0, 0 };
    printf("-- setting rotation range to %d degrees\n", deg);
    send_cmd(dev, c, "set-range");
}

static void cmd_autocentre(IOHIDDeviceRef dev, int pct) {
    if (pct <= 0) {
        const uint8_t off[7] = { 0xf5, 0, 0, 0, 0, 0, 0 };
        printf("-- autocentre OFF\n");
        send_cmd(dev, off, "autocentre-off");
        return;
    }
    if (pct > 100) pct = 100;
    uint32_t magnitude = (uint32_t)((65535.0 * pct) / 100.0);
    uint32_t expand_a, expand_b;
    if (magnitude <= 0xaaaa) {
        expand_a = 0x0c * magnitude;
        expand_b = 0x80 * magnitude;
    } else {
        expand_a = (0x0c * 0xaaaa) + 0x06 * (magnitude - 0xaaaa);
        expand_b = (0x80 * 0xaaaa) + 0xff * (magnitude - 0xaaaa);
    }
    expand_a = expand_a >> 1;   /* non-MOMO wheels */

    uint8_t set[7] = { 0xfe, 0x0d,
                       (uint8_t)(expand_a / 0xaaaa),
                       (uint8_t)(expand_a / 0xaaaa),
                       (uint8_t)(expand_b / 0xaaaa), 0, 0 };
    const uint8_t go[7] = { 0x14, 0, 0, 0, 0, 0, 0 };
    printf("-- autocentre spring at %d%%\n", pct);
    send_cmd(dev, set, "autocentre-set");
    send_cmd(dev, go,  "autocentre-on");
}

static void cmd_force(IOHIDDeviceRef dev, int pct) {
    if (pct < -100) pct = -100;
    if (pct > 100) pct = 100;
    /* level is s16; TRANSLATE_FORCE(x) = (clamp_s16(x) + 0x8000) >> 8 */
    int level = (int)((32767.0 * pct) / 100.0);
    uint8_t f = (uint8_t)(((level + 0x8000) >> 8) & 0xff);
    uint8_t c[7] = { 0x11, 0x00, f, 0, 0, 0, 0 };
    printf("-- constant force %+d%% (byte 0x%02x, 0x80 = neutral)\n", pct, f);
    send_cmd(dev, c, "constant-force");
}

static void cmd_stop(IOHIDDeviceRef dev) {
    const uint8_t stop[7] = { 0x13, 0, 0, 0, 0, 0, 0 };
    const uint8_t acoff[7] = { 0xf5, 0, 0, 0, 0, 0, 0 };
    printf("-- stopping all forces\n");
    send_cmd(dev, stop,  "stop-slot-0");
    send_cmd(dev, acoff, "autocentre-off");
}

/* ---- objective verification ----
 * Apply force to a free-standing wheel and watch the steering axis. If the
 * motor is really driving, the reported position moves on its own. This proves
 * torque without anyone having to put a hand on the rim. */

static IOHIDElementRef g_x_element = NULL;   /* steering axis element */

/* HID input callbacks only fire on change, so a stationary wheel reports
 * nothing. Poll the element directly instead. */
static IOHIDElementRef find_x_element(IOHIDDeviceRef dev) {
    CFArrayRef els = IOHIDDeviceCopyMatchingElements(dev, NULL, kIOHIDOptionsTypeNone);
    if (!els) return NULL;
    IOHIDElementRef found = NULL;
    for (CFIndex i = 0; i < CFArrayGetCount(els); i++) {
        IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(els, i);
        if (IOHIDElementGetType(e) != kIOHIDElementTypeInput_Misc &&
            IOHIDElementGetType(e) != kIOHIDElementTypeInput_Axis) continue;
        if (IOHIDElementGetUsagePage(e) != 0x01) continue;
        if (IOHIDElementGetUsage(e) != 0x30) continue;      /* GD X */
        /* prefer the widest axis: the G29 steering is 16-bit */
        if (!found || (IOHIDElementGetLogicalMax(e) > IOHIDElementGetLogicalMax(found)))
            found = e;
    }
    if (found) CFRetain(found);
    CFRelease(els);
    return found;
}

static double read_axis(IOHIDDeviceRef dev) {
    if (!g_x_element) return -1.0;
    IOHIDValueRef val = NULL;
    if (IOHIDDeviceGetValue(dev, g_x_element, &val) != kIOReturnSuccess || !val)
        return -1.0;
    CFIndex v  = IOHIDValueGetIntegerValue(val);
    CFIndex lo = IOHIDElementGetLogicalMin(g_x_element);
    CFIndex hi = IOHIDElementGetLogicalMax(g_x_element);
    if (hi <= lo) return -1.0;
    return (double)(v - lo) / (double)(hi - lo);
}

static void pump(double seconds) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
}

/* Sample for `seconds`, returning the final position and reporting how far it
 * travelled in the meantime (a wheel under torque keeps moving). */
static double sample_axis(IOHIDDeviceRef dev, double seconds) {
    double last = read_axis(dev), lo = last, hi = last;
    int steps = (int)(seconds / 0.05);
    for (int i = 0; i < steps; i++) {
        pump(0.05);
        double v = read_axis(dev);
        if (v < 0) continue;
        last = v;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    if (hi > lo) printf("      (travelled across %.4f of full lock while sampling)\n", hi - lo);
    return last;
}

static void cmd_verify(IOHIDDeviceRef dev) {
    printf("\n=== OBJECTIVE TORQUE VERIFICATION ===\n");
    printf("Let go of the wheel — it must be free to turn by itself.\n\n");

    IOHIDDeviceScheduleWithRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    g_x_element = find_x_element(dev);
    if (!g_x_element) printf("warning: no steering (GD X) element found\n");
    pump(0.5);

    cmd_stop(dev);
    pump(1.0);
    double base = sample_axis(dev, 1.0);
    if (base < 0) {
        printf("could not read the steering axis — cannot verify automatically.\n");
        IOHIDDeviceUnscheduleFromRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        return;
    }
    printf("baseline steering position : %.4f\n\n", base);

    printf("applying LEFT force (80%%) for 2s...\n");
    cmd_force(dev, -80);
    double left = sample_axis(dev, 2.0);
    cmd_force(dev, 0);
    pump(0.3);
    printf("  position after left force : %.4f   (delta %+.4f)\n\n", left, left - base);

    printf("applying RIGHT force (80%%) for 2.5s...\n");
    cmd_force(dev, 80);
    double right = sample_axis(dev, 2.5);
    cmd_force(dev, 0);
    pump(0.3);
    printf("  position after right force: %.4f   (delta %+.4f)\n\n", right, right - left);

    cmd_stop(dev);
    IOHIDDeviceUnscheduleFromRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    double swing = right - left;
    printf("-------------------------------------------\n");
    if (swing > 0.05) {
        printf("VERDICT: TORQUE CONFIRMED. The wheel moved %.1f%% of its travel\n"
               "         under commanded force. Force feedback works on this Mac.\n",
               swing * 100.0);
    } else if (swing < -0.05) {
        printf("VERDICT: TORQUE CONFIRMED (axis inverted). Movement of %.1f%%.\n",
               -swing * 100.0);
    } else {
        printf("VERDICT: NO MOVEMENT DETECTED (swing %.4f).\n", swing);
        printf("         Either the wheel was held/blocked, its power brick is\n"
               "         unplugged, or the reports are being accepted and ignored.\n");
    }
}

/* ---- axis identification ----
 * Which physical pedal is which HID axis is not guessable — the G29 reports
 * three pedal axes plus steering and games map them by usage, not by label.
 * Watch every axis at once and report what actually moved. */

#define MAX_AXES 16

struct axis_watch {
    IOHIDElementRef el;
    uint32_t usage;
    const char *name;
    double cur, min, max, last_report;
    int moved;
};

static const char *usage_name(uint32_t u) {
    switch (u) {
        case 0x30: return "X      (steering)";
        case 0x31: return "Y      ";
        case 0x32: return "Z      ";
        case 0x33: return "Rx     ";
        case 0x34: return "Ry     ";
        case 0x35: return "Rz     ";
        case 0x36: return "Slider ";
        case 0x37: return "Dial   ";
        default:   return "?      ";
    }
}

static double element_value(IOHIDDeviceRef dev, IOHIDElementRef el) {
    IOHIDValueRef v = NULL;
    if (IOHIDDeviceGetValue(dev, el, &v) != kIOReturnSuccess || !v) return -1.0;
    CFIndex raw = IOHIDValueGetIntegerValue(v);
    CFIndex lo = IOHIDElementGetLogicalMin(el), hi = IOHIDElementGetLogicalMax(el);
    if (hi <= lo) return -1.0;
    return (double)(raw - lo) / (double)(hi - lo);
}

static void cmd_axes(IOHIDDeviceRef dev, int seconds) {
    struct axis_watch ax[MAX_AXES];
    int n = 0;

    CFArrayRef els = IOHIDDeviceCopyMatchingElements(dev, NULL, kIOHIDOptionsTypeNone);
    if (!els) { printf("no elements\n"); return; }
    for (CFIndex i = 0; i < CFArrayGetCount(els) && n < MAX_AXES; i++) {
        IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(els, i);
        IOHIDElementType t = IOHIDElementGetType(e);
        if (t != kIOHIDElementTypeInput_Misc && t != kIOHIDElementTypeInput_Axis) continue;
        if (IOHIDElementGetUsagePage(e) != 0x01) continue;
        uint32_t u = IOHIDElementGetUsage(e);
        if (u < 0x30 || u > 0x37) continue;
        int dup = 0;
        for (int k = 0; k < n; k++) if (ax[k].usage == u) dup = 1;
        if (dup) continue;
        ax[n].el = e; ax[n].usage = u; ax[n].name = usage_name(u);
        ax[n].cur = ax[n].min = ax[n].max = ax[n].last_report = element_value(dev, e);
        ax[n].moved = 0;
        n++;
    }

    IOHIDDeviceScheduleWithRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    pump(0.3);

    printf("watching %d axes for %d seconds.\n\n", n, seconds);
    printf("PRESS ONE PEDAL AT A TIME, all the way down, pausing between them:\n");
    printf("   1. accelerator   2. brake   3. clutch\n");
    printf("(then turn the wheel left and right)\n\n");
    printf("resting values:\n");
    for (int i = 0; i < n; i++) {
        ax[i].cur = ax[i].min = ax[i].max = ax[i].last_report = element_value(dev, ax[i].el);
        printf("   %s = %.3f\n", ax[i].name, ax[i].cur);
    }
    printf("\n--- movement log ---\n");
    fflush(stdout);

    int steps = seconds * 20;
    for (int s = 0; s < steps; s++) {
        pump(0.05);
        for (int i = 0; i < n; i++) {
            double v = element_value(dev, ax[i].el);
            if (v < 0) continue;
            ax[i].cur = v;
            if (v < ax[i].min) ax[i].min = v;
            if (v > ax[i].max) ax[i].max = v;
            if (fabs(v - ax[i].last_report) > 0.15) {
                printf("[t=%5.1fs] %s %.3f -> %.3f\n",
                       s * 0.05, ax[i].name, ax[i].last_report, v);
                fflush(stdout);
                ax[i].last_report = v;
                ax[i].moved = 1;
            }
        }
    }

    IOHIDDeviceUnscheduleFromRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFRelease(els);

    printf("\n--- summary: what moved ---\n");
    printf("   axis                 min     max    range   verdict\n");
    for (int i = 0; i < n; i++) {
        double range = ax[i].max - ax[i].min;
        printf("   %s  %.3f   %.3f   %.3f   %s\n", ax[i].name,
               ax[i].min, ax[i].max, range,
               range > 0.2 ? "<== MOVED" : "(idle)");
    }
    printf("\nAssign the moved axes in the game by these usage names.\n");
}

static void cmd_test(IOHIDDeviceRef dev) {
    printf("\n=== TORQUE TEST — hold the wheel ===\n\n");
    cmd_stop(dev);
    cmd_range(dev, 900);
    sleep(1);

    printf("\n[1/4] pushing LEFT at 60%% for 2s...\n");
    cmd_force(dev, -60);
    sleep(2);
    cmd_force(dev, 0);

    printf("\n[2/4] pushing RIGHT at 60%% for 2s...\n");
    cmd_force(dev, 60);
    sleep(2);
    cmd_force(dev, 0);

    printf("\n[3/4] autocentre spring at 70%% for 5s — turn the wheel, it should\n"
           "      fight back and snap to centre when released...\n");
    cmd_autocentre(dev, 70);
    sleep(5);

    printf("\n[4/4] all forces off.\n");
    cmd_stop(dev);
    printf("\n=== test finished ===\n");
    printf("If you felt torque in steps 1-3, force feedback WORKS on this Mac.\n");
    printf("If the wheel never moved but every command said 'ok', the wheel is\n"
           "accepting reports but is in a mode that ignores them — try --native.\n");
}

int main(int argc, char **argv) {
    int want_detect = (argc == 1), want_native = 0, want_test = 0, want_stop = 0, want_verify = 0, axes_secs = 0;
    int range = -1, autoc = -1, force = 999, hold = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--detect"))     want_detect = 1;
        else if (!strcmp(argv[i], "--native"))     want_native = 1;
        else if (!strcmp(argv[i], "--test"))       want_test = 1;
        else if (!strcmp(argv[i], "--verify"))     want_verify = 1;
        else if (!strcmp(argv[i], "--axes")) { axes_secs = (i + 1 < argc && argv[i+1][0] != '-') ? atoi(argv[++i]) : 30; }
        else if (!strcmp(argv[i], "--stop"))       want_stop = 1;
        else if (!strcmp(argv[i], "--range")      && i + 1 < argc) range = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--autocentre") && i + 1 < argc) autoc = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--autocenter") && i + 1 < argc) autoc = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--force")      && i + 1 < argc) force = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hold")       && i + 1 < argc) hold  = atoi(argv[++i]);
        else { fprintf(stderr, "unknown option: %s\n", argv[i]); return 1; }
    }

    IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!mgr) { fprintf(stderr, "IOHIDManagerCreate failed\n"); return 1; }
    IOHIDManagerSetDeviceMatching(mgr, NULL);   /* everything, we filter by VID */
    IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);

    CFSetRef set = IOHIDManagerCopyDevices(mgr);
    if (!set) { fprintf(stderr, "no HID devices visible\n"); return 2; }
    CFIndex n = CFSetGetCount(set);
    IOHIDDeviceRef *devs = calloc((size_t)n, sizeof(IOHIDDeviceRef));
    CFSetGetValues(set, (const void **)devs);

    IOHIDDeviceRef wheel = NULL;
    const struct wheel_id *wid = NULL;
    int listed = 0, have_joystick = 0;

    printf("lgwheel — userspace Logitech wheel FFB for macOS\n");
    printf("================================================\n\n");

    for (CFIndex i = 0; i < n; i++) {
        long vid = dev_num_prop(devs[i], CFSTR(kIOHIDVendorIDKey));
        long pid = dev_num_prop(devs[i], CFSTR(kIOHIDProductIDKey));
        if (vid != LOGITECH_VID) continue;
        char product[256];
        dev_str_prop(devs[i], CFSTR(kIOHIDProductKey), product, sizeof(product));
        const struct wheel_id *k = lookup((uint32_t)pid);
        long upage = dev_num_prop(devs[i], CFSTR(kIOHIDPrimaryUsagePageKey));
        long usage = dev_num_prop(devs[i], CFSTR(kIOHIDPrimaryUsageKey));
        long omax  = dev_num_prop(devs[i], CFSTR(kIOHIDMaxOutputReportSizeKey));
        printf("found 046d:%04lx  %s\n", pid, product[0] ? product : "(unnamed)");
        printf("      usage page 0x%02lx usage 0x%02lx, output report %ld bytes\n",
               upage, usage, omax);
        if (k) {
            printf("      => %s\n", k->name);
            if (!k->native)
                printf("      => run --native to unlock full FFB and 900 degrees\n");
            /* The FFB output report lives on the joystick interface
             * (usage page 0x01 Generic Desktop, usage 0x04 Joystick), not on
             * the vendor-specific one. Pick that, whatever order the set is in. */
            int is_joystick = (upage == 0x01 && (usage == 0x04 || usage == 0x05));
            if (is_joystick || !wheel) {
                if (is_joystick || !have_joystick) {
                    wheel = devs[i]; wid = k;
                    if (omax > 0) g_out_len = (size_t)omax;
                    if (is_joystick) { have_joystick = 1; printf("      => selected (joystick interface)\n"); }
                }
            }
        } else {
            printf("      => not a known wheel (mouse/keyboard/headset?)\n");
        }
        listed++;
    }

    if (!listed) {
        printf("No Logitech device found at all.\n\n"
               "Checklist:\n"
               "  1. USB cable connected to the Mac (a hub can work, try direct first)\n"
               "  2. the wheel's external power brick is plugged in and on\n"
               "  3. on a G29, the PS3/PS4 mode switch is set to PS4\n"
               "  4. the wheel does its calibration sweep at power-on\n");
        return 2;
    }
    if (!wheel) { printf("\nLogitech devices present but none is a known wheel.\n"); return 2; }

    IOReturn open_r = IOHIDDeviceOpen(wheel, kIOHIDOptionsTypeNone);
    if (open_r != kIOReturnSuccess) {
        printf("\nIOHIDDeviceOpen failed (0x%08x).\n", open_r);
        printf("Grant your terminal Input Monitoring in\n"
               "System Settings > Privacy & Security > Input Monitoring, then retry.\n");
        return 3;
    }
    printf("\nopened %s\n\n", wid->name);

    if (want_native)      cmd_native(wheel);
    if (range   >= 0)     cmd_range(wheel, range);
    if (autoc   >= 0)     cmd_autocentre(wheel, autoc);
    if (force   != 999) { cmd_force(wheel, force); if (hold > 0) { sleep((unsigned)hold); cmd_force(wheel, 0); } }
    if (want_test)        cmd_test(wheel);
    if (want_verify)      cmd_verify(wheel);
    if (axes_secs > 0)    cmd_axes(wheel, axes_secs);
    if (want_stop)        cmd_stop(wheel);
    if (want_detect && !want_native && range < 0 && autoc < 0 && force == 999 && !want_test && !want_stop && !want_verify && axes_secs <= 0)
        printf("(detect only — try --test to actually command torque)\n");

    IOHIDDeviceClose(wheel, kIOHIDOptionsTypeNone);
    CFRelease(set);
    free(devs);
    return 0;
}

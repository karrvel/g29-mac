/*
 * ffb-probe.c — Force Feedback probe for macOS (Apple Silicon)
 *
 * Answers one question definitively: can macOS's IOKit ForceFeedback stack
 * drive the motor in an attached wheel (Logitech G29 or anything else)?
 *
 * Enumerates every IOHIDDevice, reports which ones macOS considers
 * force-feedback capable, dumps their FF capabilities, and — with --spin —
 * actually commands a constant force and a spring so you can feel whether
 * torque is real.
 *
 * Build:
 *   clang -O2 -o ffb-probe ffb-probe.c \
 *       -framework IOKit -framework ForceFeedback -framework CoreFoundation \
 *       -Wno-deprecated-declarations
 *
 * Usage:
 *   ./ffb-probe            # detect + report only, never moves the wheel
 *   ./ffb-probe --spin     # ALSO applies real force: hold the wheel first
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <ForceFeedback/ForceFeedback.h>

static int g_found = 0;
static int g_ff_capable = 0;

static long get_num_prop(io_service_t svc, CFStringRef key, long fallback) {
    long out = fallback;
    CFTypeRef ref = IORegistryEntrySearchCFProperty(
        svc, kIOServicePlane, key, kCFAllocatorDefault,
        kIORegistryIterateRecursively | kIORegistryIterateParents);
    if (ref) {
        if (CFGetTypeID(ref) == CFNumberGetTypeID())
            CFNumberGetValue((CFNumberRef)ref, kCFNumberLongType, &out);
        CFRelease(ref);
    }
    return out;
}

static void get_str_prop(io_service_t svc, CFStringRef key, char *buf, size_t len) {
    buf[0] = '\0';
    CFTypeRef ref = IORegistryEntrySearchCFProperty(
        svc, kIOServicePlane, key, kCFAllocatorDefault,
        kIORegistryIterateRecursively | kIORegistryIterateParents);
    if (ref) {
        if (CFGetTypeID(ref) == CFStringGetTypeID())
            CFStringGetCString((CFStringRef)ref, buf, (CFIndex)len, kCFStringEncodingUTF8);
        CFRelease(ref);
    }
}

static void print_effect_support(UInt32 mask) {
    struct { UInt32 bit; const char *name; } tbl[] = {
        { FFCAP_ET_CONSTANTFORCE, "ConstantForce" },
        { FFCAP_ET_RAMPFORCE,     "RampForce"     },
        { FFCAP_ET_SQUARE,        "Square"        },
        { FFCAP_ET_SINE,          "Sine"          },
        { FFCAP_ET_TRIANGLE,      "Triangle"      },
        { FFCAP_ET_SAWTOOTHUP,    "SawtoothUp"    },
        { FFCAP_ET_SAWTOOTHDOWN,  "SawtoothDown"  },
        { FFCAP_ET_SPRING,        "Spring"        },
        { FFCAP_ET_DAMPER,        "Damper"        },
        { FFCAP_ET_INERTIA,       "Inertia"       },
        { FFCAP_ET_FRICTION,      "Friction"      },
        { FFCAP_ET_CUSTOMFORCE,   "CustomForce"   },
        { 0, NULL }
    };
    printf("      effects      : ");
    int n = 0;
    for (int i = 0; tbl[i].name; i++) {
        if (mask & tbl[i].bit) { printf("%s%s", n++ ? ", " : "", tbl[i].name); }
    }
    if (!n) printf("(none reported)");
    printf("\n");
}

/* Apply a real constant force, then a spring. The wheel should visibly pull. */
static void spin_test(FFDeviceObjectReference dev) {
    HRESULT hr;

    printf("\n   >> TORQUE TEST — hold the wheel, it is about to push.\n");
    fflush(stdout);
    sleep(1);

    FFDeviceSendForceFeedbackCommand(dev, FFSFFC_RESET);
    hr = FFDeviceSendForceFeedbackCommand(dev, FFSFFC_SETACTUATORSON);
    printf("      actuators ON : %s (0x%lx)\n",
           (hr == FF_OK) ? "ok" : "FAILED", (unsigned long)hr);

    /* Gain to maximum so a weak result means "weak hardware", not "weak request". */
    UInt32 gain = 10000;
    FFDeviceSetForceFeedbackProperty(dev, FFPROP_FFGAIN, &gain);

    /* ---- 1. Constant force, right, 2 seconds ---- */
    DWORD axes[1] = { FFJOFS_X };
    LONG  dir[1]  = { 1 };
    FFCONSTANTFORCE cf;
    memset(&cf, 0, sizeof(cf));
    cf.lMagnitude = 7000;              /* of 10000 */

    FFEFFECT eff;
    memset(&eff, 0, sizeof(eff));
    eff.dwSize                = sizeof(FFEFFECT);
    eff.dwFlags               = FFEFF_OBJECTOFFSETS | FFEFF_CARTESIAN;
    eff.dwDuration            = 2 * 1000000;   /* microseconds */
    eff.dwSamplePeriod        = 0;
    eff.dwGain                = 10000;
    eff.dwTriggerButton       = FFEB_NOTRIGGER;
    eff.dwTriggerRepeatInterval = 0;
    eff.cAxes                 = 1;
    eff.rgdwAxes              = axes;
    eff.rglDirection          = dir;
    eff.lpEnvelope            = NULL;
    eff.cbTypeSpecificParams  = sizeof(FFCONSTANTFORCE);
    eff.lpvTypeSpecificParams = &cf;
    eff.dwStartDelay          = 0;

    FFEffectObjectReference e1 = NULL;
    hr = FFDeviceCreateEffect(dev, kFFEffectType_ConstantForce_ID, &eff, &e1);
    if (hr == FF_OK) {
        printf("      constant fx  : created, running 2s at 70%% right...\n");
        fflush(stdout);
        hr = FFEffectStart(e1, 1, 0);
        printf("      FFEffectStart: %s (0x%lx)\n",
               (hr == FF_OK) ? "ok" : "FAILED", (unsigned long)hr);
        sleep(3);
        FFEffectStop(e1);
        FFDeviceReleaseEffect(dev, e1);
    } else {
        printf("      constant fx  : CREATE FAILED (0x%lx)\n", (unsigned long)hr);
    }

    /* ---- 2. Spring centering, 4 seconds ---- */
    FFCONDITION cond;
    memset(&cond, 0, sizeof(cond));
    cond.dwPositiveSaturation =  10000;
    cond.dwNegativeSaturation =  10000;
    cond.lPositiveCoefficient =   8000;
    cond.lNegativeCoefficient =   8000;
    cond.lOffset              =      0;
    cond.lDeadBand            =      0;

    FFEFFECT eff2;
    memset(&eff2, 0, sizeof(eff2));
    eff2.dwSize                 = sizeof(FFEFFECT);
    eff2.dwFlags                = FFEFF_OBJECTOFFSETS | FFEFF_CARTESIAN;
    eff2.dwDuration             = FF_INFINITE;
    eff2.dwGain                 = 10000;
    eff2.dwTriggerButton        = FFEB_NOTRIGGER;
    eff2.cAxes                  = 1;
    eff2.rgdwAxes               = axes;
    eff2.rglDirection           = dir;
    eff2.cbTypeSpecificParams   = sizeof(FFCONDITION);
    eff2.lpvTypeSpecificParams  = &cond;

    FFEffectObjectReference e2 = NULL;
    hr = FFDeviceCreateEffect(dev, kFFEffectType_Spring_ID, &eff2, &e2);
    if (hr == FF_OK) {
        printf("      spring fx    : created, centering for 4s — turn the wheel,\n"
               "                     it should fight back and self-centre...\n");
        fflush(stdout);
        FFEffectStart(e2, 1, 0);
        sleep(4);
        FFEffectStop(e2);
        FFDeviceReleaseEffect(dev, e2);
    } else {
        printf("      spring fx    : CREATE FAILED (0x%lx)\n", (unsigned long)hr);
    }

    FFDeviceSendForceFeedbackCommand(dev, FFSFFC_SETACTUATORSOFF);
    FFDeviceSendForceFeedbackCommand(dev, FFSFFC_RESET);
    printf("   >> torque test finished.\n");
}

int main(int argc, char **argv) {
    int do_spin = 0;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--spin")) do_spin = 1;

    printf("ffb-probe — macOS IOKit ForceFeedback probe\n");
    printf("===========================================\n\n");

    CFMutableDictionaryRef match = IOServiceMatching(kIOHIDDeviceKey);
    if (!match) { fprintf(stderr, "IOServiceMatching failed\n"); return 1; }

    io_iterator_t iter = 0;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, match, &iter);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices failed: 0x%x\n", kr);
        return 1;
    }

    io_service_t svc;
    while ((svc = IOIteratorNext(iter))) {
        char product[256], vendor[256];
        get_str_prop(svc, CFSTR(kIOHIDProductKey), product, sizeof(product));
        get_str_prop(svc, CFSTR(kIOHIDManufacturerKey), vendor, sizeof(vendor));
        long vid   = get_num_prop(svc, CFSTR(kIOHIDVendorIDKey), 0);
        long pid   = get_num_prop(svc, CFSTR(kIOHIDProductIDKey), 0);
        long usage = get_num_prop(svc, CFSTR(kIOHIDPrimaryUsageKey), 0);
        long page  = get_num_prop(svc, CFSTR(kIOHIDPrimaryUsagePageKey), 0);

        /* Only care about game controllers / wheels / joysticks, plus anything
           Logitech, so we do not spam every keyboard and trackpad. */
        int is_stick = (page == kHIDPage_GenericDesktop &&
                        (usage == kHIDUsage_GD_Joystick ||
                         usage == kHIDUsage_GD_GamePad  ||
                         usage == kHIDUsage_GD_MultiAxisController));
        int is_logi  = (vid == 0x046d);
        if (!is_stick && !is_logi) { IOObjectRelease(svc); continue; }

        g_found++;
        printf("[%d] %s%s%s\n", g_found,
               product[0] ? product : "(unnamed HID device)",
               vendor[0] ? "  —  " : "", vendor[0] ? vendor : "");
        printf("      VID:PID      : 0x%04lx:0x%04lx%s\n", vid, pid,
               is_logi ? "   (Logitech)" : "");
        printf("      usage        : page 0x%02lx / usage 0x%02lx%s\n", page, usage,
               is_stick ? "   (joystick/wheel class)" : "");

        HRESULT hr = FFIsForceFeedback(svc);
        if (hr == FF_OK) {
            g_ff_capable++;
            printf("      FFIsForceFeedback: YES — macOS considers this FF-capable\n");

            FFDeviceObjectReference dev = NULL;
            hr = FFCreateDevice(svc, &dev);
            if (hr == FF_OK && dev) {
                FFCAPABILITIES caps;
                memset(&caps, 0, sizeof(caps));
                if (FFDeviceGetForceFeedbackCapabilities(dev, &caps) == FF_OK) {
                    printf("      FF axes      : %lu\n", (unsigned long)caps.numFfAxes);
                    printf("      storage      : %lu effects, %lu playing\n",
                           (unsigned long)caps.storageCapacity,
                           (unsigned long)caps.playbackCapacity);
                    print_effect_support(caps.supportedEffects);
                }
                UInt32 state = 0;
                if (FFDeviceGetForceFeedbackState(dev, &state) == FF_OK)
                    printf("      FF state     : 0x%08lx%s\n", (unsigned long)state,
                           (state & FFGFFS_ACTUATORSON) ? " (actuators on)" : "");
                if (do_spin) spin_test(dev);
                else printf("      (re-run with --spin to actually command torque)\n");
                FFReleaseDevice(dev);
            } else {
                printf("      FFCreateDevice: FAILED (0x%lx) — capable but not openable.\n",
                       (unsigned long)hr);
                printf("      Hint: grant this terminal Input Monitoring in\n"
                       "            System Settings > Privacy & Security > Input Monitoring.\n");
            }
        } else {
            printf("      FFIsForceFeedback: NO — macOS exposes no FF interface for it\n");
            printf("                         (0x%lx; no IOKit ForceFeedback plugin claims this device)\n",
                   (unsigned long)hr);
        }
        printf("\n");
        IOObjectRelease(svc);
    }
    IOObjectRelease(iter);

    printf("-------------------------------------------\n");
    if (!g_found) {
        printf("RESULT: no wheel/joystick/Logitech HID device found.\n");
        printf("        Plug the G29 in (USB), power its brick, and re-run.\n");
        return 2;
    }
    printf("RESULT: %d controller-class device(s) found, %d force-feedback capable.\n",
           g_found, g_ff_capable);
    if (!g_ff_capable) {
        printf("        => macOS cannot drive this wheel's motor. Any \"FFB\" you see\n");
        printf("           in a Mac-native game is the game faking it or doing nothing.\n");
        return 3;
    }
    return 0;
}

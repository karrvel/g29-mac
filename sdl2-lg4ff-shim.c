/*
 * sdl2-lg4ff-shim.c — give Windows racing sims real force feedback on macOS
 *
 * The problem chain, verified on this machine:
 *
 *   - macOS has no driver for Logitech's wheels. ForceFeedback.framework only
 *     drives HID PID-class hardware; the G29 exposes usage pages 0x01/0x09/0xFF00
 *     and no 0x0F page, so FFIsForceFeedback() returns "unsupported".
 *   - SDL2's macOS haptic backend is built on ForceFeedback.framework, so SDL2
 *     reports zero haptic devices for the wheel.
 *   - Wine's winebus.sys only synthesises a DirectInput force-feedback (PID)
 *     device when its SDL backend reports the joystick is haptic. It never
 *     does, so every Windows sim sees a wheel with no FFB.
 *   - SDL3 solved this upstream (SDL_hidapi_lg4ff.c, ported from Linux
 *     new-lg4ff) but Wine here loads SDL2.
 *
 * This shim sits in the one place that fixes the whole chain. Wine dlopen()s
 * "libSDL2-2.0.0.dylib" and dlsym()s 52 functions from it. We provide that
 * library: every non-haptic symbol is an assembly trampoline into the real
 * SDL2, and the haptic surface is reimplemented on top of the Logitech vendor
 * FFB protocol written straight to the wheel as HID output reports.
 *
 * Result: game -> dinput8 -> winebus PID device -> SDL_Haptic* -> here -> wheel.
 *
 * Build (must be x86_64: Wine's winebus.so is x86_64 under Rosetta):
 *   clang -arch x86_64 -O2 -dynamiclib -o libSDL2-2.0.0.dylib \
 *         sdl2-lg4ff-shim.c -I/opt/homebrew/include \
 *         -framework IOKit -framework CoreFoundation \
 *         -install_name @rpath/libSDL2-2.0.0.dylib
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <dlfcn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDKeys.h>

#include <SDL2/SDL.h>

#define REAL_SUFFIX "libSDL2-2.0.0.real.dylib"
#define LOGITECH_VID 0x046d

static void *g_real = NULL;

static void shim_log(const char *fmt, ...) {
    static int on = -1;
    if (on < 0) on = getenv("LG4FF_SHIM_DEBUG") ? 1 : 0;
    if (!on) return;
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[lg4ff-shim] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* ------------------------------------------------------------------ *
 *  Load the real SDL2 that sits next to us                            *
 * ------------------------------------------------------------------ */

static void load_real(void) {
    Dl_info info;
    char path[1024];
    if (dladdr((void *)load_real, &info) && info.dli_fname) {
        snprintf(path, sizeof(path), "%s", info.dli_fname);
        char *slash = strrchr(path, '/');
        if (slash) { *(slash + 1) = '\0'; strncat(path, REAL_SUFFIX, sizeof(path) - strlen(path) - 1); }
        else snprintf(path, sizeof(path), "%s", REAL_SUFFIX);
    } else {
        snprintf(path, sizeof(path), "%s", REAL_SUFFIX);
    }
    g_real = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!g_real) {
        fprintf(stderr, "[lg4ff-shim] FATAL: cannot load real SDL2 at %s: %s\n", path, dlerror());
    } else {
        shim_log("loaded real SDL2 from %s", path);
    }
}

static void *real_sym(const char *name) {
    if (!g_real) load_real();
    return g_real ? dlsym(g_real, name) : NULL;
}

/* ------------------------------------------------------------------ *
 *  Trampolines for everything we do not override.                     *
 *  A naked `jmp` preserves every argument register, so one macro      *
 *  works for any signature.                                           *
 * ------------------------------------------------------------------ */

#define FWD(name)                                                        \
    void *p_##name = NULL;                                               \
    __asm__(".globl _" #name "\n"                                        \
            "_" #name ":\n"                                              \
            "  movq _p_" #name "@GOTPCREL(%rip), %rax\n"                 \
            "  movq (%rax), %rax\n"                                      \
            "  testq %rax, %rax\n"                                       \
            "  je 1f\n"                                                  \
            "  jmp *%rax\n"                                              \
            "1:\n"                                                       \
            "  xorl %eax, %eax\n"                                        \
            "  ret\n");

FWD(SDL_GameControllerAddMapping)
FWD(SDL_GameControllerClose)
FWD(SDL_GameControllerEventState)
FWD(SDL_GameControllerGetAxis)
FWD(SDL_GameControllerGetButton)
FWD(SDL_GameControllerName)
FWD(SDL_GameControllerOpen)
FWD(SDL_GetError)
FWD(SDL_GetTicks)
FWD(SDL_Init)
FWD(SDL_IsGameController)
FWD(SDL_JoystickClose)
FWD(SDL_JoystickEventState)
FWD(SDL_JoystickGetAxis)
FWD(SDL_JoystickGetGUID)
FWD(SDL_JoystickGetGUIDString)
FWD(SDL_JoystickGetHat)
FWD(SDL_JoystickGetProduct)
FWD(SDL_JoystickGetProductVersion)
FWD(SDL_JoystickGetVendor)
FWD(SDL_JoystickInstanceID)
FWD(SDL_JoystickName)
FWD(SDL_JoystickNumAxes)
FWD(SDL_JoystickNumBalls)
FWD(SDL_JoystickNumButtons)
FWD(SDL_JoystickNumHats)
FWD(SDL_JoystickOpen)
FWD(SDL_JoystickRumble)
FWD(SDL_JoystickRumbleTriggers)
FWD(SDL_PushEvent)
FWD(SDL_RegisterEvents)
FWD(SDL_SetHint)
FWD(SDL_WaitEventTimeout)

struct fwd_entry { const char *name; void **slot; };
#define FWD_ENT(name) { #name, &p_##name }

static struct fwd_entry g_fwd[] = {
    FWD_ENT(SDL_GameControllerAddMapping), FWD_ENT(SDL_GameControllerClose),
    FWD_ENT(SDL_GameControllerEventState), FWD_ENT(SDL_GameControllerGetAxis),
    FWD_ENT(SDL_GameControllerGetButton), FWD_ENT(SDL_GameControllerName),
    FWD_ENT(SDL_GameControllerOpen), FWD_ENT(SDL_GetError),
    FWD_ENT(SDL_GetTicks), FWD_ENT(SDL_Init),
    FWD_ENT(SDL_IsGameController), FWD_ENT(SDL_JoystickClose),
    FWD_ENT(SDL_JoystickEventState), FWD_ENT(SDL_JoystickGetAxis),
    FWD_ENT(SDL_JoystickGetGUID), FWD_ENT(SDL_JoystickGetGUIDString),
    FWD_ENT(SDL_JoystickGetHat), FWD_ENT(SDL_JoystickGetProduct),
    FWD_ENT(SDL_JoystickGetProductVersion),
    FWD_ENT(SDL_JoystickGetVendor), FWD_ENT(SDL_JoystickInstanceID),
    FWD_ENT(SDL_JoystickName), FWD_ENT(SDL_JoystickNumAxes),
    FWD_ENT(SDL_JoystickNumBalls), FWD_ENT(SDL_JoystickNumButtons),
    FWD_ENT(SDL_JoystickNumHats), FWD_ENT(SDL_JoystickOpen),
    FWD_ENT(SDL_JoystickRumble), FWD_ENT(SDL_JoystickRumbleTriggers),
    FWD_ENT(SDL_PushEvent), FWD_ENT(SDL_RegisterEvents),
    FWD_ENT(SDL_SetHint), FWD_ENT(SDL_WaitEventTimeout),
    { NULL, NULL }
};

/* Real versions of the haptic calls we conditionally forward. */
static Uint16 (*r_JoystickGetVendor)(SDL_Joystick *);
static Uint16 (*r_JoystickGetProduct)(SDL_Joystick *);
static int    (*r_JoystickIsHaptic)(SDL_Joystick *);
static SDL_JoystickType (*r_JoystickGetType)(SDL_Joystick *);
static SDL_Haptic *(*r_HapticOpenFromJoystick)(SDL_Joystick *);
static void   (*r_HapticClose)(SDL_Haptic *);
static unsigned int (*r_HapticQuery)(SDL_Haptic *);
static int (*r_HapticNewEffect)(SDL_Haptic *, SDL_HapticEffect *);
static int (*r_HapticUpdateEffect)(SDL_Haptic *, int, SDL_HapticEffect *);
static int (*r_HapticRunEffect)(SDL_Haptic *, int, Uint32);
static int (*r_HapticStopEffect)(SDL_Haptic *, int);
static void (*r_HapticDestroyEffect)(SDL_Haptic *, int);
static int (*r_HapticGetEffectStatus)(SDL_Haptic *, int);
static int (*r_HapticSetGain)(SDL_Haptic *, int);
static int (*r_HapticStopAll)(SDL_Haptic *);
static int (*r_HapticPause)(SDL_Haptic *);
static int (*r_HapticUnpause)(SDL_Haptic *);
static int (*r_HapticRumbleInit)(SDL_Haptic *);
static int (*r_HapticRumblePlay)(SDL_Haptic *, float, Uint32);
static int (*r_HapticRumbleStop)(SDL_Haptic *);
static int (*r_HapticRumbleSupported)(SDL_Haptic *);

__attribute__((constructor))
static void shim_init(void) {
    load_real();
    for (int i = 0; g_fwd[i].name; i++) {
        *g_fwd[i].slot = real_sym(g_fwd[i].name);
        if (!*g_fwd[i].slot) shim_log("WARNING: real SDL2 lacks %s", g_fwd[i].name);
    }
    r_JoystickGetVendor      = real_sym("SDL_JoystickGetVendor");
    r_JoystickGetProduct     = real_sym("SDL_JoystickGetProduct");
    r_JoystickIsHaptic       = real_sym("SDL_JoystickIsHaptic");
    r_JoystickGetType        = real_sym("SDL_JoystickGetType");
    r_HapticOpenFromJoystick = real_sym("SDL_HapticOpenFromJoystick");
    r_HapticClose            = real_sym("SDL_HapticClose");
    r_HapticQuery            = real_sym("SDL_HapticQuery");
    r_HapticNewEffect        = real_sym("SDL_HapticNewEffect");
    r_HapticUpdateEffect     = real_sym("SDL_HapticUpdateEffect");
    r_HapticRunEffect        = real_sym("SDL_HapticRunEffect");
    r_HapticStopEffect       = real_sym("SDL_HapticStopEffect");
    r_HapticDestroyEffect    = real_sym("SDL_HapticDestroyEffect");
    r_HapticGetEffectStatus  = real_sym("SDL_HapticGetEffectStatus");
    r_HapticSetGain          = real_sym("SDL_HapticSetGain");
    r_HapticStopAll          = real_sym("SDL_HapticStopAll");
    r_HapticPause            = real_sym("SDL_HapticPause");
    r_HapticUnpause          = real_sym("SDL_HapticUnpause");
    r_HapticRumbleInit       = real_sym("SDL_HapticRumbleInit");
    r_HapticRumblePlay       = real_sym("SDL_HapticRumblePlay");
    r_HapticRumbleStop       = real_sym("SDL_HapticRumbleStop");
    r_HapticRumbleSupported  = real_sym("SDL_HapticRumbleSupported");
    shim_log("initialised");
}

/* ------------------------------------------------------------------ *
 *  The wheel: Logitech vendor FFB protocol over raw HID output         *
 *  (command bytes transcribed from Linux new-lg4ff hid-lg4ff.c)        *
 * ------------------------------------------------------------------ */

static IOHIDDeviceRef g_wheel = NULL;
static size_t g_out_len = 16;
static int    g_wheel_searched = 0;

static int is_wheel_pid(uint32_t pid) {
    switch (pid) {
        case 0xc24f: case 0xc260: case 0xc262: case 0xc266: case 0xc267:
        case 0xc26e: case 0xc294: case 0xc295: case 0xc298: case 0xc299:
        case 0xc29a: case 0xc29b: case 0xca03: return 1;
        default: return 0;
    }
}

static long hid_num(IOHIDDeviceRef d, CFStringRef k) {
    long v = 0;
    CFTypeRef r = IOHIDDeviceGetProperty(d, k);
    if (r && CFGetTypeID(r) == CFNumberGetTypeID())
        CFNumberGetValue((CFNumberRef)r, kCFNumberLongType, &v);
    return v;
}

/* Find the wheel's joystick interface (usage page 0x01, usage 0x04). The
 * vendor-defined interface takes 20-byte reports and is not the FFB path. */
static void find_wheel(void) {
    if (g_wheel_searched) return;
    g_wheel_searched = 1;

    IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!mgr) return;
    IOHIDManagerSetDeviceMatching(mgr, NULL);
    IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);
    CFSetRef set = IOHIDManagerCopyDevices(mgr);
    if (!set) return;

    CFIndex n = CFSetGetCount(set);
    IOHIDDeviceRef *devs = calloc((size_t)n, sizeof(IOHIDDeviceRef));
    CFSetGetValues(set, (const void **)devs);

    for (CFIndex i = 0; i < n; i++) {
        if (hid_num(devs[i], CFSTR(kIOHIDVendorIDKey)) != LOGITECH_VID) continue;
        if (!is_wheel_pid((uint32_t)hid_num(devs[i], CFSTR(kIOHIDProductIDKey)))) continue;
        long up = hid_num(devs[i], CFSTR(kIOHIDPrimaryUsagePageKey));
        long u  = hid_num(devs[i], CFSTR(kIOHIDPrimaryUsageKey));
        if (up != 0x01 || (u != 0x04 && u != 0x05)) continue;
        if (IOHIDDeviceOpen(devs[i], kIOHIDOptionsTypeNone) != kIOReturnSuccess) continue;
        long omax = hid_num(devs[i], CFSTR(kIOHIDMaxOutputReportSizeKey));
        if (omax > 0 && omax <= 64) g_out_len = (size_t)omax;
        g_wheel = devs[i];
        CFRetain(g_wheel);
        shim_log("wheel opened, output report %zu bytes", g_out_len);
        break;
    }
    free(devs);
    CFRelease(set);
}

static void wheel_cmd(const uint8_t c[7]) {
    if (!g_wheel) return;
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, c, 7);
    IOHIDDeviceSetReport(g_wheel, kIOHIDReportTypeOutput, 0, buf, g_out_len);
}

/* slot 0 constant force; pct is -100..100 */
static void wheel_constant(double pct) {
    if (pct < -100) pct = -100;
    if (pct >  100) pct =  100;
    int level = (int)((32767.0 * pct) / 100.0);
    if (level >  32767) level =  32767;
    if (level < -32768) level = -32768;
    uint8_t f = (uint8_t)(((level + 0x8000) >> 8) & 0xff);
    uint8_t c[7] = { 0x11, 0x00, f, 0, 0, 0, 0 };
    wheel_cmd(c);
}

static void wheel_stop_slot0(void) {
    const uint8_t c[7] = { 0x13, 0, 0, 0, 0, 0, 0 };
    wheel_cmd(c);
}

static void wheel_autocentre(int pct) {
    if (pct <= 0) {
        const uint8_t off[7] = { 0xf5, 0, 0, 0, 0, 0, 0 };
        wheel_cmd(off);
        return;
    }
    if (pct > 100) pct = 100;
    uint32_t magnitude = (uint32_t)((65535.0 * pct) / 100.0);
    uint32_t a, b;
    if (magnitude <= 0xaaaa) { a = 0x0c * magnitude; b = 0x80 * magnitude; }
    else { a = (0x0c * 0xaaaa) + 0x06 * (magnitude - 0xaaaa);
           b = (0x80 * 0xaaaa) + 0xff * (magnitude - 0xaaaa); }
    a >>= 1;
    uint8_t set[7] = { 0xfe, 0x0d, (uint8_t)(a / 0xaaaa), (uint8_t)(a / 0xaaaa),
                       (uint8_t)(b / 0xaaaa), 0, 0 };
    const uint8_t go[7] = { 0x14, 0, 0, 0, 0, 0, 0 };
    wheel_cmd(set);
    wheel_cmd(go);
}

/* ------------------------------------------------------------------ *
 *  Our fake SDL_Haptic                                                *
 * ------------------------------------------------------------------ */

#define SHIM_MAGIC 0x4C473446u   /* 'LG4F' */
#define MAX_EFFECTS 16

struct shim_haptic {
    uint32_t magic;
    int gain;                       /* 0..100 */
    struct {
        int used, running;
        SDL_HapticEffect eff;
    } fx[MAX_EFFECTS];
};

static struct shim_haptic *g_haptic = NULL;

static int is_ours(SDL_Haptic *h) {
    return h && ((struct shim_haptic *)h)->magic == SHIM_MAGIC;
}

static int joystick_is_our_wheel(SDL_Joystick *j) {
    if (!j || !r_JoystickGetVendor || !r_JoystickGetProduct) return 0;
    if (r_JoystickGetVendor(j) != LOGITECH_VID) return 0;
    return is_wheel_pid(r_JoystickGetProduct(j));
}

/* Direction -> signed multiplier. dinput sends polar (9000 = right) for wheels,
 * cartesian for some titles. */
static double direction_sign(const SDL_HapticDirection *d) {
    if (!d) return 1.0;
    switch (d->type) {
        case SDL_HAPTIC_POLAR: {
            double deg = (double)d->dir[0] / 100.0;
            double s = sin(deg * M_PI / 180.0);
            if (s > 0.001) return 1.0;
            if (s < -0.001) return -1.0;
            return 1.0;
        }
        case SDL_HAPTIC_CARTESIAN:
            return (d->dir[0] < 0) ? -1.0 : 1.0;
        case SDL_HAPTIC_SPHERICAL:
            return (d->dir[0] < 0) ? -1.0 : 1.0;
        default:
            return 1.0;
    }
}

static void apply_effect(struct shim_haptic *sh, int id) {
    if (id < 0 || id >= MAX_EFFECTS || !sh->fx[id].used) return;
    SDL_HapticEffect *e = &sh->fx[id].eff;
    double gain = sh->gain / 100.0;

    switch (e->type) {
        case SDL_HAPTIC_CONSTANT: {
            double lvl = (double)e->constant.level / 32767.0 * 100.0;
            lvl *= direction_sign(&e->constant.direction) * gain;
            wheel_constant(lvl);
            break;
        }
        case SDL_HAPTIC_SINE:
        case SDL_HAPTIC_TRIANGLE:
        case SDL_HAPTIC_SAWTOOTHUP:
        case SDL_HAPTIC_SAWTOOTHDOWN: {
            /* No periodic engine here: hold the peak magnitude so kerb and
             * road-texture effects are at least felt. */
            double lvl = (double)e->periodic.magnitude / 32767.0 * 100.0;
            lvl *= direction_sign(&e->periodic.direction) * gain;
            wheel_constant(lvl);
            break;
        }
        case SDL_HAPTIC_SPRING: {
            int strength = (int)((double)e->condition.right_coeff[0] / 32767.0 * 100.0 * gain);
            if (strength < 0) strength = -strength;
            wheel_autocentre(strength);
            break;
        }
        case SDL_HAPTIC_RAMP: {
            double lvl = (double)e->ramp.start / 32767.0 * 100.0;
            lvl *= direction_sign(&e->ramp.direction) * gain;
            wheel_constant(lvl);
            break;
        }
        default:
            break;
    }
}

/* ------------------------------------------------------------------ *
 *  Overridden SDL haptic entry points                                 *
 * ------------------------------------------------------------------ */

/* Wine's winebus sets desc.is_wheel from this, and for a wheel it builds the
 * report descriptor with Simulation Controls axes (Steering 0xC8, Accelerator
 * 0xC4, Brake 0xC5, Clutch 0xC6) instead of Generic Desktop X/Y/Z/Rx/Ry/Rz.
 * Older sims — Live for Speed among them — only bind Generic Desktop axes, so
 * on a "proper" wheel descriptor their pedals cannot be assigned at all.
 * Reporting UNKNOWN gets the plain joystick descriptor back. Haptics are
 * unaffected: SDL_JoystickIsHaptic below still claims the device.
 * Set LG4FF_SHIM_WHEEL_TYPE=wheel to restore the wheel descriptor. */
SDL_JoystickType SDL_JoystickGetType(SDL_Joystick *j) {
    if (joystick_is_our_wheel(j)) {
        const char *e = getenv("LG4FF_SHIM_WHEEL_TYPE");
        if (!(e && !strcmp(e, "wheel"))) {
            shim_log("SDL_JoystickGetType -> UNKNOWN (generic-desktop axes)");
            return SDL_JOYSTICK_TYPE_UNKNOWN;
        }
    }
    return r_JoystickGetType ? r_JoystickGetType(j) : SDL_JOYSTICK_TYPE_UNKNOWN;
}

int SDL_JoystickIsHaptic(SDL_Joystick *j) {
    if (joystick_is_our_wheel(j)) {
        find_wheel();
        if (g_wheel) { shim_log("SDL_JoystickIsHaptic -> YES (wheel)"); return 1; }
    }
    return r_JoystickIsHaptic ? r_JoystickIsHaptic(j) : 0;
}

SDL_Haptic *SDL_HapticOpenFromJoystick(SDL_Joystick *j) {
    if (joystick_is_our_wheel(j)) {
        find_wheel();
        if (g_wheel) {
            if (!g_haptic) {
                g_haptic = calloc(1, sizeof(*g_haptic));
                g_haptic->magic = SHIM_MAGIC;
                g_haptic->gain = 100;
            }
            shim_log("SDL_HapticOpenFromJoystick -> shim device");
            return (SDL_Haptic *)g_haptic;
        }
    }
    return r_HapticOpenFromJoystick ? r_HapticOpenFromJoystick(j) : NULL;
}

void SDL_HapticClose(SDL_Haptic *h) {
    if (is_ours(h)) { wheel_stop_slot0(); wheel_autocentre(0); return; }
    if (r_HapticClose) r_HapticClose(h);
}

unsigned int SDL_HapticQuery(SDL_Haptic *h) {
    if (is_ours(h))
        return SDL_HAPTIC_CONSTANT | SDL_HAPTIC_SINE | SDL_HAPTIC_TRIANGLE |
               SDL_HAPTIC_SAWTOOTHUP | SDL_HAPTIC_SAWTOOTHDOWN | SDL_HAPTIC_RAMP |
               SDL_HAPTIC_SPRING | SDL_HAPTIC_GAIN | SDL_HAPTIC_STATUS;
    return r_HapticQuery ? r_HapticQuery(h) : 0;
}

int SDL_HapticNewEffect(SDL_Haptic *h, SDL_HapticEffect *e) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        for (int i = 0; i < MAX_EFFECTS; i++) {
            if (!sh->fx[i].used) {
                sh->fx[i].used = 1;
                sh->fx[i].running = 0;
                if (e) sh->fx[i].eff = *e;
                shim_log("new effect %d type 0x%x", i, e ? e->type : 0);
                return i;
            }
        }
        return -1;
    }
    return r_HapticNewEffect ? r_HapticNewEffect(h, e) : -1;
}

int SDL_HapticUpdateEffect(SDL_Haptic *h, int id, SDL_HapticEffect *e) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        if (id < 0 || id >= MAX_EFFECTS || !sh->fx[id].used) return -1;
        if (e) sh->fx[id].eff = *e;
        if (sh->fx[id].running) apply_effect(sh, id);
        return 0;
    }
    return r_HapticUpdateEffect ? r_HapticUpdateEffect(h, id, e) : -1;
}

int SDL_HapticRunEffect(SDL_Haptic *h, int id, Uint32 iterations) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        if (id < 0 || id >= MAX_EFFECTS || !sh->fx[id].used) return -1;
        sh->fx[id].running = 1;
        apply_effect(sh, id);
        return 0;
    }
    return r_HapticRunEffect ? r_HapticRunEffect(h, id, iterations) : -1;
}

int SDL_HapticStopEffect(SDL_Haptic *h, int id) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        if (id < 0 || id >= MAX_EFFECTS) return -1;
        sh->fx[id].running = 0;
        if (sh->fx[id].eff.type == SDL_HAPTIC_SPRING) wheel_autocentre(0);
        else wheel_constant(0);
        return 0;
    }
    return r_HapticStopEffect ? r_HapticStopEffect(h, id) : -1;
}

void SDL_HapticDestroyEffect(SDL_Haptic *h, int id) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        if (id < 0 || id >= MAX_EFFECTS) return;
        if (sh->fx[id].running) wheel_constant(0);
        sh->fx[id].used = 0;
        sh->fx[id].running = 0;
        return;
    }
    if (r_HapticDestroyEffect) r_HapticDestroyEffect(h, id);
}

int SDL_HapticGetEffectStatus(SDL_Haptic *h, int id) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        if (id < 0 || id >= MAX_EFFECTS || !sh->fx[id].used) return -1;
        return sh->fx[id].running ? 1 : 0;
    }
    return r_HapticGetEffectStatus ? r_HapticGetEffectStatus(h, id) : -1;
}

int SDL_HapticSetGain(SDL_Haptic *h, int gain) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        if (gain < 0) gain = 0;
        if (gain > 100) gain = 100;
        sh->gain = gain;
        for (int i = 0; i < MAX_EFFECTS; i++)
            if (sh->fx[i].used && sh->fx[i].running) apply_effect(sh, i);
        return 0;
    }
    return r_HapticSetGain ? r_HapticSetGain(h, gain) : -1;
}

int SDL_HapticStopAll(SDL_Haptic *h) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        for (int i = 0; i < MAX_EFFECTS; i++) sh->fx[i].running = 0;
        wheel_constant(0);
        wheel_stop_slot0();
        wheel_autocentre(0);
        return 0;
    }
    return r_HapticStopAll ? r_HapticStopAll(h) : -1;
}

int SDL_HapticPause(SDL_Haptic *h) {
    if (is_ours(h)) { wheel_constant(0); return 0; }
    return r_HapticPause ? r_HapticPause(h) : -1;
}

int SDL_HapticUnpause(SDL_Haptic *h) {
    if (is_ours(h)) {
        struct shim_haptic *sh = (struct shim_haptic *)h;
        for (int i = 0; i < MAX_EFFECTS; i++)
            if (sh->fx[i].used && sh->fx[i].running) apply_effect(sh, i);
        return 0;
    }
    return r_HapticUnpause ? r_HapticUnpause(h) : -1;
}

int SDL_HapticRumbleSupported(SDL_Haptic *h) {
    if (is_ours(h)) return 1;
    return r_HapticRumbleSupported ? r_HapticRumbleSupported(h) : 0;
}

int SDL_HapticRumbleInit(SDL_Haptic *h) {
    if (is_ours(h)) return 0;
    return r_HapticRumbleInit ? r_HapticRumbleInit(h) : -1;
}

int SDL_HapticRumblePlay(SDL_Haptic *h, float strength, Uint32 length) {
    if (is_ours(h)) { wheel_constant(strength * 60.0); return 0; }
    return r_HapticRumblePlay ? r_HapticRumblePlay(h, strength, length) : -1;
}

int SDL_HapticRumbleStop(SDL_Haptic *h) {
    if (is_ours(h)) { wheel_constant(0); return 0; }
    return r_HapticRumbleStop ? r_HapticRumbleStop(h) : -1;
}

// g29probe.c - Does SDL3 reach the Logitech G29 force-feedback motor on macOS?
//
// Build:  clang g29probe.c -o g29probe $(pkg-config --cflags --libs sdl3)
// Run:    ./g29probe
//
// PRECONDITIONS:
//   1. SDL >= 3.4.0 (the lg4ff hidapi driver landed in 3.4.0, 2026-01-01).
//   2. G29 connected by USB.
//   3. The wheel's console selector switch set to PS3. SDL's driver is
//      "G29 (PS3)"; PS4 mode reports a different descriptor and is not driven.
//   4. Grant Input Monitoring to the terminal/binary in
//      System Settings > Privacy & Security. Without it macOS returns zero
//      HID devices and this prints "Joysticks found: 0".
//
// NOTE: the lg4ff hidapi driver is ALREADY enabled by default on macOS
// (it is only default-off on Windows, where hid.dll cannot send 7-byte
// reports). The explicit hints below are belt-and-braces, not required.
#include <SDL3/SDL.h>
#include <stdio.h>

int main(void) {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_LG4FF, "1");

    if (!SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL runtime version: %d.%d.%d\n",
           SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
           SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
           SDL_VERSIONNUM_MICRO(SDL_GetVersion()));

    SDL_Delay(1500); // let hidapi enumerate + finish the G29 mode switch

    int n = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&n);
    printf("Joysticks found: %d\n", n);
    if (n == 0)
        printf("  -> nothing detected. Check System Settings > Privacy & Security >\n"
               "     Input Monitoring, and that the wheel is USB-connected.\n");

    for (int i = 0; i < n; i++) {
        SDL_Joystick *j = SDL_OpenJoystick(ids[i]);
        if (!j) { printf("[%d] open failed: %s\n", i, SDL_GetError()); continue; }
        printf("\n[%d] %s  VID=%04x PID=%04x  axes=%d buttons=%d\n", i,
               SDL_GetJoystickName(j), SDL_GetJoystickVendor(j),
               SDL_GetJoystickProduct(j), SDL_GetNumJoystickAxes(j),
               SDL_GetNumJoystickButtons(j));
        printf("    SDL_IsJoystickHaptic: %s\n",
               SDL_IsJoystickHaptic(j) ? "YES" : "no");

        SDL_Haptic *h = SDL_OpenHapticFromJoystick(j);
        if (!h) { printf("    haptic open FAILED: %s\n", SDL_GetError()); SDL_CloseJoystick(j); continue; }

        printf("    haptic name : %s\n", SDL_GetHapticName(h));
        printf("    haptic feats: 0x%08x   axes=%d  max effects=%d\n",
               SDL_GetHapticFeatures(h), SDL_GetNumHapticAxes(h),
               SDL_GetMaxHapticEffects(h));
        printf("    CONSTANT=%d SPRING=%d DAMPER=%d FRICTION=%d SINE=%d AUTOCENTER=%d GAIN=%d\n",
               !!(SDL_GetHapticFeatures(h) & SDL_HAPTIC_CONSTANT),
               !!(SDL_GetHapticFeatures(h) & SDL_HAPTIC_SPRING),
               !!(SDL_GetHapticFeatures(h) & SDL_HAPTIC_DAMPER),
               !!(SDL_GetHapticFeatures(h) & SDL_HAPTIC_FRICTION),
               !!(SDL_GetHapticFeatures(h) & SDL_HAPTIC_SINE),
               !!(SDL_GetHapticFeatures(h) & SDL_HAPTIC_AUTOCENTER),
               !!(SDL_GetHapticFeatures(h) & SDL_HAPTIC_GAIN));

        if (SDL_GetHapticFeatures(h) & SDL_HAPTIC_CONSTANT) {
            SDL_HapticEffect e;
            SDL_zero(e);
            e.type = SDL_HAPTIC_CONSTANT;
            e.constant.direction.type = SDL_HAPTIC_CARTESIAN;
            e.constant.direction.dir[0] = 1;
            e.constant.length = 1200;
            e.constant.level = 0x5000;      // ~60% torque, one direction
            int id = SDL_CreateHapticEffect(h, &e);
            if (id < 0) {
                printf("    >> CreateHapticEffect FAILED: %s\n", SDL_GetError());
            } else {
                printf("    >> RUNNING constant force 1.2s - THE WHEEL SHOULD PULL NOW\n");
                if (!SDL_RunHapticEffect(h, id, 1)) printf("    >> Run failed: %s\n", SDL_GetError());
                SDL_Delay(1500);
                SDL_DestroyHapticEffect(h, id);
                printf("    >> done. Did it move? That is your answer.\n");
            }
        }
        SDL_CloseHaptic(h);
        SDL_CloseJoystick(j);
    }
    SDL_free(ids);
    SDL_Quit();
    return 0;
}

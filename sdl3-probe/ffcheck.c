// ffcheck.c - Does Apple's ForceFeedback.framework see ANY device?
// SDL's darwin haptic backend enumerates only devices where FFIsForceFeedback() succeeds.
#include <SDL3/SDL.h>
#include <stdio.h>
int main(void){
    SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_HAPTIC);
    SDL_Delay(1200);
    int n=0; SDL_HapticID *h = SDL_GetHaptics(&n);
    printf("ForceFeedback.framework (SDL darwin backend) haptic devices: %d\n", n);
    for(int i=0;i<n;i++) printf("   [%d] %s\n", i, SDL_GetHapticNameForID(h[i]));
    if(n==0) printf("   -> ForceFeedback.framework enumerates NOTHING (no FF IOKit plugin installed)\n");
    SDL_free(h); SDL_Quit(); return 0;
}

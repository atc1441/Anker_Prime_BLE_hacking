#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

static uint32_t steps;
static int volume;

void snd_init(void) { steps = 0; }
void snd_tick(void) { steps++; }
void snd_volume(int vol) { volume = vol; }
void snd_play(const uint8_t *tones, uint32_t n)
{
    (void)tones;
    (void)n;
}
void snd_stop(void) {}
bool snd_busy(void) { return false; }
void snd_tone(uint32_t hz) { (void)hz; }
uint32_t snd_steps(void) { return steps; }

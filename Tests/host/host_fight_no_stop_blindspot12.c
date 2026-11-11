#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Car.h"
#include "Fight.h"

static uint32_t g_tick;
static int16_t g_left;
static int16_t g_right;

uint32_t HAL_GetTick(void) { return g_tick; }
GPIO_PinState HAL_GPIO_ReadPin(void *p, uint16_t pin)
{ (void)p; (void)pin; return GPIO_PIN_RESET; }
void Car_ApplyFightCommand(const FightMotionCommand *c)
{
  if (c == NULL) { g_left = 0; g_right = 0; return; }
  g_left = c->left_pwm; g_right = c->right_pwm;
}

static void fail(const char *m)
{
  fprintf(stderr,"FAIL at %lu: %s state=%u cmd=%u pwm=%d/%d fault=%u\n",
          (unsigned long)g_tick,m,(unsigned)Fight_GetState(),
          (unsigned)Fight_GetMotionCommand()->id,(int)g_left,(int)g_right,
          (unsigned)g_fight_debug.edge_timeout_fault);
  exit(1);
}

int main(void)
{
  SensorSnapshot s;
  memset(&s,0,sizeof(s));
  g_tick=0U;
  Fight_Enter();
  for (g_tick=5U; g_tick<=5000U; g_tick+=5U)
  {
    memset(&s,0,sizeof(s));
    s.tick_ms=g_tick;
    s.snapshot_count=g_tick/5U;
    s.edge_raw_mask=SENSOR_EDGE_FRONT_LEFT|SENSOR_EDGE_FRONT_RIGHT;
    s.adc1_valid=1U;
    s.adc2_valid=1U;
    Fight_Update(&s);
    if ((g_left==0) && (g_right==0)) fail("persistent Edge latched STOP");
  }
  puts("Blindspot12 Fight persistent-Edge no-STOP regression PASSED.");
  return 0;
}

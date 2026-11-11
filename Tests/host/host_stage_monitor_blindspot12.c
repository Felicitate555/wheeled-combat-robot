#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "StageMonitor.h"

static uint32_t g_tick;
static uint32_t g_seq;
static FightState g_state = FIGHT_SEARCH;

uint32_t HAL_GetTick(void) { return g_tick; }
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin)
{
  (void)port; (void)pin; return GPIO_PIN_RESET;
}
FightState Fight_GetState(void) { return g_state; }

static void fail(const char *msg)
{
  fprintf(stderr, "FAIL at %lu: %s (state=%u reason=%u)\n",
          (unsigned long)g_tick, msg,
          (unsigned)StageMonitor_GetState(),
          (unsigned)StageMonitor_GetExitEvent()->confidence_reason);
  exit(1);
}

static void feed(uint32_t t, uint8_t edge, uint32_t r5, uint8_t valid)
{
  SensorSnapshot s;
  FightMotionCommand m;
  memset(&s, 0, sizeof(s));
  s.tick_ms = t;
  s.snapshot_count = ++g_seq;
  s.edge_raw_mask = edge;
  s.range5_raw = r5;
  s.adc2_valid = valid;
  m.id = FIGHT_CMD_SEARCH_ARC;
  m.left_pwm = 38;
  m.right_pwm = 24;
  g_tick = t;
  g_state = (edge != 0U) ? FIGHT_EDGE_ESCAPE : FIGHT_SEARCH;
  StageMonitor_Update(&s, &m);
}

static void reset_monitor(uint8_t valid)
{
  SensorSnapshot s;
  memset(&s, 0, sizeof(s));
  g_seq = 0U;
  g_tick = 0U;
  s.tick_ms = 0U;
  s.snapshot_count = ++g_seq;
  s.range5_raw = 30000U;
  s.adc2_valid = valid;
  StageMonitor_EnterOnStage(&s);
}

static void arm(void)
{
  uint32_t t;
  reset_monitor(1U);
  for (t = 5U; t <= 105U; t += 5U)
  {
    feed(t, 0U, 30000U, 1U);
  }
  if (StageMonitor_GetState() != STAGE_MONITOR_ARMED) fail("did not arm");
}

static void test_invalid_cannot_arm(void)
{
  uint32_t t;
  reset_monitor(0U);
  for (t = 5U; t <= 200U; t += 5U) feed(t, 0U, 30000U, 0U);
  if (StageMonitor_GetState() != STAGE_MONITOR_ARMING) fail("stale ADC2 armed monitor");
  for (; t <= 305U; t += 5U) feed(t, 0U, 30000U, 1U);
  if (StageMonitor_GetState() != STAGE_MONITOR_ARMED) fail("fresh ADC2 did not arm");
}

static void test_short_off_returns_fight(void)
{
  uint32_t t;
  arm();
  for (t = 110U; t <= 305U; t += 5U) feed(t, 0U, 50000U, 1U);
  feed(310U, 0U, 30000U, 1U);
  if (StageMonitor_IsStageLostConfirmed() != 0U) fail("<300ms became lost");
  if (StageMonitor_GetState() != STAGE_MONITOR_ARMED) fail("<300ms did not return ARMED");
}

static void test_medium_off_requests_verify(void)
{
  uint32_t t;
  arm();
  for (t = 110U; t <= 610U; t += 5U) feed(t, 0U, 50000U, 1U);
  feed(615U, 0U, 30000U, 1U);
  if (StageMonitor_IsStageLostConfirmed() == 0U) fail("300-1500ms did not exit Fight");
  if (StageMonitor_GetExitEvent()->confidence_reason !=
      STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN) fail("medium interval wrong reason");
}

static void test_hard_off_1500(void)
{
  uint32_t t;
  arm();
  for (t = 110U; t <= 1615U; t += 5U) feed(t, 0U, 50000U, 1U);
  if (StageMonitor_IsStageLostConfirmed() == 0U) fail(">=1500ms did not hard-confirm");
  if (StageMonitor_GetExitEvent()->confidence_reason !=
      STAGE_CONFIDENCE_RANGE5_HARD_LOST) fail("hard interval wrong reason");
}

static void test_edge_only_never_confirms(void)
{
  uint32_t t;
  arm();
  for (t = 110U; t <= 300U; t += 5U)
    feed(t, SENSOR_EDGE_FRONT_LEFT, 30000U, 1U);
  for (; t <= 380U; t += 5U) feed(t, 0U, 30000U, 1U);
  if (StageMonitor_IsStageLostConfirmed() != 0U) fail("Edge-only confirmed loss");
  if (StageMonitor_GetState() != STAGE_MONITOR_ARMED) fail("Edge-only candidate did not dismiss");
}


static void test_invalid_gap_breaks_hard_off_continuity(void)
{
  uint32_t t;
  arm();

  /* Observe OFF for ~500ms, then lose ADC2 validity for >1500ms. */
  for (t = 110U; t <= 610U; t += 5U) feed(t, 0U, 50000U, 1U);
  for (; t <= 2210U; t += 5U) feed(t, 0U, 50000U, 0U);

  /* One fresh OFF sample after the stale gap must start a NEW window, not
     inherit the old timer and instantly hard-confirm stage loss. */
  feed(2215U, 0U, 50000U, 1U);
  if (StageMonitor_IsStageLostConfirmed() != 0U)
    fail("invalid ADC2 gap did not break 1500ms OFF continuity");

  /* Only a new 1500ms run of fresh OFF samples may hard-confirm. */
  for (t = 2220U; t <= 3720U; t += 5U) feed(t, 0U, 50000U, 1U);
  if (StageMonitor_IsStageLostConfirmed() == 0U)
    fail("fresh 1500ms OFF after invalid gap did not hard-confirm");
}

int main(void)
{
  test_invalid_cannot_arm();
  test_short_off_returns_fight();
  test_medium_off_requests_verify();
  test_hard_off_1500();
  test_edge_only_never_confirms();
  test_invalid_gap_breaks_hard_off_continuity();
  puts("Blindspot12 StageMonitor regression PASSED.");
  return 0;
}

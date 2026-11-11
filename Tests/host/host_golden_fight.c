#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Fight.h"

static uint32_t g_host_tick;
static unsigned int g_checks;

void Golden_Fight_Enter(void);
void Golden_Fight_Exit(void);
void Golden_Fight_Update(const SensorSnapshot *snapshot);
const FightMotionCommand *Golden_Fight_GetMotionCommand(void);
FightState Golden_Fight_GetState(void);
TargetRegion Golden_Fight_GetTargetRegion(void);

void Final_Fight_Enter(void);
void Final_Fight_Exit(void);
void Final_Fight_Update(const SensorSnapshot *snapshot);
const FightMotionCommand *Final_Fight_GetMotionCommand(void);
FightState Final_Fight_GetState(void);
TargetRegion Final_Fight_GetTargetRegion(void);

uint32_t HAL_GetTick(void)
{
  return g_host_tick;
}

GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin)
{
  (void)port;
  (void)pin;
  return GPIO_PIN_RESET;
}

void Golden_Car_ApplyFightCommand(const FightMotionCommand *command)
{
  (void)command;
}

void Final_Car_ApplyFightCommand(const FightMotionCommand *command)
{
  (void)command;
}

static void Fail(const char *message, uint32_t tick_ms)
{
  fprintf(stderr, "Golden Fight regression FAILED at %lu ms: %s\n",
          (unsigned long)tick_ms,
          message);
  exit(1);
}

static SensorSnapshot MakeSnapshot(uint32_t tick_ms,
                                   uint32_t snapshot_count)
{
  SensorSnapshot snapshot;

  memset(&snapshot, 0, sizeof(snapshot));
  snapshot.tick_ms = tick_ms;
  snapshot.snapshot_count = snapshot_count;
  snapshot.range5_raw = 50000U;
  snapshot.adc1_valid = 1U;
  snapshot.adc2_valid = 1U;
  return snapshot;
}

static void ResetBoth(void)
{
  g_host_tick = 0U;
  Golden_Fight_Enter();
  Final_Fight_Enter();
}

static void UpdateBoth(const SensorSnapshot *snapshot)
{
  g_host_tick = snapshot->tick_ms;
  Golden_Fight_Update(snapshot);
  Final_Fight_Update(snapshot);
}

static uint8_t IsAttackCommand(FightCommandId id)
{
  return ((id == FIGHT_CMD_ATTACK_FORWARD) ||
          (id == FIGHT_CMD_ATTACK_ARC_LEFT) ||
          (id == FIGHT_CMD_ATTACK_ARC_RIGHT)) ? 1U : 0U;
}

static void CompareNormalFight(uint32_t tick_ms, uint8_t allow_attack_pwm)
{
  const FightMotionCommand *golden = Golden_Fight_GetMotionCommand();
  const FightMotionCommand *final = Final_Fight_GetMotionCommand();

  g_checks++;
  if (Golden_Fight_GetState() != Final_Fight_GetState())
  {
    Fail("Fight state differs from feibiao1", tick_ms);
  }
  if (Golden_Fight_GetTargetRegion() != Final_Fight_GetTargetRegion())
  {
    Fail("target region differs from feibiao1", tick_ms);
  }
  if (golden->id != final->id)
  {
    Fail("command id differs from feibiao1", tick_ms);
  }
  if ((allow_attack_pwm != 0U) && (IsAttackCommand(final->id) != 0U))
  {
    if ((final->id == FIGHT_CMD_ATTACK_FORWARD) &&
        ((final->left_pwm != 75) || (final->right_pwm != 75)))
    {
      Fail("final FRONT attack is not 75/75", tick_ms);
    }
    return;
  }
  if ((golden->left_pwm != final->left_pwm) ||
      (golden->right_pwm != final->right_pwm))
  {
    Fail("signed wheel command differs from feibiao1", tick_ms);
  }
}

static void TestFiveSecondSearch(void)
{
  uint32_t tick_ms;
  uint32_t sequence = 1U;

  ResetBoth();
  for (tick_ms = 5U; tick_ms <= 5000U; tick_ms += 5U)
  {
    SensorSnapshot snapshot = MakeSnapshot(tick_ms, sequence++);
    UpdateBoth(&snapshot);
    CompareNormalFight(tick_ms, 0U);
    if (Final_Fight_GetMotionCommand()->id == FIGHT_CMD_STOP)
    {
      Fail("five-second empty Search stopped unexpectedly", tick_ms);
    }
  }
}

static void TestEdgeCombination(uint8_t edge_mask)
{
  uint32_t tick_ms;
  uint32_t sequence = 1U;
  SensorSnapshot snapshot;

  ResetBoth();
  snapshot = MakeSnapshot(5U, sequence++);
  UpdateBoth(&snapshot);
  CompareNormalFight(5U, 0U);

  snapshot = MakeSnapshot(10U, sequence++);
  snapshot.edge_raw_mask = edge_mask;
  UpdateBoth(&snapshot);
  CompareNormalFight(10U, 0U);
  if (Final_Fight_GetState() != FIGHT_EDGE_ESCAPE)
  {
    Fail("raw Edge did not preempt in the first control period", 10U);
  }

  for (tick_ms = 15U; tick_ms <= 300U; tick_ms += 5U)
  {
    snapshot = MakeSnapshot(tick_ms, sequence++);
    UpdateBoth(&snapshot);
    CompareNormalFight(tick_ms, 0U);
  }
}

typedef struct
{
  TargetRegion expected;
  uint16_t enemy_mask;
  uint8_t analog_mask;
} TargetCase;

static void TestTargetCase(const TargetCase *target_case)
{
  uint32_t sequence = 1U;
  uint32_t tick_ms;

  ResetBoth();
  for (tick_ms = 5U; tick_ms <= 25U; tick_ms += 5U)
  {
    SensorSnapshot snapshot = MakeSnapshot(tick_ms, sequence++);
    snapshot.enemy_filtered_mask = target_case->enemy_mask;
    snapshot.analog_target_mask = target_case->analog_mask;
    UpdateBoth(&snapshot);
    CompareNormalFight(tick_ms,
                       (target_case->expected == TARGET_FRONT) ? 1U : 0U);
  }
  if (Final_Fight_GetTargetRegion() != target_case->expected)
  {
    Fail("target fusion region differs from expected baseline region", 25U);
  }
}

static void TestFrontAttackHoldAndEdge(void)
{
  uint32_t sequence = 1U;
  uint32_t tick_ms;
  SensorSnapshot snapshot;

  ResetBoth();
  for (tick_ms = 5U; tick_ms <= 20U; tick_ms += 5U)
  {
    snapshot = MakeSnapshot(tick_ms, sequence++);
    snapshot.analog_target_mask = SENSOR_ANALOG_A11 | SENSOR_ANALOG_A1;
    UpdateBoth(&snapshot);
    CompareNormalFight(tick_ms, 1U);
  }
  if ((Final_Fight_GetState() != FIGHT_ATTACK) ||
      (Final_Fight_GetMotionCommand()->left_pwm != 75) ||
      (Final_Fight_GetMotionCommand()->right_pwm != 75))
  {
    Fail("confirmed FRONT target did not produce 75/75 attack", 20U);
  }

  snapshot = MakeSnapshot(25U, sequence++);
  UpdateBoth(&snapshot);
  CompareNormalFight(25U, 0U);
  if ((Final_Fight_GetState() != FIGHT_TRACK) ||
      (Final_Fight_GetMotionCommand()->left_pwm != 45) ||
      (Final_Fight_GetMotionCommand()->right_pwm != 45))
  {
    Fail("Target Hold no longer matches feibiao1", 25U);
  }

  snapshot = MakeSnapshot(30U, sequence++);
  snapshot.edge_raw_mask = SENSOR_EDGE_FRONT_LEFT;
  UpdateBoth(&snapshot);
  CompareNormalFight(30U, 0U);
  if (Final_Fight_GetState() != FIGHT_EDGE_ESCAPE)
  {
    Fail("Edge did not preempt target handling", 30U);
  }
}

int main(void)
{
  static const uint8_t edge_cases[] =
  {
    SENSOR_EDGE_FRONT_LEFT,
    SENSOR_EDGE_FRONT_RIGHT,
    SENSOR_EDGE_REAR_LEFT,
    SENSOR_EDGE_REAR_RIGHT,
    SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_FRONT_RIGHT,
    SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT,
    SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_REAR_LEFT,
    SENSOR_EDGE_FRONT_RIGHT | SENSOR_EDGE_REAR_RIGHT
  };
  static const TargetCase target_cases[] =
  {
    {TARGET_FRONT, 0U, SENSOR_ANALOG_A11 | SENSOR_ANALOG_A1},
    {TARGET_FRONT_LEFT, 0U, SENSOR_ANALOG_A11},
    {TARGET_LEFT, 0U, SENSOR_ANALOG_A9},
    {TARGET_REAR_LEFT, SENSOR_ENEMY_D7 | SENSOR_ENEMY_D8, 0U},
    {TARGET_REAR, SENSOR_ENEMY_D5 | SENSOR_ENEMY_D7, 0U},
    {TARGET_REAR_RIGHT, SENSOR_ENEMY_D4 | SENSOR_ENEMY_D5, 0U},
    {TARGET_RIGHT, 0U, SENSOR_ANALOG_A3},
    {TARGET_FRONT_RIGHT, 0U, SENSOR_ANALOG_A1}
  };
  size_t index;

  TestFiveSecondSearch();
  for (index = 0U; index < (sizeof(edge_cases) / sizeof(edge_cases[0])); index++)
  {
    TestEdgeCombination(edge_cases[index]);
  }
  for (index = 0U; index < (sizeof(target_cases) / sizeof(target_cases[0])); index++)
  {
    TestTargetCase(&target_cases[index]);
  }
  TestFrontAttackHoldAndEdge();

  printf("Golden Fight regression PASSED (%u cycle comparisons).\n", g_checks);
  return 0;
}

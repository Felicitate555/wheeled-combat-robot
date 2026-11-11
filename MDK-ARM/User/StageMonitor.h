#ifndef STAGE_MONITOR_H__
#define STAGE_MONITOR_H__

#include "Fight.h"
#include "Sensor.h"
#include "fight_config.h"

#if (STAGE_HISTORY_SAMPLE_COUNT < 1U)
#error "STAGE_HISTORY_SAMPLE_COUNT must be positive"
#endif

typedef enum
{
  STAGE_EXIT_UNKNOWN = 0,
  STAGE_EXIT_FRONT,
  STAGE_EXIT_REAR,
  STAGE_EXIT_LEFT,
  STAGE_EXIT_RIGHT,
  STAGE_EXIT_SINGLE_FL,
  STAGE_EXIT_SINGLE_FR,
  STAGE_EXIT_SINGLE_RL,
  STAGE_EXIT_SINGLE_RR
} StageExitDirection;

typedef enum
{
  STAGE_MONITOR_OFF = 0,
  STAGE_MONITOR_ARMING,
  STAGE_MONITOR_ARMED,
  STAGE_MONITOR_EXIT_CANDIDATE,
  STAGE_MONITOR_LOST_CONFIRMED
} StageMonitorState;

typedef enum
{
  STAGE_CONFIDENCE_NONE = 0,

  /* 保留旧值，方便旧调试字段兼容；不再作为最终掉台依据。 */
  STAGE_CONFIDENCE_EDGE_PROPAGATION,
  STAGE_CONFIDENCE_EDGE_UNION,
  STAGE_CONFIDENCE_RANGE5_AND_MOTION,

  /* 新逻辑 */
  STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN,
  STAGE_CONFIDENCE_RANGE5_HARD_LOST
} StageConfidenceReason;
typedef struct
{
  uint32_t tick_ms;
  uint32_t range5_raw;
  uint8_t edge_raw_mask;
  int16_t left_pwm;
  int16_t right_pwm;
  FightState fight_state;
} StageHistorySample;

typedef struct
{
  uint8_t valid;
  StageExitDirection direction;
  uint8_t first_edge;
  uint8_t coalesced_first_edge;
  uint8_t edge_union;
  uint8_t last_edge;
  FightMotionCommand pre_exit_motion;
  uint32_t exit_time;
  uint8_t range5_transition_seen;
  uint8_t edge_propagation_seen;
  StageConfidenceReason confidence_reason;
} StageExitEvent;

typedef struct
{
  uint32_t tick_ms;
  uint32_t snapshot_count;
  uint32_t candidate_age_ms;
  uint32_t range5_baseline;
  uint32_t range5_min;
  uint32_t range5_max;
  uint8_t monitor_state;
  uint8_t armed;
  uint8_t candidate_active;
  uint8_t first_edge;
  uint8_t coalesced_first_edge;
  uint8_t edge_union;
  uint8_t last_edge;
  uint8_t history_count;
  uint8_t range5_transition_seen;
  uint8_t edge_propagation_seen;
  int16_t pre_exit_left_pwm;
  int16_t pre_exit_right_pwm;
  uint8_t lost_confirmed;
  StageExitDirection exit_direction;
  StageConfidenceReason confidence_reason;
  int16_t final_left_pwm;
  int16_t final_right_pwm;
} StageMonitorDebug;

extern volatile StageMonitorDebug g_stage_debug;

void StageMonitor_EnterOnStage(const SensorSnapshot *snapshot);
void StageMonitor_Update(const SensorSnapshot *snapshot,
                         const FightMotionCommand *final_motion);
uint8_t StageMonitor_IsStageLostConfirmed(void);
const StageExitEvent *StageMonitor_GetExitEvent(void);
StageMonitorState StageMonitor_GetState(void);

#endif /* STAGE_MONITOR_H__ */

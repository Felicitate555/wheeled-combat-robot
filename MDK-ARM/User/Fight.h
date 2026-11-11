#ifndef FIGHT_H__
#define FIGHT_H__

#include "Sensor.h"

typedef enum
{
  TARGET_NONE = 0,
  TARGET_FRONT,
  TARGET_FRONT_LEFT,
  TARGET_LEFT,
  TARGET_REAR_LEFT,
  TARGET_REAR,
  TARGET_REAR_RIGHT,
  TARGET_RIGHT,
  TARGET_FRONT_RIGHT
} TargetRegion;

typedef enum
{
  FIGHT_SEARCH = 0,
  FIGHT_TRACK,
  FIGHT_ATTACK,
  FIGHT_EDGE_ESCAPE,
  FIGHT_RECOVER
} FightState;

typedef enum
{
  SEARCH_SPIN_PRIMARY = 0,
  SEARCH_ADVANCE_ARC,
  SEARCH_SPIN_REVERSE,
  SEARCH_ADVANCE_ARC_REVERSE
} FightSearchPhase;

typedef enum
{
  EDGE_PHASE_PUSH = 0,
  EDGE_PHASE_SAFE_CONFIRM,
  EDGE_PHASE_RETRY,
  EDGE_PHASE_TIMEOUT
} FightEdgePhase;

typedef enum
{
  FIGHT_CMD_STOP = 0,
  FIGHT_CMD_SEARCH_SPIN,
  FIGHT_CMD_SEARCH_ARC,
  FIGHT_CMD_TRACK_FORWARD,
  FIGHT_CMD_TRACK_ARC_LEFT,
  FIGHT_CMD_TRACK_ARC_RIGHT,
  FIGHT_CMD_TURN_LEFT,
  FIGHT_CMD_TURN_RIGHT,
  FIGHT_CMD_ATTACK_FORWARD,
  FIGHT_CMD_ATTACK_ARC_LEFT,
  FIGHT_CMD_ATTACK_ARC_RIGHT,
  FIGHT_CMD_EDGE_BACK,
  FIGHT_CMD_EDGE_BACK_LEFT,
  FIGHT_CMD_EDGE_BACK_RIGHT,
  FIGHT_CMD_EDGE_FORWARD,
  FIGHT_CMD_EDGE_FORWARD_LEFT,
  FIGHT_CMD_EDGE_FORWARD_RIGHT,
  FIGHT_CMD_EDGE_TURN_LEFT,
  FIGHT_CMD_EDGE_TURN_RIGHT,
  FIGHT_CMD_RECOVER_TURN
} FightCommandId;

typedef struct
{
  int16_t left_pwm;
  int16_t right_pwm;
  FightCommandId id;
} FightMotionCommand;

typedef struct
{
  uint32_t tick_ms;
  uint32_t snapshot_count;
  uint32_t range1_raw;
  uint32_t range2_raw;
  uint32_t range3_raw;
  uint32_t range4_raw;
  uint32_t range5_raw;
  uint32_t range1_filtered;
  uint32_t range2_filtered;
  uint32_t range3_filtered;
  uint32_t range4_filtered;
  uint32_t adc1_frame_count;
  uint32_t adc1_error_count;
  uint32_t adc1_age_ms;
  uint32_t adc2_error_count;
  uint32_t adc2_age_ms;
  uint16_t enemy_raw_mask;
  uint16_t enemy_filtered_mask;
  uint8_t analog_target_mask;
  uint8_t edge_raw_mask;
  uint8_t adc1_valid;
  uint8_t adc2_valid;
  TargetRegion observed_target;
  TargetRegion target_region;
  TargetRegion last_target_region;
  uint8_t target_score;
  uint8_t target_held;
  FightState fight_state;
  FightSearchPhase search_phase;
  FightEdgePhase edge_phase;
  FightCommandId command_id;
  int16_t left_command;
  int16_t right_command;
  uint8_t edge_retry_count;
  uint8_t edge_timeout_fault;
  uint8_t restage_request;
  uint8_t active;
} FightDebugFrame;

extern volatile FightDebugFrame g_fight_debug;

void Fight_Enter(void);
void Fight_Exit(void);
void Fight_Update(const SensorSnapshot *snapshot);
const FightMotionCommand *Fight_GetMotionCommand(void);
FightState Fight_GetState(void);
TargetRegion Fight_GetTargetRegion(void);
uint8_t Fight_ConsumeRestageRequest(void);

#endif /* FIGHT_H__ */

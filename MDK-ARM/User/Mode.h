#ifndef MODE_H__
#define MODE_H__

#include "main.h"
#include "StageMonitor.h"

typedef enum
{
  MODE_RESTAGE_SETTLE = 0,
  MODE_RESTAGE_VERIFY_GROUND,
  MODE_RESTAGE_RETRY_FORWARD,
  MODE_RESTAGE_RETRY_LEFT_TURN,
  MODE_RESTAGE_SEARCH,
  MODE_RESTAGE_CLIMB,
  MODE_RESTAGE_CONFIRM_ON_STAGE,
  MODE_RESTAGE_RAMP_RETREAT_STOP
} ModeRestageState;

typedef struct
{
  uint32_t tick_ms;
  uint32_t range1;
  uint32_t range2;
  uint32_t range5;
  uint32_t front_delta;
  uint8_t small_mode;
  uint8_t needs_real_car_validation;
} ModeDebugFrame;

typedef struct
{
  uint32_t tick_ms;
  ModeRestageState restage_state;
  StageExitDirection exit_direction;
  StageConfidenceReason confidence_reason;
  uint16_t retry_attempt;
  uint8_t rear_search_armed;
  uint8_t rear_edge_new_event;
  uint8_t climb_started;
  uint8_t climb_range_event;
  uint8_t climb_edge_propagation;
  uint32_t verify_on_ms;
  uint32_t verify_off_ms;
  uint32_t confirm_stage_ms;
  int16_t final_left_pwm;
  int16_t final_right_pwm;
} RestageDebug;

extern volatile ModeDebugFrame g_mode_debug;
extern volatile RestageDebug g_restage_debug;

unsigned char Mode_WaitStart(void);
unsigned char Mode_GoOnStage(void);
unsigned char Mode_Finding(void);
unsigned char Mode_GoOnStageAgain(void);
void Mode_FightEnter(void);
void Mode_FightExit(void);

#endif /* MODE_H__ */

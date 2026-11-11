#include "main.h"

#include "Car.h"
#include "Fight.h"
#include "Mode.h"
#include "Sensor.h"
#include "StageMonitor.h"
#include "TIME.h"
#include "fight_config.h"

unsigned char Mode_SmallMode = 0U;
unsigned char Mode_BackFlag = 0U;
unsigned int Mode_ClockNum = 0U;
unsigned char Mode_ClockTriggerNum = 0U;
volatile ModeDebugFrame g_mode_debug;
volatile RestageDebug g_restage_debug;

typedef enum
{
  MODE_FIRST_STAGE_REVERSE_DELAY = 0U,
  MODE_FIRST_STAGE_WAIT_RANGE5,
  MODE_FIRST_STAGE_ON_STAGE_CONFIRM,
  MODE_FIRST_STAGE_CONFIRM_STOPPED
} ModeFirstStageState;

typedef struct
{
  uint8_t initialized;
  ModeRestageState state;
  uint32_t state_enter_ms;
  StageExitEvent exit_event;
  uint16_t retry_attempt;

  uint8_t verify_on_active;
  uint32_t verify_on_start_ms;
  uint8_t verify_off_active;
  uint32_t verify_off_start_ms;
  uint8_t verify_edge_active;
  uint32_t verify_edge_start_ms;
  int8_t verify_edge_turn_direction;

  uint8_t rear_pending_mask;
  uint8_t rear_baseline_mask;
  uint32_t rear_pending_start_ms;
  uint8_t rear_search_armed;
  uint8_t rear_edge_new_event;

  uint8_t climb_range_active;
  uint32_t climb_range_start_ms;
  uint8_t climb_range_event;
  uint8_t climb_last_edge;
  uint8_t climb_edge_propagation;
  uint8_t climb_edge_safe_active;
  uint32_t climb_edge_safe_start_ms;
  uint8_t climb_edge_complete;

  uint8_t confirm_stage_active;
  uint32_t confirm_stage_start_ms;
} ModeRestageContext;

static ModeRestageContext g_restage;

static uint8_t Mode_Range5OnStage(const SensorSnapshot *snapshot)
{
  if ((snapshot == NULL) || (snapshot->adc2_valid == 0U))
  {
    return 0U;
  }
  return (snapshot->range5_raw < RANGE5_STAGE_THRESHOLD) ? 1U : 0U;
}

static void Mode_ResetRestageContext(void)
{
  ModeRestageContext empty_context = {0};
  RestageDebug empty_debug = {0};

  g_restage = empty_context;
  g_restage.state = MODE_RESTAGE_SETTLE;
  g_restage_debug = empty_debug;
}

static void Mode_SetRestageState(ModeRestageState state, uint32_t now_ms)
{
  g_restage.state = state;
  g_restage.state_enter_ms = now_ms;
  Mode_SmallMode = (unsigned char)state;
}

static int8_t Mode_VerifyEdgeTurnDirection(uint8_t edge_mask)
{
  uint8_t left_edges = edge_mask &
    (SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_REAR_LEFT);
  uint8_t right_edges = edge_mask &
    (SENSOR_EDGE_FRONT_RIGHT | SENSOR_EDGE_REAR_RIGHT);

  if ((left_edges != 0U) && (right_edges == 0U))
  {
    return 1;  /* turn right */
  }
  if ((right_edges != 0U) && (left_edges == 0U))
  {
    return -1; /* turn left */
  }

  /* Front/rear pair or mixed corner: prefer the known useful left-turn
     relocation used by the restage retry sequence. */
  return -1;
}

static void Mode_ApplyVerifyMotion(const SensorSnapshot *snapshot)
{
  const uint8_t front_pair =
    SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_FRONT_RIGHT;
  const uint8_t rear_pair =
    SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT;
  const uint8_t left_pair =
    SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_REAR_LEFT;
  const uint8_t right_pair =
    SENSOR_EDGE_FRONT_RIGHT | SENSOR_EDGE_REAR_RIGHT;
  uint8_t edge_mask = snapshot->edge_raw_mask;
  uint32_t edge_elapsed = 0U;
  FightMotionCommand motion;
  int16_t outer = (int16_t)STAGE_VERIFY_SPEED;
  int16_t inner = (int16_t)(((uint32_t)STAGE_VERIFY_SPEED *
                             STAGE_VERIFY_ARC_INNER_PERCENT) / 100U);

  /* VERIFY_GROUND remains conservative, but its Edge override is now
     stateful. The old implementation could keep issuing Forward(80) forever
     when the rear pair stayed active on the start square, physically pushing
     the nose into the platform wall and never reaching the retry turn. */
  if (edge_mask != 0U)
  {
    if (g_restage.verify_edge_active == 0U)
    {
      g_restage.verify_edge_active = 1U;
      g_restage.verify_edge_start_ms = snapshot->tick_ms;
      g_restage.verify_edge_turn_direction =
        Mode_VerifyEdgeTurnDirection(edge_mask);
    }
    edge_elapsed = snapshot->tick_ms - g_restage.verify_edge_start_ms;

    /* Only allow a short translational escape pulse. If an edge is still
       present afterwards, pivot instead of continuing to ram the boundary. */
    if (edge_elapsed >= STAGE_VERIFY_EDGE_PUSH_MAX_MS)
    {
      if (g_restage.verify_edge_turn_direction < 0)
      {
        Car_SetTurnSpeed(Car_Lift, STAGE_VERIFY_EDGE_TURN_SPEED);
      }
      else
      {
        Car_SetTurnSpeed(Car_Right, STAGE_VERIFY_EDGE_TURN_SPEED);
      }
      return;
    }

    if (((edge_mask & front_pair) != 0U) &&
        ((edge_mask & rear_pair) == 0U))
    {
      Car_SetBackSpeed(EDGE_ESCAPE_SPEED);
      return;
    }
    if (((edge_mask & rear_pair) != 0U) &&
        ((edge_mask & front_pair) == 0U))
    {
      Car_SetForwardSpeed(EDGE_ESCAPE_SPEED);
      return;
    }
    if (((edge_mask & left_pair) != 0U) &&
        ((edge_mask & right_pair) == 0U))
    {
      Car_SetTurnSpeed(Car_Right, EDGE_ESCAPE_SPEED);
      return;
    }
    if (((edge_mask & right_pair) != 0U) &&
        ((edge_mask & left_pair) == 0U))
    {
      Car_SetTurnSpeed(Car_Lift, EDGE_ESCAPE_SPEED);
      return;
    }

    if (g_restage.verify_edge_turn_direction < 0)
    {
      Car_SetTurnSpeed(Car_Lift, STAGE_VERIFY_EDGE_TURN_SPEED);
    }
    else
    {
      Car_SetTurnSpeed(Car_Right, STAGE_VERIFY_EDGE_TURN_SPEED);
    }
    return;
  }

  g_restage.verify_edge_active = 0U;
  g_restage.verify_edge_start_ms = 0U;

  /* Right-front arc: left wheel is the outer wheel, right wheel is 70%. */
  motion.id = FIGHT_CMD_SEARCH_ARC;
  motion.left_pwm = outer;
  motion.right_pwm = inner;
  Car_ApplyFightCommand(&motion);
}

static void Mode_StartVerifyGround(const SensorSnapshot *snapshot)
{
  Mode_SetRestageState(MODE_RESTAGE_VERIFY_GROUND, snapshot->tick_ms);
  g_restage.verify_on_active = 0U;
  g_restage.verify_on_start_ms = 0U;
  g_restage.verify_off_active = 0U;
  g_restage.verify_off_start_ms = 0U;
  g_restage.verify_edge_active = 0U;
  g_restage.verify_edge_start_ms = 0U;
  g_restage.verify_edge_turn_direction = -1;
  Mode_ApplyVerifyMotion(snapshot);
}

static void Mode_StartRetryForward(uint32_t now_ms)
{
  Mode_SetRestageState(MODE_RESTAGE_RETRY_FORWARD, now_ms);
  Car_SetForwardSpeed(RESTAGE_RETRY_FORWARD_SPEED);
}

static void Mode_StartRetryLeftTurn(uint32_t now_ms)
{
  Mode_SetRestageState(MODE_RESTAGE_RETRY_LEFT_TURN, now_ms);
  Car_SetTurnSpeed(Car_Lift, RESTAGE_RETRY_LEFT_TURN_SPEED);
}

static uint32_t Mode_GetRetryTurnDuration(void)
{
  uint16_t pattern_index;

  if (g_restage.retry_attempt == 0U)
  {
    return RESTAGE_RETRY_FIRST_LEFT_TURN_MS;
  }

  /* Repeating exactly the same failed turn can create a geometric limit
     cycle. Keep all later turns shorter than the first one, but dither them
     deterministically to probe slightly different rear-to-ramp angles. */
  pattern_index = (uint16_t)((g_restage.retry_attempt - 1U) % 3U);
  if (pattern_index == 0U)
  {
    return RESTAGE_RETRY_NEXT_LEFT_TURN_A_MS;
  }
  if (pattern_index == 1U)
  {
    return RESTAGE_RETRY_NEXT_LEFT_TURN_B_MS;
  }
  return RESTAGE_RETRY_NEXT_LEFT_TURN_C_MS;
}

static void Mode_StartRestageSearch(const SensorSnapshot *snapshot)
{
  const uint8_t rear_pair =
    SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT;
  uint8_t rear_mask = snapshot->edge_raw_mask & rear_pair;

  Mode_SetRestageState(MODE_RESTAGE_SEARCH, snapshot->tick_ms);
  g_restage.rear_pending_mask = rear_mask;
  g_restage.rear_baseline_mask = rear_mask;
  g_restage.rear_pending_start_ms = snapshot->tick_ms;
  g_restage.rear_search_armed = 0U;
  g_restage.rear_edge_new_event = 0U;
  Car_SetBackSpeed(RESTAGE_SEARCH_SPEED);
}

static void Mode_RequestRetry(uint32_t now_ms)
{
  /* No FAILED_STOP. Every failure starts another forward -> left turn -> back
     attempt. retry_attempt saturates only for debug; behavior stays infinite. */
  if (g_restage.retry_attempt < 0xFFFFU)
  {
    g_restage.retry_attempt++;
  }
  Mode_StartRetryForward(now_ms);
}

static void Mode_StartRampRetreat(uint32_t now_ms)
{
  Mode_SetRestageState(MODE_RESTAGE_RAMP_RETREAT_STOP, now_ms);
  Car_SetDirection(Car_Stop);
}

static uint8_t Mode_UpdateRearSearchEvent(const SensorSnapshot *snapshot)
{
  const uint8_t rear_pair =
    SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT;
  uint8_t rear_mask = snapshot->edge_raw_mask & rear_pair;
  uint8_t changed_mask;

  if (g_restage.rear_search_armed == 0U)
  {
    if (rear_mask != g_restage.rear_pending_mask)
    {
      g_restage.rear_pending_mask = rear_mask;
      g_restage.rear_pending_start_ms = snapshot->tick_ms;
      return 0U;
    }
    if ((uint32_t)(snapshot->tick_ms -
                   g_restage.rear_pending_start_ms) >=
        RESTAGE_REAR_BASELINE_MS)
    {
      g_restage.rear_baseline_mask = rear_mask;
      g_restage.rear_search_armed = 1U;
    }
    return 0U;
  }

  if (rear_mask == g_restage.rear_baseline_mask)
  {
    g_restage.rear_pending_mask = rear_mask;
    g_restage.rear_pending_start_ms = snapshot->tick_ms;
    return 0U;
  }
  if (rear_mask != g_restage.rear_pending_mask)
  {
    g_restage.rear_pending_mask = rear_mask;
    g_restage.rear_pending_start_ms = snapshot->tick_ms;
    return 0U;
  }
  if ((uint32_t)(snapshot->tick_ms -
                 g_restage.rear_pending_start_ms) <
      RESTAGE_REAR_EVENT_STABLE_MS)
  {
    return 0U;
  }

  changed_mask = (rear_mask ^ g_restage.rear_baseline_mask) & rear_pair;
  return changed_mask;
}

static void Mode_StartRestageClimb(const SensorSnapshot *snapshot,
                                   uint8_t rear_event)
{
  Mode_SetRestageState(MODE_RESTAGE_CLIMB, snapshot->tick_ms);
  g_restage.rear_edge_new_event = rear_event;
  g_restage.climb_range_active = Mode_Range5OnStage(snapshot);
  g_restage.climb_range_start_ms = snapshot->tick_ms;
  g_restage.climb_range_event = 0U;
  g_restage.climb_last_edge = snapshot->edge_raw_mask;
  g_restage.climb_edge_propagation = 0U;
  g_restage.climb_edge_safe_active = 0U;
  g_restage.climb_edge_complete = 0U;
  Car_SetBackSpeed(RESTAGE_CLIMB_SPEED);
}

static void Mode_UpdateClimbEvidence(const SensorSnapshot *snapshot)
{
  uint8_t on_stage = Mode_Range5OnStage(snapshot);

  /* Primary evidence: fresh Range5 has actually crossed into the stage region
     and remains there briefly. No generic delta can finish Restage anymore. */
  if (on_stage != 0U)
  {
    if (g_restage.climb_range_active == 0U)
    {
      g_restage.climb_range_active = 1U;
      g_restage.climb_range_start_ms = snapshot->tick_ms;
    }
    else if ((uint32_t)(snapshot->tick_ms -
                        g_restage.climb_range_start_ms) >=
             RESTAGE_CLIMB_RANGE_STABLE_MS)
    {
      g_restage.climb_range_event = 1U;
    }
  }
  else
  {
    g_restage.climb_range_active = 0U;
  }

  /* Edge is only auxiliary evidence while climbing. It may help us reduce
     speed earlier, but it can never complete the final on-stage decision. */
  if (snapshot->edge_raw_mask != g_restage.climb_last_edge)
  {
    g_restage.climb_edge_propagation = 1U;
    g_restage.climb_last_edge = snapshot->edge_raw_mask;
  }

  if ((g_restage.climb_edge_propagation != 0U) &&
      (snapshot->edge_raw_mask == 0U))
  {
    if (g_restage.climb_edge_safe_active == 0U)
    {
      g_restage.climb_edge_safe_active = 1U;
      g_restage.climb_edge_safe_start_ms = snapshot->tick_ms;
    }
    else if ((uint32_t)(snapshot->tick_ms -
                        g_restage.climb_edge_safe_start_ms) >=
             RESTAGE_CLIMB_EDGE_SAFE_MS)
    {
      g_restage.climb_edge_complete = 1U;
    }
  }
  else
  {
    g_restage.climb_edge_safe_active = 0U;
  }
}

static void Mode_StartRestageConfirm(const SensorSnapshot *snapshot)
{
  Mode_SetRestageState(MODE_RESTAGE_CONFIRM_ON_STAGE, snapshot->tick_ms);
  g_restage.confirm_stage_active = Mode_Range5OnStage(snapshot);
  g_restage.confirm_stage_start_ms = snapshot->tick_ms;
  Car_SetBackSpeed(RESTAGE_CONFIRM_SPEED);
}

static void Mode_UpdateRestageDebug(const SensorSnapshot *snapshot)
{
  RestageDebug frame = {0};
  int16_t left_pwm = 0;
  int16_t right_pwm = 0;

  Car_GetSignedOutput(&left_pwm, &right_pwm);
  frame.tick_ms = snapshot->tick_ms;
  frame.restage_state = g_restage.state;
  frame.exit_direction = g_restage.exit_event.direction;
  frame.confidence_reason = g_restage.exit_event.confidence_reason;
  frame.retry_attempt = g_restage.retry_attempt;
  frame.rear_search_armed = g_restage.rear_search_armed;
  frame.rear_edge_new_event = g_restage.rear_edge_new_event;
  frame.climb_started =
    ((g_restage.state == MODE_RESTAGE_CLIMB) ||
     (g_restage.state == MODE_RESTAGE_CONFIRM_ON_STAGE)) ? 1U : 0U;
  frame.climb_range_event = g_restage.climb_range_event;
  frame.climb_edge_propagation = g_restage.climb_edge_propagation;
  if (g_restage.verify_on_active != 0U)
  {
    frame.verify_on_ms = snapshot->tick_ms - g_restage.verify_on_start_ms;
  }
  if (g_restage.verify_off_active != 0U)
  {
    frame.verify_off_ms = snapshot->tick_ms - g_restage.verify_off_start_ms;
  }
  if (g_restage.confirm_stage_active != 0U)
  {
    frame.confirm_stage_ms =
      snapshot->tick_ms - g_restage.confirm_stage_start_ms;
  }
  frame.final_left_pwm = left_pwm;
  frame.final_right_pwm = right_pwm;
  g_restage_debug = frame;

  g_mode_debug.tick_ms = snapshot->tick_ms;
  g_mode_debug.range1 = snapshot->range_raw[0];
  g_mode_debug.range2 = snapshot->range_raw[1];
  g_mode_debug.range5 = snapshot->range5_raw;
  g_mode_debug.front_delta =
    (snapshot->range_raw[0] >= snapshot->range_raw[1]) ?
    (snapshot->range_raw[0] - snapshot->range_raw[1]) :
    (snapshot->range_raw[1] - snapshot->range_raw[0]);
  g_mode_debug.small_mode = Mode_SmallMode;
  g_mode_debug.needs_real_car_validation = 1U;
}

unsigned char Mode_WaitStart(void)
{
  if (HAL_GPIO_ReadPin(IR3_GPIO_Port, IR3_Pin) == GPIO_PIN_RESET)
  {
    return 1U;
  }
  return 0U;
}

unsigned char Mode_GoOnStage(void)
{
  const SensorSnapshot *snapshot = Sensor_GetSnapshot();

  switch (Mode_SmallMode)
  {
    case MODE_FIRST_STAGE_REVERSE_DELAY:
      Car_SetBackSpeed(FIRST_STAGE_BACK_SPEED);
      TIME_SetTimingMs(1U, FIRST_STAGE_BACK_MS);
      if (TIME_GetTimeOverFlag(1U) == TIME_Over)
      {
        Mode_SmallMode = MODE_FIRST_STAGE_WAIT_RANGE5;
      }
      break;

    case MODE_FIRST_STAGE_WAIT_RANGE5:
      Car_SetBackSpeed(FIRST_STAGE_BACK_SPEED);
      if (TIME_GetADCValue(5U) < RANGE5_STAGE_THRESHOLD)
      {
        Car_SetBackSpeed(FIRST_STAGE_CONFIRM_SPEED);
        TIME_ClearSetTime(4U);
        TIME_ClearSetTime(5U);
        TIME_SetTimingMs(5U, FIRST_STAGE_CONFIRM_MAX_MS);
        Mode_SmallMode = MODE_FIRST_STAGE_ON_STAGE_CONFIRM;
      }
      break;

    case MODE_FIRST_STAGE_ON_STAGE_CONFIRM:
      Car_SetBackSpeed(FIRST_STAGE_CONFIRM_SPEED);
      if (snapshot->edge_raw_mask == 0U)
      {
        TIME_SetTimingMs(4U, FIRST_STAGE_EDGE_SAFE_CONFIRM_MS);
        if (TIME_GetTimeOverFlag(4U) == TIME_Over)
        {
          TIME_ClearSetTime(5U);
          Car_SetDirection(Car_Stop);
          Mode_SmallMode = MODE_FIRST_STAGE_REVERSE_DELAY;
          Mode_BackFlag = 1U;
          return 1U;
        }
      }
      else
      {
        TIME_ClearSetTime(4U);
      }

      TIME_SetTimingMs(5U, FIRST_STAGE_CONFIRM_MAX_MS);
      if (TIME_GetTimeOverFlag(5U) == TIME_Over)
      {
        Car_SetDirection(Car_Stop);
        TIME_ClearSetTime(4U);
        Mode_SmallMode = MODE_FIRST_STAGE_CONFIRM_STOPPED;
      }
      break;

    case MODE_FIRST_STAGE_CONFIRM_STOPPED:
      Car_SetDirection(Car_Stop);
      if (snapshot->edge_raw_mask == 0U)
      {
        TIME_SetTimingMs(4U, FIRST_STAGE_EDGE_SAFE_CONFIRM_MS);
        if (TIME_GetTimeOverFlag(4U) == TIME_Over)
        {
          Mode_SmallMode = MODE_FIRST_STAGE_REVERSE_DELAY;
          Mode_BackFlag = 1U;
          return 1U;
        }
      }
      else
      {
        TIME_ClearSetTime(4U);
      }
      break;

    default:
      Car_SetDirection(Car_Stop);
      TIME_ClearSetTime(4U);
      TIME_ClearSetTime(5U);
      Mode_SmallMode = MODE_FIRST_STAGE_REVERSE_DELAY;
      break;
  }
  return 0U;
}

unsigned char Mode_Finding(void)
{
  const SensorSnapshot *snapshot = Sensor_GetSnapshot();

  Fight_Update(snapshot);
#if ENABLE_STAGE_RESTAGE
  StageMonitor_Update(snapshot, Fight_GetMotionCommand());
  if (StageMonitor_IsStageLostConfirmed() != 0U)
  {
    return 1U;
  }
#endif
  return 0U;
}

unsigned char Mode_GoOnStageAgain(void)
{
  const SensorSnapshot *snapshot = Sensor_GetSnapshot();
  uint32_t now_ms = snapshot->tick_ms;
  uint32_t state_elapsed;
  uint32_t turn_duration;
  uint8_t rear_event;
  uint8_t on_stage;

  if (g_restage.initialized == 0U)
  {
    const StageExitEvent *event = StageMonitor_GetExitEvent();

    Mode_ResetRestageContext();
    g_restage.initialized = 1U;
    if ((event != NULL) && (event->valid != 0U))
    {
      g_restage.exit_event = *event;
    }
    else
    {
      g_restage.exit_event.valid = 1U;
      g_restage.exit_event.direction = STAGE_EXIT_UNKNOWN;
      g_restage.exit_event.confidence_reason = STAGE_CONFIDENCE_RANGE5_HARD_LOST;
    }

    if (g_restage.exit_event.confidence_reason ==
        STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN)
    {
      Mode_StartVerifyGround(snapshot);
    }
    else
    {
      Mode_SetRestageState(MODE_RESTAGE_SETTLE, now_ms);
      Car_SetDirection(Car_Stop);
    }
  }

  state_elapsed = now_ms - g_restage.state_enter_ms;
  switch (g_restage.state)
  {
    case MODE_RESTAGE_VERIFY_GROUND:
      on_stage = Mode_Range5OnStage(snapshot);

      if (snapshot->adc2_valid != 0U)
      {
        if (on_stage != 0U)
        {
          g_restage.verify_off_active = 0U;

          /* Edge is an auxiliary safety veto for declaring "back on main
             stage". It does not decide that we are off-stage. */
          if (snapshot->edge_raw_mask == 0U)
          {
            if (g_restage.verify_on_active == 0U)
            {
              g_restage.verify_on_active = 1U;
              g_restage.verify_on_start_ms = now_ms;
            }
            else if ((uint32_t)(now_ms - g_restage.verify_on_start_ms) >=
                     STAGE_VERIFY_ON_CONFIRM_MS)
            {
              Car_SetDirection(Car_Stop);
              Mode_UpdateRestageDebug(snapshot);
              return 1U;
            }
          }
          else
          {
            g_restage.verify_on_active = 0U;
          }
        }
        else
        {
          g_restage.verify_on_active = 0U;
          if (g_restage.verify_off_active == 0U)
          {
            g_restage.verify_off_active = 1U;
            g_restage.verify_off_start_ms = now_ms;
          }
          else if ((uint32_t)(now_ms - g_restage.verify_off_start_ms) >=
                   STAGE_VERIFY_OFF_CONFIRM_MS)
          {
            /* We moved away from the candidate surface and Range5 remained
               off-stage: treat the prior ON as the start square/ground. */
            g_restage.verify_off_active = 0U;
            g_restage.verify_on_active = 0U;
            Mode_SetRestageState(MODE_RESTAGE_SETTLE, now_ms);
            Car_SetDirection(Car_Stop);
            break;
          }
        }
      }
      else
      {
        /* Never accumulate evidence from stale ADC2 data. */
        g_restage.verify_on_active = 0U;
        g_restage.verify_off_active = 0U;
      }

      if (state_elapsed >= STAGE_VERIFY_MAX_MS)
      {
        /* Inconclusive verification fails open back to Fight. This matches the
           competition priority: avoid a false Restage while still on stage. */
        Car_SetDirection(Car_Stop);
        Mode_UpdateRestageDebug(snapshot);
        return 1U;
      }

      Mode_ApplyVerifyMotion(snapshot);
      break;

    case MODE_RESTAGE_SETTLE:
      Car_SetDirection(Car_Stop);
      if (state_elapsed >= RESTAGE_SETTLE_MS)
      {
        Mode_StartRetryForward(now_ms);
      }
      break;

    case MODE_RESTAGE_RETRY_FORWARD:
      Car_SetForwardSpeed(RESTAGE_RETRY_FORWARD_SPEED);
      if (state_elapsed >= RESTAGE_RETRY_FORWARD_MS)
      {
        Mode_StartRetryLeftTurn(now_ms);
      }
      break;

    case MODE_RESTAGE_RETRY_LEFT_TURN:
      Car_SetTurnSpeed(Car_Lift, RESTAGE_RETRY_LEFT_TURN_SPEED);
      turn_duration = Mode_GetRetryTurnDuration();
      if (state_elapsed >= turn_duration)
      {
        Mode_StartRestageSearch(snapshot);
      }
      break;

    case MODE_RESTAGE_SEARCH:
      Car_SetBackSpeed(RESTAGE_SEARCH_SPEED);
      rear_event = Mode_UpdateRearSearchEvent(snapshot);
      if (rear_event != 0U)
      {
        Mode_StartRestageClimb(snapshot, rear_event);
        break;
      }
      if (state_elapsed >= RESTAGE_SEARCH_TIMEOUT_MS)
      {
        Mode_RequestRetry(now_ms);
      }
      break;

    case MODE_RESTAGE_CLIMB:
      Car_SetBackSpeed(RESTAGE_CLIMB_SPEED);
      Mode_UpdateClimbEvidence(snapshot);
      if ((state_elapsed >= RESTAGE_CLIMB_MIN_MS) &&
          ((g_restage.climb_range_event != 0U) ||
           (g_restage.climb_edge_complete != 0U)))
      {
        Mode_StartRestageConfirm(snapshot);
        break;
      }
      if (state_elapsed >= RESTAGE_CLIMB_TIMEOUT_MS)
      {
        Mode_StartRampRetreat(now_ms);
      }
      break;

    case MODE_RESTAGE_CONFIRM_ON_STAGE:
      Car_SetBackSpeed(RESTAGE_CONFIRM_SPEED);
      if (Mode_Range5OnStage(snapshot) != 0U)
      {
        if (g_restage.confirm_stage_active == 0U)
        {
          g_restage.confirm_stage_active = 1U;
          g_restage.confirm_stage_start_ms = now_ms;
        }
        else if ((uint32_t)(now_ms - g_restage.confirm_stage_start_ms) >=
                 RESTAGE_STAGE_CONFIRM_MS)
        {
          Car_SetDirection(Car_Stop);
          Mode_BackFlag = 1U;
          Mode_UpdateRestageDebug(snapshot);
          return 1U;
        }
      }
      else
      {
        g_restage.confirm_stage_active = 0U;
      }

      if (state_elapsed >= RESTAGE_CONFIRM_TIMEOUT_MS)
      {
        Mode_StartRampRetreat(now_ms);
      }
      break;

    case MODE_RESTAGE_RAMP_RETREAT_STOP:
      Car_SetDirection(Car_Stop);
      if (state_elapsed >= RESTAGE_RAMP_STOP_MS)
      {
        /* The next state is the mandatory forward pulse, so the robot can
           leave a jammed ramp/contact before the next left turn + reverse. */
        Mode_RequestRetry(now_ms);
      }
      break;

    default:
      /* Defensive fallback: never turn an unexpected state into permanent
         STOP. Re-enter the infinite retry loop instead. */
      Mode_RequestRetry(now_ms);
      break;
  }

  Mode_UpdateRestageDebug(snapshot);
  return 0U;
}

void Mode_FightEnter(void)
{
  Mode_SmallMode = 0U;
  Mode_BackFlag = 0U;
  Mode_ClockNum = 0U;
  Mode_ClockTriggerNum = 0U;
  TIME_ClearAll();
  g_mode_debug.needs_real_car_validation = 0U;
  Fight_Enter();
#if ENABLE_STAGE_RESTAGE
  StageMonitor_EnterOnStage(Sensor_GetSnapshot());
#endif
  Mode_ResetRestageContext();
}

void Mode_FightExit(void)
{
  Fight_Exit();
  Mode_SmallMode = 0U;
  Mode_BackFlag = 0U;
  Mode_ClockNum = 0U;
  Mode_ClockTriggerNum = 0U;
  TIME_ClearAll();
  Mode_ResetRestageContext();
  g_mode_debug.needs_real_car_validation = 1U;
}

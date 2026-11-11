#include "Fight.h"

#include "Car.h"
#include "fight_config.h"

typedef struct
{
  TargetRegion region;
  uint8_t score;
  uint8_t front_attack_confident;
  int8_t steering;
} TargetObservation;

typedef struct
{
  uint8_t active;
  FightState state;
  uint32_t state_enter_ms;
  uint32_t last_snapshot_count;

  TargetRegion observed_target;
  TargetRegion target_region;
  TargetRegion candidate_region;
  TargetRegion last_target_region;
  uint32_t last_target_seen_ms;
  uint8_t candidate_count;
  uint8_t target_score;
  uint8_t target_held;
  uint8_t attack_enter_count;
  uint8_t attack_exit_count;
  int8_t last_turn_direction;

  FightSearchPhase search_phase;
  uint32_t search_phase_enter_ms;
  int8_t search_primary_direction;
  uint8_t search_seed_from_edge;

  FightEdgePhase edge_phase;
  uint32_t edge_hard_start_ms;
  uint32_t edge_retry_start_ms;
  uint8_t edge_trigger_mask;
  uint8_t edge_safe_count;
  uint8_t edge_retry_count;
  uint8_t edge_timeout_fault;
  uint8_t restage_request;
  int8_t recover_turn_direction;

  FightMotionCommand command;
  uint32_t debug_last_publish_ms;
} FightContext;

static FightContext g_fight;
volatile FightDebugFrame g_fight_debug;

static FightMotionCommand Fight_MakeCommand(FightCommandId id,
                                             int16_t left_pwm,
                                             int16_t right_pwm)
{
  FightMotionCommand command;

  command.id = id;
  command.left_pwm = left_pwm;
  command.right_pwm = right_pwm;
  return command;
}

static int16_t Fight_ScalePwm(uint16_t pwm, uint16_t percent)
{
  return (int16_t)(((uint32_t)pwm * (uint32_t)percent) / 100U);
}

static void Fight_AddScore(uint8_t *scores,
                           TargetRegion region,
                           uint8_t amount)
{
  scores[(uint32_t)region] = (uint8_t)(scores[(uint32_t)region] + amount);
}

static TargetRegion Fight_SelectRegion(const uint8_t *scores,
                                       TargetRegion preferred,
                                       uint8_t *selected_score)
{
  static const TargetRegion priority[] =
  {
    TARGET_FRONT,
    TARGET_FRONT_LEFT,
    TARGET_FRONT_RIGHT,
    TARGET_LEFT,
    TARGET_RIGHT,
    TARGET_REAR_LEFT,
    TARGET_REAR_RIGHT,
    TARGET_REAR
  };
  TargetRegion best_region = TARGET_NONE;
  uint8_t best_score = 0U;
  uint32_t index;

  for (index = 0U; index < (sizeof(priority) / sizeof(priority[0])); index++)
  {
    TargetRegion region = priority[index];
    uint8_t score = scores[(uint32_t)region];

    if (score > best_score)
    {
      best_score = score;
      best_region = region;
    }
  }

  if (best_score < TARGET_REGION_MIN_SCORE)
  {
    *selected_score = 0U;
    return TARGET_NONE;
  }

  if ((preferred != TARGET_NONE) &&
      (scores[(uint32_t)preferred] >= TARGET_REGION_MIN_SCORE) &&
      ((uint16_t)scores[(uint32_t)preferred] + TARGET_REGION_SWITCH_MARGIN >=
       best_score))
  {
    best_region = preferred;
    best_score = scores[(uint32_t)preferred];
  }

  *selected_score = best_score;
  return best_region;
}

static TargetObservation Fight_FuseTarget(const SensorSnapshot *snapshot)
{
  uint8_t scores[TARGET_FRONT_RIGHT + 1U] = {0U};
  TargetObservation observation;
  TargetRegion preferred;
  uint8_t input_a11 = ((snapshot->analog_target_mask & SENSOR_ANALOG_A11) != 0U) ? 1U : 0U;
  uint8_t input_a1 = ((snapshot->analog_target_mask & SENSOR_ANALOG_A1) != 0U) ? 1U : 0U;
  uint8_t input_a3 = ((snapshot->analog_target_mask & SENSOR_ANALOG_A3) != 0U) ? 1U : 0U;
  uint8_t input_a9 = ((snapshot->analog_target_mask & SENSOR_ANALOG_A9) != 0U) ? 1U : 0U;
  uint8_t input_d2 = ((snapshot->enemy_filtered_mask & SENSOR_ENEMY_D2) != 0U) ? 1U : 0U;
  uint8_t input_d4 = ((snapshot->enemy_filtered_mask & SENSOR_ENEMY_D4) != 0U) ? 1U : 0U;
  uint8_t input_d5 = ((snapshot->enemy_filtered_mask & SENSOR_ENEMY_D5) != 0U) ? 1U : 0U;
  uint8_t input_d7 = ((snapshot->enemy_filtered_mask & SENSOR_ENEMY_D7) != 0U) ? 1U : 0U;
  uint8_t input_d8 = ((snapshot->enemy_filtered_mask & SENSOR_ENEMY_D8) != 0U) ? 1U : 0U;
  uint8_t input_d10 = ((snapshot->enemy_filtered_mask & SENSOR_ENEMY_D10) != 0U) ? 1U : 0U;
  uint8_t front_bridge = (((input_a11 != 0U) || (input_d10 != 0U)) &&
                          ((input_a1 != 0U) || (input_d2 != 0U))) ? 1U : 0U;
  uint8_t rear_bridge = (((input_d4 != 0U) || (input_d5 != 0U)) &&
                         ((input_d7 != 0U) || (input_d8 != 0U))) ? 1U : 0U;
  uint16_t left_strength;
  uint16_t right_strength;

  if (input_a11)
  {
    Fight_AddScore(scores, TARGET_FRONT_LEFT, 3U);
    Fight_AddScore(scores, TARGET_FRONT, 1U);
  }
  if (input_a1)
  {
    Fight_AddScore(scores, TARGET_FRONT_RIGHT, 3U);
    Fight_AddScore(scores, TARGET_FRONT, 1U);
  }
  if (input_a3)
  {
    Fight_AddScore(scores, TARGET_RIGHT, 3U);
    Fight_AddScore(scores, TARGET_FRONT_RIGHT, 1U);
    Fight_AddScore(scores, TARGET_REAR_RIGHT, 1U);
  }
  if (input_a9)
  {
    Fight_AddScore(scores, TARGET_LEFT, 3U);
    Fight_AddScore(scores, TARGET_FRONT_LEFT, 1U);
    Fight_AddScore(scores, TARGET_REAR_LEFT, 1U);
  }
  if (input_d2)
  {
    Fight_AddScore(scores, TARGET_FRONT_RIGHT, 3U);
    Fight_AddScore(scores, TARGET_RIGHT, 1U);
  }
  if (input_d4)
  {
    Fight_AddScore(scores, TARGET_REAR_RIGHT, 3U);
    Fight_AddScore(scores, TARGET_RIGHT, 1U);
  }
  if (input_d5)
  {
    Fight_AddScore(scores, TARGET_REAR_RIGHT, 3U);
    Fight_AddScore(scores, TARGET_REAR, 1U);
  }
  if (input_d7)
  {
    Fight_AddScore(scores, TARGET_REAR_LEFT, 3U);
    Fight_AddScore(scores, TARGET_REAR, 1U);
  }
  if (input_d8)
  {
    Fight_AddScore(scores, TARGET_REAR_LEFT, 3U);
    Fight_AddScore(scores, TARGET_LEFT, 1U);
  }
  if (input_d10)
  {
    Fight_AddScore(scores, TARGET_FRONT_LEFT, 3U);
    Fight_AddScore(scores, TARGET_LEFT, 1U);
  }

  if (front_bridge != 0U)
  {
    Fight_AddScore(scores, TARGET_FRONT, 4U);
  }
  if (rear_bridge != 0U)
  {
    Fight_AddScore(scores, TARGET_REAR, 4U);
  }
  if ((input_a11 != 0U) && (input_d10 != 0U))
  {
    Fight_AddScore(scores, TARGET_FRONT_LEFT, 2U);
  }
  if ((input_a1 != 0U) && (input_d2 != 0U))
  {
    Fight_AddScore(scores, TARGET_FRONT_RIGHT, 2U);
  }
  if (((input_d2 != 0U) && (input_a3 != 0U)) ||
      ((input_a3 != 0U) && (input_d4 != 0U)))
  {
    Fight_AddScore(scores, TARGET_RIGHT, 2U);
  }
  if ((input_d4 != 0U) && (input_d5 != 0U))
  {
    Fight_AddScore(scores, TARGET_REAR_RIGHT, 2U);
  }
  if ((input_d7 != 0U) && (input_d8 != 0U))
  {
    Fight_AddScore(scores, TARGET_REAR_LEFT, 2U);
  }
  if (((input_d8 != 0U) && (input_a9 != 0U)) ||
      ((input_a9 != 0U) && (input_d10 != 0U)))
  {
    Fight_AddScore(scores, TARGET_LEFT, 2U);
  }

  preferred = (g_fight.target_region != TARGET_NONE) ?
              g_fight.target_region : g_fight.last_target_region;
  observation.region = Fight_SelectRegion(scores,
                                          preferred,
                                          &observation.score);
  observation.front_attack_confident =
    ((observation.region == TARGET_FRONT) && (front_bridge != 0U)) ? 1U : 0U;

  left_strength = (uint16_t)scores[TARGET_FRONT_LEFT] +
                  (uint16_t)scores[TARGET_LEFT];
  right_strength = (uint16_t)scores[TARGET_FRONT_RIGHT] +
                   (uint16_t)scores[TARGET_RIGHT];
  if (left_strength >= (right_strength + 3U))
  {
    observation.steering = -1;
  }
  else if (right_strength >= (left_strength + 3U))
  {
    observation.steering = 1;
  }
  else
  {
    observation.steering = 0;
  }

  return observation;
}

static void Fight_SetState(FightState state, uint32_t now_ms)
{
  if (g_fight.state != state)
  {
    g_fight.state = state;
    g_fight.state_enter_ms = now_ms;
  }
}

static void Fight_UpdateTurnPreference(TargetRegion region)
{
  if ((region == TARGET_FRONT_LEFT) ||
      (region == TARGET_LEFT) ||
      (region == TARGET_REAR_LEFT))
  {
    g_fight.last_turn_direction = -1;
  }
  else if ((region == TARGET_FRONT_RIGHT) ||
           (region == TARGET_RIGHT) ||
           (region == TARGET_REAR_RIGHT))
  {
    g_fight.last_turn_direction = 1;
  }
}

static TargetRegion Fight_UpdateTargetLock(const TargetObservation *observation,
                                           uint32_t now_ms)
{
  g_fight.observed_target = observation->region;
  g_fight.target_score = observation->score;

  if (observation->region != TARGET_NONE)
  {
    g_fight.target_held = 0U;

    if (observation->region == g_fight.target_region)
    {
      g_fight.candidate_region = TARGET_NONE;
      g_fight.candidate_count = 0U;
      g_fight.last_target_region = observation->region;
      g_fight.last_target_seen_ms = now_ms;
    }
    else
    {
      if (observation->region != g_fight.candidate_region)
      {
        g_fight.candidate_region = observation->region;
        g_fight.candidate_count = 1U;
      }
      else if (g_fight.candidate_count < TARGET_REGION_SWITCH_CYCLES)
      {
        g_fight.candidate_count++;
      }

      if (g_fight.candidate_count >= TARGET_REGION_SWITCH_CYCLES)
      {
        g_fight.target_region = observation->region;
        g_fight.last_target_region = observation->region;
        g_fight.last_target_seen_ms = now_ms;
        g_fight.candidate_region = TARGET_NONE;
        g_fight.candidate_count = 0U;
      }
    }

    return g_fight.target_region;
  }

  g_fight.candidate_region = TARGET_NONE;
  g_fight.candidate_count = 0U;
  if ((g_fight.last_target_region != TARGET_NONE) &&
      ((uint32_t)(now_ms - g_fight.last_target_seen_ms) <= TARGET_HOLD_MS))
  {
    g_fight.target_region = g_fight.last_target_region;
    g_fight.target_held = 1U;
    return g_fight.target_region;
  }

  g_fight.target_region = TARGET_NONE;
  g_fight.target_held = 0U;
  return TARGET_NONE;
}

static void Fight_StartSearch(uint32_t now_ms)
{
  Fight_SetState(FIGHT_SEARCH, now_ms);
  g_fight.search_phase = SEARCH_SPIN_PRIMARY;
  g_fight.search_phase_enter_ms = now_ms;

  if (g_fight.search_seed_from_edge != 0U)
  {
    g_fight.search_seed_from_edge = 0U;
  }
  else if ((g_fight.last_target_region == TARGET_FRONT_LEFT) ||
      (g_fight.last_target_region == TARGET_LEFT) ||
      (g_fight.last_target_region == TARGET_REAR_LEFT))
  {
    g_fight.search_primary_direction = -1;
  }
  else if ((g_fight.last_target_region == TARGET_FRONT_RIGHT) ||
           (g_fight.last_target_region == TARGET_RIGHT) ||
           (g_fight.last_target_region == TARGET_REAR_RIGHT))
  {
    g_fight.search_primary_direction = 1;
  }
  else if (g_fight.search_primary_direction == 0)
  {
    g_fight.search_primary_direction = 1;
  }
}

static void Fight_AdvanceSearch(uint32_t now_ms)
{
  uint32_t elapsed_ms = now_ms - g_fight.search_phase_enter_ms;

  switch (g_fight.search_phase)
  {
    case SEARCH_SPIN_PRIMARY:
      if (elapsed_ms >= SEARCH_SPIN_PRIMARY_MS)
      {
        g_fight.search_phase = SEARCH_ADVANCE_ARC;
        g_fight.search_phase_enter_ms = now_ms;
      }
      break;

    case SEARCH_ADVANCE_ARC:
      if (elapsed_ms >= SEARCH_ADVANCE_ARC_MS)
      {
        g_fight.search_phase = SEARCH_SPIN_REVERSE;
        g_fight.search_phase_enter_ms = now_ms;
      }
      break;

    case SEARCH_SPIN_REVERSE:
      if (elapsed_ms >= SEARCH_SPIN_REVERSE_MS)
      {
        g_fight.search_phase = SEARCH_ADVANCE_ARC_REVERSE;
        g_fight.search_phase_enter_ms = now_ms;
      }
      break;

    case SEARCH_ADVANCE_ARC_REVERSE:
    default:
      if (elapsed_ms >= SEARCH_ADVANCE_ARC_REVERSE_MS)
      {
        g_fight.search_primary_direction =
          (int8_t)(-g_fight.search_primary_direction);
        g_fight.search_phase = SEARCH_SPIN_PRIMARY;
        g_fight.search_phase_enter_ms = now_ms;
      }
      break;
  }
}

static FightMotionCommand Fight_CommandForSearch(void)
{
  int16_t speed = (int16_t)SEARCH_SPEED;
  int16_t inner = Fight_ScalePwm(SEARCH_SPEED, SEARCH_ARC_INNER_PERCENT);
  int8_t direction = g_fight.search_primary_direction;

  if ((g_fight.search_phase == SEARCH_SPIN_REVERSE) ||
      (g_fight.search_phase == SEARCH_ADVANCE_ARC_REVERSE))
  {
    direction = (int8_t)(-direction);
  }

  if ((g_fight.search_phase == SEARCH_SPIN_PRIMARY) ||
      (g_fight.search_phase == SEARCH_SPIN_REVERSE))
  {
    if (direction > 0)
    {
      return Fight_MakeCommand(FIGHT_CMD_SEARCH_SPIN, speed, (int16_t)(-speed));
    }
    return Fight_MakeCommand(FIGHT_CMD_SEARCH_SPIN, (int16_t)(-speed), speed);
  }

  if (direction > 0)
  {
    return Fight_MakeCommand(FIGHT_CMD_SEARCH_ARC, speed, inner);
  }
  return Fight_MakeCommand(FIGHT_CMD_SEARCH_ARC, inner, speed);
}

static FightMotionCommand Fight_CommandForTarget(TargetRegion region,
                                                 uint8_t held_only,
                                                 uint8_t attack,
                                                 int8_t steering)
{
  int16_t speed = (int16_t)((held_only != 0U) ? TARGET_HOLD_SPEED : TRACK_SPEED);
  int16_t inner = Fight_ScalePwm((uint16_t)speed, TRACK_ARC_INNER_PERCENT);
  int16_t turn_speed =
    (int16_t)((held_only != 0U) ? TARGET_HOLD_SPEED : TURN_SPEED);

  if (attack != 0U)
  {
    speed = (int16_t)ATTACK_SPEED;
    inner = Fight_ScalePwm(ATTACK_SPEED, ATTACK_STEER_INNER_PERCENT);
    if (steering < 0)
    {
      return Fight_MakeCommand(FIGHT_CMD_ATTACK_ARC_LEFT, inner, speed);
    }
    if (steering > 0)
    {
      return Fight_MakeCommand(FIGHT_CMD_ATTACK_ARC_RIGHT, speed, inner);
    }
    return Fight_MakeCommand(FIGHT_CMD_ATTACK_FORWARD, speed, speed);
  }

  switch (region)
  {
    case TARGET_FRONT:
      if ((held_only == 0U) && (steering < 0))
      {
        return Fight_MakeCommand(FIGHT_CMD_TRACK_ARC_LEFT, inner, speed);
      }
      if ((held_only == 0U) && (steering > 0))
      {
        return Fight_MakeCommand(FIGHT_CMD_TRACK_ARC_RIGHT, speed, inner);
      }
      return Fight_MakeCommand(FIGHT_CMD_TRACK_FORWARD, speed, speed);

    case TARGET_FRONT_LEFT:
      return Fight_MakeCommand(FIGHT_CMD_TRACK_ARC_LEFT, inner, speed);

    case TARGET_FRONT_RIGHT:
      return Fight_MakeCommand(FIGHT_CMD_TRACK_ARC_RIGHT, speed, inner);

    case TARGET_LEFT:
    case TARGET_REAR_LEFT:
      return Fight_MakeCommand(FIGHT_CMD_TURN_LEFT,
                               (int16_t)(-turn_speed),
                               turn_speed);

    case TARGET_RIGHT:
    case TARGET_REAR_RIGHT:
      return Fight_MakeCommand(FIGHT_CMD_TURN_RIGHT,
                               turn_speed,
                               (int16_t)(-turn_speed));

    case TARGET_REAR:
      if (g_fight.last_turn_direction < 0)
      {
        return Fight_MakeCommand(FIGHT_CMD_TURN_LEFT,
                                 (int16_t)(-turn_speed),
                                 turn_speed);
      }
      return Fight_MakeCommand(FIGHT_CMD_TURN_RIGHT,
                               turn_speed,
                               (int16_t)(-turn_speed));

    case TARGET_NONE:
    default:
      return Fight_MakeCommand(FIGHT_CMD_STOP, 0, 0);
  }
}

static int8_t Fight_EdgeTurnDirection(uint8_t edge_mask)
{
  uint8_t left_edges = edge_mask &
    (SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_REAR_LEFT);
  uint8_t right_edges = edge_mask &
    (SENSOR_EDGE_FRONT_RIGHT | SENSOR_EDGE_REAR_RIGHT);

  if ((left_edges != 0U) && (right_edges == 0U))
  {
    return 1;
  }
  if ((right_edges != 0U) && (left_edges == 0U))
  {
    return -1;
  }
  return (g_fight.last_turn_direction == 0) ? 1 : g_fight.last_turn_direction;
}

static FightMotionCommand Fight_CommandForEdge(uint8_t edge_mask)
{
  int16_t speed = (int16_t)EDGE_ESCAPE_SPEED;
  uint8_t front_count = 0U;
  uint8_t rear_count = 0U;

  if ((edge_mask & SENSOR_EDGE_FRONT_LEFT) != 0U)
  {
    front_count++;
  }
  if ((edge_mask & SENSOR_EDGE_FRONT_RIGHT) != 0U)
  {
    front_count++;
  }
  if ((edge_mask & SENSOR_EDGE_REAR_LEFT) != 0U)
  {
    rear_count++;
  }
  if ((edge_mask & SENSOR_EDGE_REAR_RIGHT) != 0U)
  {
    rear_count++;
  }

  /* Edge V2 deliberately avoids per-sensor left/right arc switching.
     At a corner that old policy could alternate FL/FR (or left/right side)
     every few milliseconds and make the robot shake its head in place.

     First priority is to create real longitudinal clearance:
       any front-only danger -> reverse straight;
       any rear-only danger  -> forward straight.
     If danger exists at both front and rear, translating is ambiguous, so
     keep one turn direction locked for the whole edge episode. */
  if ((front_count != 0U) && (rear_count == 0U))
  {
    return Fight_MakeCommand(FIGHT_CMD_EDGE_BACK,
                             (int16_t)(-speed),
                             (int16_t)(-speed));
  }
  if ((rear_count != 0U) && (front_count == 0U))
  {
    return Fight_MakeCommand(FIGHT_CMD_EDGE_FORWARD, speed, speed);
  }

  if (g_fight.recover_turn_direction < 0)
  {
    return Fight_MakeCommand(FIGHT_CMD_EDGE_TURN_LEFT,
                             (int16_t)(-speed),
                             speed);
  }
  return Fight_MakeCommand(FIGHT_CMD_EDGE_TURN_RIGHT,
                           speed,
                           (int16_t)(-speed));
}

static FightMotionCommand Fight_CommandForRecover(void)
{
  int16_t speed = (int16_t)EDGE_REORIENT_SPEED;

  if (g_fight.recover_turn_direction < 0)
  {
    return Fight_MakeCommand(FIGHT_CMD_RECOVER_TURN,
                             (int16_t)(-speed),
                             speed);
  }
  return Fight_MakeCommand(FIGHT_CMD_RECOVER_TURN,
                           speed,
                           (int16_t)(-speed));
}

static void Fight_EnterEdge(uint8_t edge_mask, uint32_t now_ms)
{
  Fight_SetState(FIGHT_EDGE_ESCAPE, now_ms);
  g_fight.edge_phase = EDGE_PHASE_PUSH;
  g_fight.edge_hard_start_ms = now_ms;
  g_fight.edge_retry_start_ms = now_ms;
  g_fight.edge_trigger_mask = edge_mask;
  g_fight.edge_safe_count = 0U;
  g_fight.edge_retry_count = 0U;
  g_fight.edge_timeout_fault = 0U;
  g_fight.restage_request = 0U;
  g_fight.recover_turn_direction = Fight_EdgeTurnDirection(edge_mask);
}

static void Fight_HandleEdge(const SensorSnapshot *snapshot)
{
  uint32_t now_ms = snapshot->tick_ms;
  uint32_t hard_elapsed_ms = now_ms - g_fight.edge_hard_start_ms;
  uint32_t retry_elapsed_ms = now_ms - g_fight.edge_retry_start_ms;
  uint8_t danger_mask = snapshot->edge_raw_mask;
  uint8_t command_mask;

  if (danger_mask != 0U)
  {
    /* Keep edge_trigger_mask latched from entry so the escape direction does
       not ping-pong with every raw-mask change. */
    g_fight.edge_safe_count = 0U;
    g_fight.edge_phase = EDGE_PHASE_PUSH;

    if (retry_elapsed_ms >= EDGE_ESCAPE_MAX_MS)
    {
      if (g_fight.edge_retry_count < EDGE_ESCAPE_RETRY_LIMIT)
      {
        g_fight.edge_retry_count++;
        g_fight.edge_retry_start_ms = now_ms;
        g_fight.edge_phase = EDGE_PHASE_RETRY;
      }
      else
      {
        g_fight.edge_timeout_fault = 1U;
        g_fight.edge_phase = EDGE_PHASE_TIMEOUT;
      }
    }
  }
  else
  {
    if (g_fight.edge_safe_count < EDGE_SAFE_STABLE_CYCLES)
    {
      g_fight.edge_safe_count++;
    }
    g_fight.edge_phase = EDGE_PHASE_SAFE_CONFIRM;
  }

  if (hard_elapsed_ms >= EDGE_ESCAPE_HARD_LIMIT_MS)
  {
    /* Never latch into a permanent STOP. A truly persistent/stuck edge gets
       another reorientation attempt in the opposite direction. */
    g_fight.edge_timeout_fault = 1U;
    g_fight.restage_request = 0U;
    g_fight.edge_phase = EDGE_PHASE_RETRY;
    g_fight.edge_hard_start_ms = now_ms;
    g_fight.edge_retry_start_ms = now_ms;
    g_fight.edge_retry_count = 0U;
    g_fight.recover_turn_direction =
      (int8_t)(-g_fight.recover_turn_direction);
    g_fight.command = Fight_CommandForRecover();
    return;
  }

  /* If the same edge episode survives the initial clearance pulse, stop
     pushing linearly against the boundary. Pivot in the direction chosen at
     edge entry until the sensors clear. This is the anti-corner-oscillation
     path and also breaks the "rear edge -> forward into wall forever" loop. */
  if ((danger_mask != 0U) &&
      (hard_elapsed_ms >= EDGE_ESCAPE_STUCK_TURN_MS))
  {
    g_fight.edge_phase = EDGE_PHASE_RETRY;
    g_fight.command = Fight_CommandForRecover();
    return;
  }

  if ((danger_mask == 0U) &&
      (hard_elapsed_ms >= EDGE_ESCAPE_MIN_MS) &&
      (g_fight.edge_safe_count >= EDGE_SAFE_STABLE_CYCLES))
  {
    Fight_SetState(FIGHT_RECOVER, now_ms);
    g_fight.search_primary_direction = g_fight.recover_turn_direction;
    g_fight.search_seed_from_edge = 1U;
    g_fight.command = Fight_CommandForRecover();
    return;
  }

  command_mask = (danger_mask != 0U) ? danger_mask : g_fight.edge_trigger_mask;
  g_fight.command = Fight_CommandForEdge(command_mask);
}

static void Fight_HandleTarget(TargetRegion target,
                               const TargetObservation *observation,
                               uint32_t now_ms)
{
  uint8_t front_confident =
    ((target == TARGET_FRONT) &&
     (observation->region == TARGET_FRONT) &&
     (observation->front_attack_confident != 0U)) ? 1U : 0U;

  Fight_UpdateTurnPreference(target);
  g_fight.search_seed_from_edge = 0U;

  if (g_fight.target_held != 0U)
  {
    g_fight.attack_enter_count = 0U;
    g_fight.attack_exit_count = 0U;
    Fight_SetState(FIGHT_TRACK, now_ms);
    g_fight.command = Fight_CommandForTarget(target,
                                             1U,
                                             0U,
                                             observation->steering);
    return;
  }

  if (g_fight.state == FIGHT_ATTACK)
  {
    if (front_confident != 0U)
    {
      g_fight.attack_exit_count = 0U;
      g_fight.command = Fight_CommandForTarget(target,
                                               0U,
                                               1U,
                                               observation->steering);
      return;
    }

    if (g_fight.attack_exit_count < TARGET_ATTACK_EXIT_CYCLES)
    {
      g_fight.attack_exit_count++;
    }
    if (g_fight.attack_exit_count >= TARGET_ATTACK_EXIT_CYCLES)
    {
      Fight_SetState(FIGHT_TRACK, now_ms);
      g_fight.attack_exit_count = 0U;
    }
    g_fight.command = Fight_CommandForTarget(target,
                                             0U,
                                             0U,
                                             observation->steering);
    return;
  }

  g_fight.attack_exit_count = 0U;
  if (front_confident != 0U)
  {
    if (g_fight.attack_enter_count < TARGET_ATTACK_ENTER_CYCLES)
    {
      g_fight.attack_enter_count++;
    }
  }
  else
  {
    g_fight.attack_enter_count = 0U;
  }

  if (g_fight.attack_enter_count >= TARGET_ATTACK_ENTER_CYCLES)
  {
    Fight_SetState(FIGHT_ATTACK, now_ms);
    g_fight.command = Fight_CommandForTarget(target,
                                             0U,
                                             1U,
                                             observation->steering);
  }
  else
  {
    Fight_SetState(FIGHT_TRACK, now_ms);
    g_fight.command = Fight_CommandForTarget(target,
                                             0U,
                                             0U,
                                             observation->steering);
  }
}

static void Fight_HandleNoTarget(uint32_t now_ms)
{
  g_fight.attack_enter_count = 0U;
  g_fight.attack_exit_count = 0U;

  if (g_fight.state == FIGHT_RECOVER)
  {
    if ((uint32_t)(now_ms - g_fight.state_enter_ms) < EDGE_REORIENT_MS)
    {
      g_fight.command = Fight_CommandForRecover();
      return;
    }
    Fight_StartSearch(now_ms);
  }
  else if (g_fight.state != FIGHT_SEARCH)
  {
    Fight_StartSearch(now_ms);
  }

  Fight_AdvanceSearch(now_ms);
  g_fight.command = Fight_CommandForSearch();
}

static void Fight_UpdateDebug(const SensorSnapshot *snapshot)
{
#if FIGHT_DEBUG
  FightDebugFrame frame;

  if (((uint32_t)(snapshot->tick_ms - g_fight.debug_last_publish_ms) <
       FIGHT_DEBUG_PUBLISH_PERIOD_MS) &&
      (g_fight.edge_timeout_fault == 0U) &&
      (g_fight.restage_request == 0U))
  {
    return;
  }
  g_fight.debug_last_publish_ms = snapshot->tick_ms;

  frame.tick_ms = snapshot->tick_ms;
  frame.snapshot_count = snapshot->snapshot_count;
  frame.range1_raw = snapshot->range_raw[0];
  frame.range2_raw = snapshot->range_raw[1];
  frame.range3_raw = snapshot->range_raw[2];
  frame.range4_raw = snapshot->range_raw[3];
  frame.range5_raw = snapshot->range5_raw;
  frame.range1_filtered = snapshot->range_filtered[0];
  frame.range2_filtered = snapshot->range_filtered[1];
  frame.range3_filtered = snapshot->range_filtered[2];
  frame.range4_filtered = snapshot->range_filtered[3];
  frame.adc1_frame_count = snapshot->adc1_frame_count;
  frame.adc1_error_count = snapshot->adc1_error_count;
  frame.adc1_age_ms = snapshot->adc1_age_ms;
  frame.adc2_error_count = snapshot->adc2_error_count;
  frame.adc2_age_ms = snapshot->adc2_age_ms;
  frame.enemy_raw_mask = snapshot->enemy_raw_mask;
  frame.enemy_filtered_mask = snapshot->enemy_filtered_mask;
  frame.analog_target_mask = snapshot->analog_target_mask;
  frame.edge_raw_mask = snapshot->edge_raw_mask;
  frame.adc1_valid = snapshot->adc1_valid;
  frame.adc2_valid = snapshot->adc2_valid;
  frame.observed_target = g_fight.observed_target;
  frame.target_region = g_fight.target_region;
  frame.last_target_region = g_fight.last_target_region;
  frame.target_score = g_fight.target_score;
  frame.target_held = g_fight.target_held;
  frame.fight_state = g_fight.state;
  frame.search_phase = g_fight.search_phase;
  frame.edge_phase = g_fight.edge_phase;
  frame.command_id = g_fight.command.id;
  frame.left_command = g_fight.command.left_pwm;
  frame.right_command = g_fight.command.right_pwm;
  frame.edge_retry_count = g_fight.edge_retry_count;
  frame.edge_timeout_fault = g_fight.edge_timeout_fault;
  frame.restage_request = g_fight.restage_request;
  frame.active = g_fight.active;

  g_fight_debug = frame;
#else
  (void)snapshot;
#endif
}

void Fight_Enter(void)
{
  FightDebugFrame empty_debug = {0};
  uint32_t now_ms = HAL_GetTick();

  g_fight.active = 1U;
  g_fight.state = FIGHT_SEARCH;
  g_fight.state_enter_ms = now_ms;
  g_fight.last_snapshot_count = 0xFFFFFFFFU;
  g_fight.observed_target = TARGET_NONE;
  g_fight.target_region = TARGET_NONE;
  g_fight.candidate_region = TARGET_NONE;
  g_fight.last_target_region = TARGET_NONE;
  g_fight.last_target_seen_ms = 0U;
  g_fight.candidate_count = 0U;
  g_fight.target_score = 0U;
  g_fight.target_held = 0U;
  g_fight.attack_enter_count = 0U;
  g_fight.attack_exit_count = 0U;
  g_fight.last_turn_direction = 1;
  g_fight.search_phase = SEARCH_SPIN_PRIMARY;
  g_fight.search_phase_enter_ms = now_ms;
  g_fight.search_primary_direction = 1;
  g_fight.search_seed_from_edge = 0U;
  g_fight.edge_phase = EDGE_PHASE_PUSH;
  g_fight.edge_hard_start_ms = 0U;
  g_fight.edge_retry_start_ms = 0U;
  g_fight.edge_trigger_mask = 0U;
  g_fight.edge_safe_count = 0U;
  g_fight.edge_retry_count = 0U;
  g_fight.edge_timeout_fault = 0U;
  g_fight.restage_request = 0U;
  g_fight.recover_turn_direction = 1;
  g_fight.command = Fight_MakeCommand(FIGHT_CMD_STOP, 0, 0);
  g_fight.debug_last_publish_ms = now_ms - FIGHT_DEBUG_PUBLISH_PERIOD_MS;

  g_fight_debug = empty_debug;
  Car_ApplyFightCommand(&g_fight.command);
}

void Fight_Exit(void)
{
  g_fight.active = 0U;
  g_fight.target_region = TARGET_NONE;
  g_fight.candidate_region = TARGET_NONE;
  g_fight.last_target_region = TARGET_NONE;
  g_fight.target_held = 0U;
  g_fight.search_seed_from_edge = 0U;
  g_fight.restage_request = 0U;
  g_fight.command = Fight_MakeCommand(FIGHT_CMD_STOP, 0, 0);
  Car_ApplyFightCommand(&g_fight.command);
}

void Fight_Update(const SensorSnapshot *snapshot)
{
  TargetObservation observation;
  TargetRegion target;

  if ((snapshot == NULL) || (g_fight.active == 0U))
  {
    return;
  }
  if (snapshot->snapshot_count == g_fight.last_snapshot_count)
  {
    return;
  }
  g_fight.last_snapshot_count = snapshot->snapshot_count;

    /* 绝对优先级：原始边缘危险信号会在同一个 5 ms 控制快照内抢占
       SEARCH/TRACK/ATTACK；该路径不会再执行目标处理或搜索代码。 */
  if (snapshot->edge_raw_mask != 0U)
  {
    if (g_fight.state != FIGHT_EDGE_ESCAPE)
    {
      Fight_EnterEdge(snapshot->edge_raw_mask, snapshot->tick_ms);
    }
    Fight_HandleEdge(snapshot);
  }
  else if (g_fight.state == FIGHT_EDGE_ESCAPE)
  {
    Fight_HandleEdge(snapshot);
  }
  else if ((g_fight.state == FIGHT_RECOVER) &&
           ((uint32_t)(snapshot->tick_ms - g_fight.state_enter_ms) <
            EDGE_REORIENT_MS))
  {
    /* Recovery is safety-critical. Do not let a freshly reacquired opponent
       cancel the short centerward reorientation and drag the robot straight
       back into the same corner. */
    g_fight.command = Fight_CommandForRecover();
  }
  else
  {
    observation = Fight_FuseTarget(snapshot);
    target = Fight_UpdateTargetLock(&observation, snapshot->tick_ms);

    if (target != TARGET_NONE)
    {
      Fight_HandleTarget(target, &observation, snapshot->tick_ms);
    }
    else
    {
      Fight_HandleNoTarget(snapshot->tick_ms);
    }
  }

  Car_ApplyFightCommand(&g_fight.command);
  Fight_UpdateDebug(snapshot);
}

const FightMotionCommand *Fight_GetMotionCommand(void)
{
  return &g_fight.command;
}

FightState Fight_GetState(void)
{
  return g_fight.state;
}

TargetRegion Fight_GetTargetRegion(void)
{
  return g_fight.target_region;
}

uint8_t Fight_ConsumeRestageRequest(void)
{
  uint8_t request = g_fight.restage_request;

  g_fight.restage_request = 0U;
  return request;
}

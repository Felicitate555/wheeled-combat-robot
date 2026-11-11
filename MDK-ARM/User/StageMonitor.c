#include "StageMonitor.h"

typedef struct
{
  StageMonitorState state;
  uint32_t last_snapshot_count;

  StageHistorySample history[STAGE_HISTORY_SAMPLE_COUNT];
  uint8_t history_head;
  uint8_t history_count;

  FightMotionCommand last_final_motion;
  uint8_t arm_safe_active;
  uint32_t arm_safe_start_ms;
  uint32_t last_safe_range5;

  uint32_t candidate_start_ms;
  uint32_t range5_baseline;
  uint32_t range5_min;
  uint32_t range5_max;
  uint8_t range5_departed_seen;
  uint8_t range5_transition_seen;
	/* Range5 连续离开主台面的时间证据 */
	uint8_t range5_offstage_active;
	uint32_t range5_offstage_start_ms;
	
  uint8_t first_edge;
  uint8_t first_seen_union;
  uint8_t coherent_pending_active;
  uint8_t coherent_pending_mask;
  uint32_t coherent_pending_start_ms;
  uint8_t coherent_single_mask;
  uint8_t first_coherent_exit_edge;
  uint32_t first_coherent_time_ms;
  FightMotionCommand pre_exit_motion;

  uint8_t edge_union;
  uint8_t last_edge;
  uint8_t propagation_pending_active;
  uint8_t propagation_pending_mask;
  uint32_t propagation_pending_start_ms;
  uint8_t edge_propagation_seen;

  uint8_t edge_clear_active;
  uint32_t edge_clear_start_ms;

  StageExitEvent event;
} StageMonitorContext;

static StageMonitorContext g_stage;
volatile StageMonitorDebug g_stage_debug;

static FightMotionCommand StageMonitor_StopMotion(void)
{
  FightMotionCommand motion;

  motion.id = FIGHT_CMD_STOP;
  motion.left_pwm = 0;
  motion.right_pwm = 0;
  return motion;
}

static FightMotionCommand StageMonitor_CopyMotion(
  const FightMotionCommand *motion)
{
  if (motion == NULL)
  {
    return StageMonitor_StopMotion();
  }
  return *motion;
}

static uint32_t StageMonitor_AbsDiff(uint32_t first, uint32_t second)
{
  return (first >= second) ? (first - second) : (second - first);
}

static uint8_t StageMonitor_Range5OnStage(
  const SensorSnapshot *snapshot)
{
  /*
   * ADC2 无效时不允许产生掉台判断。
   * 当前实车判据：
   * Range5 < RANGE5_STAGE_THRESHOLD = 主台面
   * Range5 >= RANGE5_STAGE_THRESHOLD = 非台面候选
   */
  if ((snapshot == NULL) || (snapshot->adc2_valid == 0U))
  {
    return 1U;
  }

  return (snapshot->range5_raw < RANGE5_STAGE_THRESHOLD) ? 1U : 0U;
}
static uint8_t StageMonitor_BitCount(uint8_t mask)
{
  uint8_t count = 0U;

  while (mask != 0U)
  {
    count = (uint8_t)(count + (mask & 1U));
    mask >>= 1U;
  }
  return count;
}

static StageExitDirection StageMonitor_DirectionFromFirstEdge(
  uint8_t edge_mask)
{
  const uint8_t front =
    SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_FRONT_RIGHT;
  const uint8_t rear =
    SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT;
  const uint8_t left =
    SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_REAR_LEFT;
  const uint8_t right =
    SENSOR_EDGE_FRONT_RIGHT | SENSOR_EDGE_REAR_RIGHT;

  if (edge_mask == front)
  {
    return STAGE_EXIT_FRONT;
  }
  if (edge_mask == rear)
  {
    return STAGE_EXIT_REAR;
  }
  if (edge_mask == left)
  {
    return STAGE_EXIT_LEFT;
  }
  if (edge_mask == right)
  {
    return STAGE_EXIT_RIGHT;
  }
  if (edge_mask == SENSOR_EDGE_FRONT_LEFT)
  {
    return STAGE_EXIT_SINGLE_FL;
  }
  if (edge_mask == SENSOR_EDGE_FRONT_RIGHT)
  {
    return STAGE_EXIT_SINGLE_FR;
  }
  if (edge_mask == SENSOR_EDGE_REAR_LEFT)
  {
    return STAGE_EXIT_SINGLE_RL;
  }
  if (edge_mask == SENSOR_EDGE_REAR_RIGHT)
  {
    return STAGE_EXIT_SINGLE_RR;
  }
  return STAGE_EXIT_UNKNOWN;
}

static uint8_t StageMonitor_CoalesceFirstEdge(uint8_t first_edge,
                                              uint8_t edge_union)
{
  const uint8_t pairs[] =
  {
    SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_FRONT_RIGHT,
    SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT,
    SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_REAR_LEFT,
    SENSOR_EDGE_FRONT_RIGHT | SENSOR_EDGE_REAR_RIGHT
  };
  uint8_t selected = 0U;
  uint8_t matches = 0U;
  uint32_t index;

  if ((StageMonitor_DirectionFromFirstEdge(first_edge) >= STAGE_EXIT_FRONT) &&
      (StageMonitor_DirectionFromFirstEdge(first_edge) <= STAGE_EXIT_RIGHT))
  {
    return first_edge;
  }
  if (StageMonitor_BitCount(first_edge) != 1U)
  {
    return first_edge;
  }

  for (index = 0U; index < (sizeof(pairs) / sizeof(pairs[0])); index++)
  {
    if (((pairs[index] & first_edge) != 0U) &&
        ((edge_union & pairs[index]) == pairs[index]))
    {
      selected = pairs[index];
      matches++;
    }
  }
  return (matches == 1U) ? selected : first_edge;
}

static uint8_t StageMonitor_PropagationTargetMask(
  StageExitDirection direction)
{
  switch (direction)
  {
    case STAGE_EXIT_FRONT:
      return SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT;
    case STAGE_EXIT_REAR:
      return SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_FRONT_RIGHT;
    case STAGE_EXIT_LEFT:
      return SENSOR_EDGE_FRONT_RIGHT | SENSOR_EDGE_REAR_RIGHT;
    case STAGE_EXIT_RIGHT:
      return SENSOR_EDGE_FRONT_LEFT | SENSOR_EDGE_REAR_LEFT;
    case STAGE_EXIT_SINGLE_FL:
      return SENSOR_EDGE_REAR_RIGHT;
    case STAGE_EXIT_SINGLE_FR:
      return SENSOR_EDGE_REAR_LEFT;
    case STAGE_EXIT_SINGLE_RL:
      return SENSOR_EDGE_FRONT_RIGHT;
    case STAGE_EXIT_SINGLE_RR:
      return SENSOR_EDGE_FRONT_LEFT;
    case STAGE_EXIT_UNKNOWN:
    default:
      return 0U;
  }
}

static void StageMonitor_RecordHistory(
  const SensorSnapshot *snapshot,
  const FightMotionCommand *final_motion)
{
  StageHistorySample *sample = &g_stage.history[g_stage.history_head];
  FightMotionCommand motion = StageMonitor_CopyMotion(final_motion);

  sample->tick_ms = snapshot->tick_ms;
  sample->range5_raw = snapshot->range5_raw;
  sample->edge_raw_mask = snapshot->edge_raw_mask;
  sample->left_pwm = motion.left_pwm;
  sample->right_pwm = motion.right_pwm;
  sample->fight_state = Fight_GetState();

  g_stage.history_head++;
  if (g_stage.history_head >= STAGE_HISTORY_SAMPLE_COUNT)
  {
    g_stage.history_head = 0U;
  }
  if (g_stage.history_count < STAGE_HISTORY_SAMPLE_COUNT)
  {
    g_stage.history_count++;
  }
}

static void StageMonitor_ResetCandidateEvidence(void)
{
  g_stage.candidate_start_ms = 0U;
  g_stage.range5_baseline = g_stage.last_safe_range5;
  g_stage.range5_min = g_stage.last_safe_range5;
  g_stage.range5_max = g_stage.last_safe_range5;
  g_stage.range5_departed_seen = 0U;
  g_stage.range5_transition_seen = 0U;
	
	g_stage.range5_offstage_active = 0U;
	g_stage.range5_offstage_start_ms = 0U;
	
  g_stage.first_edge = 0U;
  g_stage.first_seen_union = 0U;
  g_stage.coherent_pending_active = 0U;
  g_stage.coherent_pending_mask = 0U;
  g_stage.coherent_pending_start_ms = 0U;
  g_stage.coherent_single_mask = 0U;
  g_stage.first_coherent_exit_edge = 0U;
  g_stage.first_coherent_time_ms = 0U;
  g_stage.pre_exit_motion = StageMonitor_StopMotion();
  g_stage.edge_union = 0U;
  g_stage.last_edge = 0U;
  g_stage.propagation_pending_active = 0U;
  g_stage.propagation_pending_mask = 0U;
  g_stage.propagation_pending_start_ms = 0U;
  g_stage.edge_propagation_seen = 0U;
  g_stage.edge_clear_active = 0U;
  g_stage.edge_clear_start_ms = 0U;
}

static void StageMonitor_ReturnToArmed(const SensorSnapshot *snapshot)
{
  g_stage.last_safe_range5 = snapshot->range5_raw;
  StageMonitor_ResetCandidateEvidence();
  g_stage.state = STAGE_MONITOR_ARMED;
}

static void StageMonitor_ReturnToArming(void)
{
  StageMonitor_ResetCandidateEvidence();
  g_stage.arm_safe_active = 0U;
  g_stage.arm_safe_start_ms = 0U;
  g_stage.state = STAGE_MONITOR_ARMING;
}

static void StageMonitor_StartCandidate(
  const SensorSnapshot *snapshot,
  const FightMotionCommand *pre_exit_motion)
{
  g_stage.state = STAGE_MONITOR_EXIT_CANDIDATE;
  g_stage.candidate_start_ms = snapshot->tick_ms;
  g_stage.range5_baseline = g_stage.last_safe_range5;
  g_stage.range5_min = g_stage.last_safe_range5;
  g_stage.range5_max = g_stage.last_safe_range5;
  g_stage.range5_departed_seen = 0U;
  g_stage.range5_transition_seen = 0U;
	
	g_stage.range5_offstage_active = 0U;
	g_stage.range5_offstage_start_ms = 0U;
	
  g_stage.first_edge = snapshot->edge_raw_mask;
  g_stage.first_seen_union = snapshot->edge_raw_mask;
  g_stage.coherent_pending_active = 1U;
  g_stage.coherent_pending_mask = snapshot->edge_raw_mask;
  g_stage.coherent_pending_start_ms = snapshot->tick_ms;
  g_stage.coherent_single_mask = 0U;
  g_stage.first_coherent_exit_edge = 0U;
  g_stage.first_coherent_time_ms = 0U;
  g_stage.pre_exit_motion = StageMonitor_CopyMotion(pre_exit_motion);
  g_stage.edge_union = snapshot->edge_raw_mask;
  g_stage.last_edge = snapshot->edge_raw_mask;
  g_stage.propagation_pending_active = 0U;
  g_stage.edge_propagation_seen = 0U;
  g_stage.edge_clear_active = 0U;
}

static void StageMonitor_UpdateRangeEvidence(uint32_t range5)
{
  uint32_t baseline_delta;
  uint32_t range_span;

  if (range5 < g_stage.range5_min)
  {
    g_stage.range5_min = range5;
  }
  if (range5 > g_stage.range5_max)
  {
    g_stage.range5_max = range5;
  }
  baseline_delta = StageMonitor_AbsDiff(range5, g_stage.range5_baseline);
  range_span = g_stage.range5_max - g_stage.range5_min;

  if ((g_stage.range5_departed_seen == 0U) &&
      ((baseline_delta >= STAGE_RANGE5_EVENT_DELTA) ||
       (range_span >= STAGE_RANGE5_EVENT_DELTA)))
  {
    g_stage.range5_departed_seen = 1U;
  }
  else if ((g_stage.range5_departed_seen != 0U) &&
           (baseline_delta <= STAGE_RANGE5_RETURN_DELTA))
  {
    g_stage.range5_transition_seen = 1U;
  }
}

static void StageMonitor_LockFirstEdge(uint8_t edge_mask)
{
  if (g_stage.first_coherent_exit_edge != 0U)
  {
    return;
  }
  g_stage.first_coherent_exit_edge = edge_mask;
  g_stage.first_coherent_time_ms = g_stage.candidate_start_ms;
}

static void StageMonitor_UpdateFirstEdge(const SensorSnapshot *snapshot)
{
  uint8_t edge_mask = snapshot->edge_raw_mask;
	
  uint8_t coalesced;
  StageExitDirection direction;
	  /*
   * 新逻辑允许 Range5 先触发掉台候选。
   * 如果 Candidate 创建时没有 Edge，
   * 后续第一次出现 Edge 时把它记录成首个边缘方向证据。
   */
  if ((g_stage.first_edge == 0U) && (edge_mask != 0U))
  {
    g_stage.first_edge = edge_mask;
    g_stage.first_seen_union = edge_mask;
    g_stage.coherent_pending_active = 1U;
    g_stage.coherent_pending_mask = edge_mask;
    g_stage.coherent_pending_start_ms = snapshot->tick_ms;
  }
  if (g_stage.first_coherent_exit_edge != 0U)
  {
    return;
  }
  if ((edge_mask != 0U) &&
      ((uint32_t)(snapshot->tick_ms - g_stage.candidate_start_ms) <=
       STAGE_EDGE_PAIR_WINDOW_MS))
  {
    g_stage.first_seen_union |= edge_mask;
  }

  if (edge_mask == 0U)
  {
    g_stage.coherent_pending_active = 0U;
  }
  else if ((g_stage.coherent_pending_active == 0U) ||
           (edge_mask != g_stage.coherent_pending_mask))
  {
    g_stage.coherent_pending_active = 1U;
    g_stage.coherent_pending_mask = edge_mask;
    g_stage.coherent_pending_start_ms = snapshot->tick_ms;
  }
  else if ((uint32_t)(snapshot->tick_ms -
                      g_stage.coherent_pending_start_ms) >=
           STAGE_EDGE_COHERENT_MS)
  {
    coalesced = StageMonitor_CoalesceFirstEdge(g_stage.first_edge,
                                               g_stage.first_seen_union);
    direction = StageMonitor_DirectionFromFirstEdge(coalesced);
    if ((direction >= STAGE_EXIT_FRONT) && (direction <= STAGE_EXIT_RIGHT))
    {
      StageMonitor_LockFirstEdge(coalesced);
      return;
    }
    if (StageMonitor_BitCount(edge_mask) == 1U)
    {
      g_stage.coherent_single_mask = g_stage.first_edge;
    }
    else
    {
      StageMonitor_LockFirstEdge(edge_mask);
      return;
    }
  }

  if (((uint32_t)(snapshot->tick_ms - g_stage.candidate_start_ms) >=
       STAGE_EDGE_PAIR_WINDOW_MS) &&
      (g_stage.coherent_single_mask != 0U))
  {
    coalesced = StageMonitor_CoalesceFirstEdge(g_stage.coherent_single_mask,
                                               g_stage.first_seen_union);
    StageMonitor_LockFirstEdge(coalesced);
  }
}

static void StageMonitor_UpdatePropagation(const SensorSnapshot *snapshot)
{
  StageExitDirection direction;
  uint8_t target_mask;
  uint8_t live_target;

  if (g_stage.first_coherent_exit_edge == 0U)
  {
    return;
  }
  direction = StageMonitor_DirectionFromFirstEdge(
    g_stage.first_coherent_exit_edge);
  target_mask = StageMonitor_PropagationTargetMask(direction);
  live_target = snapshot->edge_raw_mask & target_mask;

  if (live_target == 0U)
  {
    g_stage.propagation_pending_active = 0U;
    return;
  }
  if ((g_stage.propagation_pending_active == 0U) ||
      (live_target != g_stage.propagation_pending_mask))
  {
    g_stage.propagation_pending_active = 1U;
    g_stage.propagation_pending_mask = live_target;
    g_stage.propagation_pending_start_ms = snapshot->tick_ms;
    return;
  }
  if ((uint32_t)(snapshot->tick_ms -
                 g_stage.propagation_pending_start_ms) >=
      STAGE_EDGE_PROPAGATION_MS)
  {
    g_stage.edge_propagation_seen = 1U;
  }
}

static void StageMonitor_LatchLost(StageConfidenceReason reason)
{
  g_stage.event.valid = 1U;
  g_stage.event.direction = StageMonitor_DirectionFromFirstEdge(
    g_stage.first_coherent_exit_edge);
  g_stage.event.first_edge = g_stage.first_edge;
  g_stage.event.coalesced_first_edge =
    g_stage.first_coherent_exit_edge;
  g_stage.event.edge_union = g_stage.edge_union;
  g_stage.event.last_edge = g_stage.last_edge;
  g_stage.event.pre_exit_motion = g_stage.pre_exit_motion;
 g_stage.event.exit_time =
  (g_stage.first_coherent_time_ms != 0U) ?
  g_stage.first_coherent_time_ms :
  g_stage.candidate_start_ms;
  g_stage.event.range5_transition_seen =
    g_stage.range5_transition_seen;
  g_stage.event.edge_propagation_seen =
    g_stage.edge_propagation_seen;
  g_stage.event.confidence_reason = reason;
  g_stage.state = STAGE_MONITOR_LOST_CONFIRMED;
}

static void StageMonitor_TryConfirmLost(const SensorSnapshot *snapshot)
{
  uint32_t event_age;
  uint32_t offstage_elapsed;
  uint32_t clear_elapsed;
  uint8_t range5_on_stage;

  event_age = snapshot->tick_ms - g_stage.candidate_start_ms;

  /*
   * ADC2 无效时，绝不使用 Range5 判断掉台。
   * 仍然保留 Edge 方向历史，但不允许确认 Lost。
   */
  if (snapshot->adc2_valid == 0U)
  {
    /* "Continuous off-stage" must mean continuously observed by fresh ADC2.
       Any stale/invalid interval breaks the 1500ms evidence window; otherwise
       a long ADC2 outage followed by one fresh OFF sample could immediately
       hard-confirm a false stage loss. */
    g_stage.range5_offstage_active = 0U;
    g_stage.range5_offstage_start_ms = 0U;
    g_stage.edge_clear_active = 0U;
    return;
  }

  range5_on_stage = StageMonitor_Range5OnStage(snapshot);

  /*
   * Range5 当前处于非台面。
   */
  if (range5_on_stage == 0U)
  {
    g_stage.edge_clear_active = 0U;

    if (g_stage.range5_offstage_active == 0U)
    {
      g_stage.range5_offstage_active = 1U;
      g_stage.range5_offstage_start_ms = snapshot->tick_ms;
      return;
    }

    offstage_elapsed =
      snapshot->tick_ms - g_stage.range5_offstage_start_ms;

    /*
     * 连续非台面达到 1500ms：
     * 强确认真正掉台。
     *
     * Edge 只负责方向信息，不再拥有单独确认 Lost 的权力。
     */
    if (offstage_elapsed >= STAGE_RANGE5_LOST_CONFIRM_MS)
    {
      StageMonitor_LatchLost(
        STAGE_CONFIDENCE_RANGE5_HARD_LOST);
      return;
    }

    return;
  }

  /*
   * Range5 当前重新看到了“台面”。
   * 如果之前确实经历了一段连续非台面，则根据持续时间判断。
   */
  if (g_stage.range5_offstage_active != 0U)
  {
    offstage_elapsed =
      snapshot->tick_ms - g_stage.range5_offstage_start_ms;

    g_stage.range5_offstage_active = 0U;
    g_stage.range5_offstage_start_ms = 0U;

    /*
     * <300ms：
     * 认为是正常探边 + Edge Escape 拉回。
     */
    if (offstage_elapsed < STAGE_RANGE5_SUSPECT_MS)
    {
      if (snapshot->edge_raw_mask == 0U)
      {
        StageMonitor_ReturnToArmed(snapshot);
      }
      return;
    }

    /*
     * 300~1500ms：
     * 不能确定是真的被救回主擂台，
     * 还是掉到了起始小方框。
     *
     * 离开 Fight，后续进入 VERIFY_GROUND 移动验证。
     */
    if (offstage_elapsed < STAGE_RANGE5_LOST_CONFIRM_MS)
    {
      StageMonitor_LatchLost(
        STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN);
      return;
    }

    /*
     * 理论上 >=1500ms 会在上面的 OFF 分支提前锁存，
     * 此处保留保险。
     */
    StageMonitor_LatchLost(
      STAGE_CONFIDENCE_RANGE5_HARD_LOST);
    return;
  }

  /*
   * 如果 Candidate 只是由 Edge 启动，
   * Range5 从头到尾都没有真正离开台面，
   * Edge 安全后一小段时间就取消候选。
   */
  if (snapshot->edge_raw_mask != 0U)
  {
    g_stage.edge_clear_active = 0U;
    return;
  }

  if (g_stage.edge_clear_active == 0U)
  {
    g_stage.edge_clear_active = 1U;
    g_stage.edge_clear_start_ms = snapshot->tick_ms;
    return;
  }

  clear_elapsed =
    snapshot->tick_ms - g_stage.edge_clear_start_ms;

  if (clear_elapsed >= STAGE_EVENT_DISMISS_MS)
  {
    StageMonitor_ReturnToArmed(snapshot);
    return;
  }

  /*
   * 防止异常 Candidate 永久存在。
   * 正常情况下 1500ms 判据会比这里更早结束。
   */
  if (event_age >= STAGE_EVENT_MAX_MS)
  {
    StageMonitor_ReturnToArming();
  }
}

static void StageMonitor_UpdateDebug(const SensorSnapshot *snapshot)
{
  StageMonitorDebug frame = {0};
  StageExitDirection direction = STAGE_EXIT_UNKNOWN;

  if (g_stage.first_coherent_exit_edge != 0U)
  {
    direction = StageMonitor_DirectionFromFirstEdge(
      g_stage.first_coherent_exit_edge);
  }
  if (g_stage.event.valid != 0U)
  {
    direction = g_stage.event.direction;
  }

  frame.tick_ms = snapshot->tick_ms;
  frame.snapshot_count = snapshot->snapshot_count;
  frame.candidate_age_ms =
    (g_stage.state == STAGE_MONITOR_EXIT_CANDIDATE) ?
    (snapshot->tick_ms - g_stage.candidate_start_ms) : 0U;
  frame.range5_baseline = g_stage.range5_baseline;
  frame.range5_min = g_stage.range5_min;
  frame.range5_max = g_stage.range5_max;
  frame.monitor_state = (uint8_t)g_stage.state;
  frame.armed = (g_stage.state == STAGE_MONITOR_ARMED) ? 1U : 0U;
  frame.candidate_active =
    (g_stage.state == STAGE_MONITOR_EXIT_CANDIDATE) ? 1U : 0U;
  frame.first_edge = g_stage.first_edge;
  frame.coalesced_first_edge = g_stage.first_coherent_exit_edge;
  frame.edge_union = g_stage.edge_union;
  frame.last_edge = g_stage.last_edge;
  frame.history_count = g_stage.history_count;
  frame.range5_transition_seen = g_stage.range5_transition_seen;
  frame.edge_propagation_seen = g_stage.edge_propagation_seen;
  frame.pre_exit_left_pwm = g_stage.pre_exit_motion.left_pwm;
  frame.pre_exit_right_pwm = g_stage.pre_exit_motion.right_pwm;
  frame.lost_confirmed =
    (g_stage.state == STAGE_MONITOR_LOST_CONFIRMED) ? 1U : 0U;
  frame.exit_direction = direction;
  frame.confidence_reason = g_stage.event.confidence_reason;
  frame.final_left_pwm = g_stage.last_final_motion.left_pwm;
  frame.final_right_pwm = g_stage.last_final_motion.right_pwm;
  g_stage_debug = frame;
}

void StageMonitor_EnterOnStage(const SensorSnapshot *snapshot)
{
  StageMonitorContext empty_context = {0};
  StageMonitorDebug empty_debug = {0};

  g_stage = empty_context;
  g_stage_debug = empty_debug;
  g_stage.state = STAGE_MONITOR_ARMING;
  g_stage.last_snapshot_count = 0xFFFFFFFFU;
  g_stage.last_final_motion = StageMonitor_StopMotion();
  g_stage.pre_exit_motion = StageMonitor_StopMotion();
  g_stage.event.pre_exit_motion = StageMonitor_StopMotion();

  if (snapshot != NULL)
  {
    g_stage.last_safe_range5 = snapshot->range5_raw;
    g_stage.range5_baseline = snapshot->range5_raw;
    g_stage.range5_min = snapshot->range5_raw;
    g_stage.range5_max = snapshot->range5_raw;
    if (snapshot->edge_raw_mask == 0U)
    {
      g_stage.arm_safe_active = 1U;
      g_stage.arm_safe_start_ms = snapshot->tick_ms;
    }
    StageMonitor_UpdateDebug(snapshot);
  }
}

void StageMonitor_Update(const SensorSnapshot *snapshot,
                         const FightMotionCommand *final_motion)
{
  FightMotionCommand pre_cycle_motion;

  if ((snapshot == NULL) || (g_stage.state == STAGE_MONITOR_OFF))
  {
    return;
  }
  if (snapshot->snapshot_count == g_stage.last_snapshot_count)
  {
    return;
  }
  g_stage.last_snapshot_count = snapshot->snapshot_count;
  pre_cycle_motion = g_stage.last_final_motion;
  StageMonitor_RecordHistory(snapshot, final_motion);
  g_stage.last_final_motion = StageMonitor_CopyMotion(final_motion);

  if (g_stage.state == STAGE_MONITOR_LOST_CONFIRMED)
  {
    StageMonitor_UpdateDebug(snapshot);
    return;
  }
  if (g_stage.state == STAGE_MONITOR_ARMING)
{
  if ((snapshot->edge_raw_mask == 0U) &&
      (snapshot->adc2_valid != 0U) &&
      (StageMonitor_Range5OnStage(snapshot) != 0U))
    {
      g_stage.last_safe_range5 = snapshot->range5_raw;
      if (g_stage.arm_safe_active == 0U)
      {
        g_stage.arm_safe_active = 1U;
        g_stage.arm_safe_start_ms = snapshot->tick_ms;
      }
      if ((uint32_t)(snapshot->tick_ms - g_stage.arm_safe_start_ms) >=
          STAGE_ARM_SAFE_MS)
      {
        g_stage.state = STAGE_MONITOR_ARMED;
        g_stage.arm_safe_active = 0U;
      }
    }
    else
    {
      g_stage.arm_safe_active = 0U;
    }
    StageMonitor_UpdateDebug(snapshot);
    return;
  }
 if (g_stage.state == STAGE_MONITOR_ARMED)
{
  uint8_t range5_on_stage =
    StageMonitor_Range5OnStage(snapshot);

  /*
   * Edge安全 + Range5确认仍在主台面：
   * 正常状态。
   */
  if ((snapshot->edge_raw_mask == 0U) &&
      (snapshot->adc2_valid != 0U) &&
      (range5_on_stage != 0U))
  {
    g_stage.last_safe_range5 = snapshot->range5_raw;
    StageMonitor_UpdateDebug(snapshot);
    return;
  }

  /*
   * 以下任一种情况都进入掉台候选：
   * 1. 四周任一 Edge 出现危险；
   * 2. Range5 已经离开台面。
   */
  StageMonitor_StartCandidate(snapshot, &pre_cycle_motion);
}

  if (snapshot->edge_raw_mask != 0U)
  {
    g_stage.edge_union |= snapshot->edge_raw_mask;
    g_stage.last_edge = snapshot->edge_raw_mask;
  }
  StageMonitor_UpdateRangeEvidence(snapshot->range5_raw);
  StageMonitor_UpdateFirstEdge(snapshot);
  StageMonitor_UpdatePropagation(snapshot);
  StageMonitor_TryConfirmLost(snapshot);
  StageMonitor_UpdateDebug(snapshot);
}

uint8_t StageMonitor_IsStageLostConfirmed(void)
{
  return (g_stage.state == STAGE_MONITOR_LOST_CONFIRMED) ? 1U : 0U;
}

const StageExitEvent *StageMonitor_GetExitEvent(void)
{
  return &g_stage.event;
}

StageMonitorState StageMonitor_GetState(void)
{
  return g_stage.state;
}

#ifndef FIGHT_CONFIG_H__
#define FIGHT_CONFIG_H__

/* 调度与调试 -------------------------------------------------------------- */
#define FIGHT_CONTROL_PERIOD_MS              5U
#define FIGHT_DEBUG                          1U
#define FIGHT_DEBUG_PUBLISH_PERIOD_MS       20U

/* 比赛现场保险：置 0 后 Mode_Finding 只执行 Fight；置 1 才启用掉台重登。 */
#ifndef ENABLE_STAGE_RESTAGE
#define ENABLE_STAGE_RESTAGE                  1
#endif

/* 掉台旁路监控 ------------------------------------------------------------ */
#define STAGE_MONITOR_HISTORY_MS            300U
#define STAGE_HISTORY_SAMPLE_COUNT           60U
#define STAGE_ARM_SAFE_MS                   100U
#define STAGE_EDGE_COHERENT_MS               10U
#define STAGE_EDGE_PAIR_WINDOW_MS            35U
#define STAGE_EDGE_PROPAGATION_MS            10U
#define STAGE_EDGE_SAFE_AFTER_EVENT_MS       30U
#define STAGE_EVENT_DISMISS_MS               60U

/* Range5 掉台时间判据：
 * <300ms：认为是正常探边 + Edge Escape 拉回；
 * 300~1500ms 后重新看到台面：离开 Fight，进入 VERIFY_GROUND 移动验证；
 * >=1500ms 连续非台面：强确认掉台。
 */
#define STAGE_RANGE5_SUSPECT_MS             300U
#define STAGE_RANGE5_LOST_CONFIRM_MS       1500U

/* 必须明显长于 1500ms 强确认窗口，避免候选事件提前被清理。 */
#define STAGE_EVENT_MAX_MS                 2500U

/* VERIFY_GROUND：向右前方弧线主动制造空间变化。
 * Range5 连续 ON 700ms 且 Edge 安全 -> 认为已经在主台；
 * Range5 连续 OFF 300ms -> 认为只是压到了起始小方框/仍在台下；
 * ADC2 无法给出有效证据时最多验证 2500ms，超时回 Fight，避免误重登。
 */
#define STAGE_VERIFY_SPEED                   60U
#define STAGE_VERIFY_ARC_INNER_PERCENT       70U
#define STAGE_VERIFY_ON_CONFIRM_MS          700U
#define STAGE_VERIFY_OFF_CONFIRM_MS         300U
#define STAGE_VERIFY_MAX_MS                2500U
/* VERIFY edge override must not push against the same boundary forever. */
#define STAGE_VERIFY_EDGE_PUSH_MAX_MS       180U
#define STAGE_VERIFY_EDGE_TURN_SPEED         55U

/* 旧的 Range5 波动参数保留给重登阶段的辅助证据。 */
#define STAGE_RANGE5_EVENT_DELTA            3000U
#define STAGE_RANGE5_RETURN_DELTA           1500U

/* ADC 数据新鲜度 ---------------------------------------------------------- */
#define ADC1_FRAME_STALE_MS                  10U
#define ADC2_FRAME_STALE_MS                  20U

#define ADC_RANGE_EMA_SHIFT                   2U
/* 需要实车标定：暂时保留旧工程的检测极性和标称阈值。 */
#define RANGE1_TARGET_ON                 17500U
#define RANGE1_TARGET_OFF                16500U
#define RANGE2_TARGET_ON                 17500U
#define RANGE2_TARGET_OFF                16500U
#define RANGE3_TARGET_ON                 22500U
#define RANGE3_TARGET_OFF                21500U
#define RANGE4_TARGET_ON                 17500U
#define RANGE4_TARGET_OFF                16500U
#define RANGE5_STAGE_THRESHOLD           37000U

/* 数字敌人输入积分滤波 ---------------------------------------------------- */
#define TARGET_DIGITAL_COUNTER_MAX           3U
#define TARGET_DIGITAL_ON_COUNT              2U
#define TARGET_DIGITAL_OFF_COUNT             0U

/* 目标融合与保持 ---------------------------------------------------------- */
#define TARGET_REGION_MIN_SCORE              3U
#define TARGET_REGION_SWITCH_MARGIN          2U
#define TARGET_REGION_SWITCH_CYCLES          2U
#define TARGET_ATTACK_ENTER_CYCLES           2U
#define TARGET_ATTACK_EXIT_CYCLES            2U
#define TARGET_HOLD_MS                       80U

/* 搜索时序 ---------------------------------------------------------------- */
#define SEARCH_SPIN_PRIMARY_MS              220U
#define SEARCH_ADVANCE_ARC_MS               180U
#define SEARCH_SPIN_REVERSE_MS              300U
#define SEARCH_ADVANCE_ARC_REVERSE_MS       180U

/* 边缘脱险时序 ------------------------------------------------------------ */
#define EDGE_SAFE_STABLE_CYCLES               4U
/* Edge V2: first create real clearance, then reorient. */
#define EDGE_ESCAPE_MIN_MS                  200U
#define EDGE_ESCAPE_STUCK_TURN_MS           260U
#define EDGE_REORIENT_MS                    160U
#define EDGE_ESCAPE_MAX_MS                  650U
#define EDGE_ESCAPE_HARD_LIMIT_MS          1500U
#define EDGE_ESCAPE_RETRY_LIMIT               1U

/* 电机 PWM：TIM2 周期为 100 个计数，因此可用占空比范围为 0～99。 */
#define MOTOR_PWM_MAX                        99U
#define SEARCH_SPEED                         38U
#define SEARCH_ARC_INNER_PERCENT             65U
#define TRACK_SPEED                          50U
#define TRACK_ARC_INNER_PERCENT              65U
#define TURN_SPEED                           50U
#define ATTACK_SPEED                         75U
#define ATTACK_STEER_INNER_PERCENT           70U
#define TARGET_HOLD_SPEED                    45U
/* 80 已由用户确认可用，本轮不回退。 */
#define EDGE_ESCAPE_SPEED                    80U
#define EDGE_ESCAPE_ARC_INNER_PERCENT        75U
#define EDGE_REORIENT_SPEED                  50U

/* 首次登台 ---------------------------------------------------------------- */
#define FIRST_STAGE_BACK_SPEED                75U
#define FIRST_STAGE_CONFIRM_SPEED             42U
#define FIRST_STAGE_BACK_MS                 2000U
#define FIRST_STAGE_EDGE_SAFE_CONFIRM_MS      40U
#define FIRST_STAGE_CONFIRM_MAX_MS            700U

/* 再次登台：不允许永久 FAILED_STOP；失败后无限循环尝试。 ---------------- */
#define RESTAGE_SETTLE_MS                    100U

/* 每一次后退寻找坡前都先前冲，再左转。
 * 第一次左转角度更大；后续尝试缩短左转时间，避免每次都大幅过转。
 */
#define RESTAGE_RETRY_FORWARD_SPEED           60U
#define RESTAGE_RETRY_FORWARD_MS             220U
#define RESTAGE_RETRY_LEFT_TURN_SPEED         45U
#define RESTAGE_RETRY_FIRST_LEFT_TURN_MS     420U
#define RESTAGE_RETRY_NEXT_LEFT_TURN_A_MS    180U
#define RESTAGE_RETRY_NEXT_LEFT_TURN_B_MS    220U
#define RESTAGE_RETRY_NEXT_LEFT_TURN_C_MS    140U

#define RESTAGE_SEARCH_SPEED                  50U
#define RESTAGE_SEARCH_TIMEOUT_MS           1800U
#define RESTAGE_REAR_BASELINE_MS              30U
#define RESTAGE_REAR_EVENT_STABLE_MS          10U

#define RESTAGE_CLIMB_SPEED                   75U
#define RESTAGE_CONFIRM_SPEED                 42U
#define RESTAGE_CLIMB_MIN_MS                 300U
#define RESTAGE_CLIMB_TIMEOUT_MS            2000U
#define RESTAGE_CLIMB_RANGE_STABLE_MS         20U
#define RESTAGE_CLIMB_EDGE_SAFE_MS            20U

/* 真正回到主台只由“新鲜 Range5 连续台面”完成最终确认；Edge 仅做辅助。 */
#define RESTAGE_STAGE_CONFIRM_MS             200U
#define RESTAGE_CONFIRM_TIMEOUT_MS           700U
#define RESTAGE_RAMP_STOP_MS                 100U

#endif /* FIGHT_CONFIG_H__ */

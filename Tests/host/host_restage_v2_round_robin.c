#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Car.h"
#include "Mode.h"
#include "TIME.h"

static SensorSnapshot g_snapshot;
static StageExitEvent g_event;
static FightMotionCommand g_motion;
static int16_t g_left, g_right;
static uint32_t g_seq;
static uint8_t g_stage_lost;

uint32_t HAL_GetTick(void) { return g_snapshot.tick_ms; }
GPIO_PinState HAL_GPIO_ReadPin(void *p, uint16_t pin)
{ (void)p; (void)pin; return GPIO_PIN_RESET; }
const SensorSnapshot *Sensor_GetSnapshot(void) { return &g_snapshot; }
void Fight_Enter(void) { memset(&g_motion,0,sizeof(g_motion)); }
void Fight_Exit(void) { memset(&g_motion,0,sizeof(g_motion)); g_left=0; g_right=0; }
void Fight_Update(const SensorSnapshot *s)
{ (void)s; g_motion.id=FIGHT_CMD_SEARCH_SPIN; g_motion.left_pwm=38; g_motion.right_pwm=-38; g_left=38; g_right=-38; }
const FightMotionCommand *Fight_GetMotionCommand(void) { return &g_motion; }
FightState Fight_GetState(void) { return FIGHT_SEARCH; }
TargetRegion Fight_GetTargetRegion(void) { return TARGET_NONE; }
uint8_t Fight_ConsumeRestageRequest(void) { return 0U; }
void StageMonitor_EnterOnStage(const SensorSnapshot *s) { (void)s; g_stage_lost=0U; }
void StageMonitor_Update(const SensorSnapshot *s,const FightMotionCommand *m) { (void)s;(void)m; }
uint8_t StageMonitor_IsStageLostConfirmed(void) { return g_stage_lost; }
const StageExitEvent *StageMonitor_GetExitEvent(void) { return &g_event; }
StageMonitorState StageMonitor_GetState(void) { return STAGE_MONITOR_LOST_CONFIRMED; }
void Car_Init(void) {}
void Car_SetDirection(unsigned char d)
{
  if (d==Car_Forward) {g_left=50;g_right=50;}
  else if (d==Car_Back) {g_left=-60;g_right=-60;}
  else if (d==Car_Lift) {g_left=-50;g_right=50;}
  else if (d==Car_Right) {g_left=50;g_right=-50;}
  else {g_left=0;g_right=0;}
}
void Car_SetBackSpeed(unsigned char s){g_left=-(int16_t)s;g_right=-(int16_t)s;}
void Car_SetForwardSpeed(unsigned char s){g_left=(int16_t)s;g_right=(int16_t)s;}
void Car_SetTurnSpeed(unsigned char d,unsigned char s)
{ if(d==Car_Lift){g_left=-(int16_t)s;g_right=(int16_t)s;} else if(d==Car_Right){g_left=(int16_t)s;g_right=-(int16_t)s;} else {g_left=0;g_right=0;} }
void Car_ApplyFightCommand(const FightMotionCommand *m){ if(m){g_left=m->left_pwm;g_right=m->right_pwm;} else {g_left=0;g_right=0;} }
void Car_GetSignedOutput(int16_t *l,int16_t *r){*l=g_left;*r=g_right;}
unsigned char Car_GetDirection(void){return Car_Stop;}
void TIME_Init(void){}
void TIME_SetTimingMs(unsigned char f,unsigned int c){(void)f;(void)c;}
unsigned char TIME_GetTimeOverFlag(unsigned char f){(void)f;return 0U;}
void TIME_ClearSetTime(unsigned char f){(void)f;}
void TIME_ClearAll(void){}
float TIME_GetADCValue(unsigned char i){(void)i;return (float)g_snapshot.range5_raw;}
void TIME_SetADCValue(unsigned char i,unsigned int v){(void)i;(void)v;}

static void fail(const char *msg)
{
  fprintf(stderr,"FAIL t=%lu state=%u pwm=%d/%d retry=%u: %s\n",
          (unsigned long)g_snapshot.tick_ms,
          (unsigned)g_restage_debug.restage_state,(int)g_left,(int)g_right,
          (unsigned)g_restage_debug.retry_attempt,msg);
  exit(1);
}
static void snap(uint32_t t,uint8_t edge,uint32_t r5,uint8_t valid)
{
  memset(&g_snapshot,0,sizeof(g_snapshot));
  g_snapshot.tick_ms=t;g_snapshot.snapshot_count=++g_seq;
  g_snapshot.edge_raw_mask=edge;g_snapshot.range5_raw=r5;g_snapshot.adc2_valid=valid;
}
static unsigned char step(uint32_t t,uint8_t edge,uint32_t r5,uint8_t valid)
{ snap(t,edge,r5,valid); return Mode_GoOnStageAgain(); }
static void begin(StageConfidenceReason reason)
{
  Mode_FightExit(); memset(&g_event,0,sizeof(g_event)); g_event.valid=1U;
  g_event.direction=STAGE_EXIT_UNKNOWN;g_event.confidence_reason=reason;g_seq=0U;
  (void)step(0U,0U,30000U,1U);
}

static void test_verify_on_returns_fight(void)
{
  uint32_t t; unsigned char r=0U;
  begin(STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN);
  if (g_restage_debug.restage_state!=MODE_RESTAGE_VERIFY_GROUND || g_left!=60 || g_right!=42)
    fail("verify did not start right-front 60/42 arc");
  for(t=5U;t<=705U;t+=5U) r=step(t,0U,30000U,1U);
  if(r==0U || g_left!=0 || g_right!=0) fail("700ms ON did not return Fight");
}

static uint32_t drive_verify_to_search(void)
{
  uint32_t t;
  begin(STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN);
  for(t=5U;t<=305U;t+=5U) (void)step(t,0U,50000U,1U);
  if (g_restage_debug.restage_state!=MODE_RESTAGE_SETTLE) fail("300ms OFF did not confirm ground");
  (void)step(405U,0U,50000U,1U);
  if (g_restage_debug.restage_state!=MODE_RESTAGE_RETRY_FORWARD || g_left!=60 || g_right!=60)
    fail("retry did not begin with forward pulse");
  (void)step(625U,0U,50000U,1U);
  if (g_restage_debug.restage_state!=MODE_RESTAGE_RETRY_LEFT_TURN || g_left!=-45 || g_right!=45)
    fail("forward pulse did not lead to left turn");
  (void)step(1045U,0U,50000U,1U);
  if (g_restage_debug.restage_state!=MODE_RESTAGE_SEARCH || g_left!=-50 || g_right!=-50)
    fail("first long left turn did not lead to reverse search");
  return 1045U;
}

static void test_retry_is_infinite_and_shorter(void)
{
  uint32_t search=drive_verify_to_search();
  uint32_t t=search+RESTAGE_SEARCH_TIMEOUT_MS;
  (void)step(t,0U,50000U,1U);
  if (g_restage_debug.restage_state!=MODE_RESTAGE_RETRY_FORWARD || g_left!=60) fail("search timeout did not retry forward");
  (void)step(t+RESTAGE_RETRY_FORWARD_MS,0U,50000U,1U);
  if (g_restage_debug.restage_state!=MODE_RESTAGE_RETRY_LEFT_TURN || g_left!=-45) fail("second retry no left turn");
  (void)step(t+RESTAGE_RETRY_FORWARD_MS+RESTAGE_RETRY_NEXT_LEFT_TURN_A_MS,0U,50000U,1U);
  if (g_restage_debug.restage_state!=MODE_RESTAGE_SEARCH || g_left!=-50) fail("second short turn did not search");
  /* Run several full search timeouts. There must never be a terminal stop state. */
  for (unsigned i=0;i<12;i++)
  {
    t += 3000U;
    (void)step(t,0U,50000U,1U);
    if (g_restage_debug.restage_state>MODE_RESTAGE_RAMP_RETREAT_STOP) fail("invalid terminal state");
  }
}

static void test_edge_vetoes_verify_on_but_not_off(void)
{
  uint32_t t; unsigned char r=0U;
  begin(STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN);
  for(t=5U;t<=900U;t+=5U) r=step(t,SENSOR_EDGE_FRONT_LEFT,30000U,1U);
  if(r!=0U) fail("dangerous Edge allowed ON verification to finish");
  if(g_left!=55 || g_right!=-55) fail("persistent verify Edge did not pivot away");
  for(;t<=t+300U;) { break; }
  /* fresh OFF remains primary proof even if Edge is noisy on the floor */
  for(t=905U;t<=1210U;t+=5U) (void)step(t,SENSOR_EDGE_FRONT_LEFT,50000U,1U);
  if(g_restage_debug.restage_state!=MODE_RESTAGE_SETTLE) fail("OFF proof was blocked by floor Edge noise");
}

static void test_verify_stale_adc2_never_accumulates(void)
{
  uint32_t t; unsigned char r=0U;
  begin(STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN);
  for(t=5U;t<=2495U;t+=5U) r=step(t,0U,30000U,0U);
  if(r!=0U) fail("stale ADC2 finished verification early");
  r=step(2500U,0U,30000U,0U);
  if(r==0U || g_left!=0 || g_right!=0) fail("inconclusive verify did not fail-open to Fight");
}


static void test_start_square_rear_pair_does_not_push_forever(void)
{
  uint32_t t;
  begin(STAGE_CONFIDENCE_RANGE5_SUSPECT_RETURN);
  /* Rear pair on the start square initially asks for a short forward escape. */
  for (t=5U; t<=175U; t+=5U)
  {
    (void)step(t, SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT, 30000U, 1U);
  }
  if (g_left != 80 || g_right != 80)
    fail("rear-pair verify did not start with forward escape pulse");

  /* Once the same edge persists for 180 ms, it must pivot left instead of
     continuing to ram the platform boundary. */
  (void)step(185U, SENSOR_EDGE_REAR_LEFT | SENSOR_EDGE_REAR_RIGHT, 30000U, 1U);
  if (g_left != -55 || g_right != 55)
    fail("start-square rear pair still pushes forward instead of left pivot");
}

int main(void)
{
  test_start_square_rear_pair_does_not_push_forever();
  test_verify_on_returns_fight();
  test_retry_is_infinite_and_shorter();
  test_edge_vetoes_verify_on_but_not_off();
  test_verify_stale_adc2_never_accumulates();
  puts("Blindspot12 Restage regression PASSED.");
  return 0;
}

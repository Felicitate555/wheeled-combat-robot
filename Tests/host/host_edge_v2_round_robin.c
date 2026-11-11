#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Car.h"
#include "Fight.h"

static int16_t L,R;
void Car_ApplyFightCommand(const FightMotionCommand *c){L=c?c->left_pwm:0;R=c?c->right_pwm:0;}
uint32_t HAL_GetTick(void){return 0;}
GPIO_PinState HAL_GPIO_ReadPin(void *p,uint16_t pin){(void)p;(void)pin;return GPIO_PIN_RESET;}
static void die(const char*m,const SensorSnapshot*s){fprintf(stderr,"FAIL %s t=%lu state=%u cmd=%u pwm=%d/%d\n",m,(unsigned long)s->tick_ms,(unsigned)Fight_GetState(),(unsigned)Fight_GetMotionCommand()->id,(int)L,(int)R);exit(1);}
static void step(SensorSnapshot *s,uint32_t t,uint8_t edge,uint8_t analog){memset(s,0,sizeof(*s));s->tick_ms=t;s->snapshot_count=t/5+1;s->edge_raw_mask=edge;s->analog_target_mask=analog;s->adc1_valid=1;s->adc2_valid=1;Fight_Update(s);}
int main(void){SensorSnapshot s;Fight_Enter();
 step(&s,5,SENSOR_EDGE_FRONT_LEFT,0); if(L!=-80||R!=-80)die("FL should reverse straight",&s);
 step(&s,200,SENSOR_EDGE_FRONT_RIGHT,0); if(L!=-80||R!=-80)die("front sensor swap should still reverse straight",&s);
 step(&s,270,SENSOR_EDGE_FRONT_RIGHT,0); if(!(L>0&&R<0))die("persistent corner should locked-turn right",&s);
 /* Clear for four cycles; EDGE_ESCAPE_MIN_MS is already satisfied. */
 step(&s,275,0,0); step(&s,280,0,0); step(&s,285,0,0); step(&s,290,0,0);
 if(Fight_GetState()!=FIGHT_RECOVER) die("did not enter recover",&s);
 /* Present strong front target during mandatory recover; command must remain a turn. */
 step(&s,295,0,SENSOR_ANALOG_A11|SENSOR_ANALOG_A1);
 if(!(L>0&&R<0)) die("target interrupted mandatory recover",&s);
 puts("Edge V2 targeted regression PASSED.");return 0;}

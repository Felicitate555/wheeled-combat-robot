#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Sensor.h"
#include "adc.h"

ADC_HandleTypeDef hadc1, hadc2;
DMA_HandleTypeDef hdma_adc1;
static uint32_t tick;
static uint32_t flags;
static uint32_t adc_value;
static unsigned starts, stops;
static ADC1_DMA_Frame adc1;

uint32_t HAL_GetTick(void){return tick;}
GPIO_PinState HAL_GPIO_ReadPin(void *p,uint16_t pin){(void)p;(void)pin;return GPIO_PIN_RESET;}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef*h,uint32_t a,uint32_t b){(void)h;(void)a;(void)b;return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef*h){(void)h;starts++;return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef*h){(void)h;stops++;return HAL_OK;}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef*h){(void)h;flags&=~ADC_FLAG_EOC;return adc_value;}
uint32_t Host_ADC_GetFlag(ADC_HandleTypeDef*h,uint32_t f){(void)h;return (flags&f)!=0U;}
void Host_ADC_ClearFlag(ADC_HandleTypeDef*h,uint32_t f){(void)h;flags&=~f;}
void ADC1_DMA_Service(void){}
uint8_t ADC1_DMA_ReadLatest(ADC1_DMA_Frame *f){*f=adc1;return adc1.valid;}
void TIME_SetADCValue(unsigned char i,unsigned int v){(void)i;(void)v;}

static void fail(const char*m)
{
 const SensorSnapshot*s=Sensor_GetSnapshot();
 fprintf(stderr,"FAIL t=%lu raw=%lu valid=%u age=%lu err=%lu: %s\n",
  (unsigned long)tick,(unsigned long)s->range5_raw,(unsigned)s->adc2_valid,
  (unsigned long)s->adc2_age_ms,(unsigned long)s->adc2_error_count,m);exit(1);
}

int main(void)
{
 const SensorSnapshot*s;
 memset(&adc1,0,sizeof(adc1)); adc1.valid=1U; adc1.last_update_ms=0U;
 for(int i=0;i<4;i++) adc1.range[i]=10000U+i;
 tick=0U; if(Sensor_Init()!=HAL_OK) fail("init");

 /* New EOC arrives before a publish. The same 5ms Snapshot must contain it. */
 tick=5U; adc1.last_update_ms=5U; adc1.frame_count=1U; adc_value=31000U; flags|=ADC_FLAG_EOC;
 if(Sensor_Service()==0U) fail("5ms publish missing");
 s=Sensor_GetSnapshot();
 if(s->range5_raw!=31000U || s->adc2_valid==0U || s->adc2_age_ms!=0U)
   fail("new ADC2 sample was not atomically published");

 /* OVR+EOC must not stamp preserved/old DR contents as a fresh sample. */
 tick=10U; adc1.last_update_ms=10U; adc1.frame_count++;
 adc_value=12345U; flags|=(ADC_FLAG_OVR|ADC_FLAG_EOC);
 (void)Sensor_Service();
 s=Sensor_GetSnapshot();
 if(s->range5_raw!=31000U) fail("OVR data was incorrectly published as fresh Range5");
 if(s->adc2_error_count==0U) fail("OVR was not counted");

 /* No more EOC: old value may be retained, but it must become invalid after 20ms. */
 for(tick=15U;tick<=30U;tick+=5U){adc1.last_update_ms=tick;adc1.frame_count++;(void)Sensor_Service();}
 s=Sensor_GetSnapshot();
 if(s->range5_raw!=31000U) fail("last raw should be retained for diagnostics");
 if(s->adc2_valid!=0U || s->adc2_age_ms<=20U) fail("stale ADC2 stayed valid");
 if(stops==0U) fail("stalled conversion was not restarted");

 /* A later fresh EOC must recover validity. */
 tick=35U;adc1.last_update_ms=35U;adc1.frame_count++;adc_value=28000U;flags|=ADC_FLAG_EOC;(void)Sensor_Service();
 s=Sensor_GetSnapshot();
 if(s->range5_raw!=28000U || s->adc2_valid==0U || s->adc2_age_ms!=0U) fail("ADC2 did not recover");
 if(starts<2U) fail("ADC2 was not restarted");
 puts("Blindspot12 Sensor ADC2 freshness/snapshot regression PASSED.");
 return 0;
}

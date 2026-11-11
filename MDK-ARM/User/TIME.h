#ifndef TIME_H__
#define TIME_H__

#define TIME_AllNum		8
#define TIME_Over		2

void TIME_Init(void);
void TIME_SetTimingMs(unsigned char BitFlag, unsigned int CountNum);
unsigned char TIME_GetTimeOverFlag(unsigned char BitFlag);
void TIME_ClearSetTime(unsigned char BitFlag);
void TIME_ClearAll(void);
float TIME_GetADCValue(unsigned char IRx);
void TIME_SetADCValue(unsigned char IRx, unsigned int Value);



#endif


#include "Timer.h"
#include "Arduino.h"

volatile bool timerFlg = false;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR onTime()
{
    portENTER_CRITICAL_ISR(&timerMux);
    timerFlg = true;
    portEXIT_CRITICAL_ISR(&timerMux);
}

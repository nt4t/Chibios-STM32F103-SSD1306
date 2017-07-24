/*
    ChibiOS - Copyright (C) 2006..2016 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "ch_test.h"
#include <stdlib.h>
#include "chprintf.h"
#include "ssd1306.h"
#include "stdio.h"
#include <string.h>

#define ADC_GRP1_NUM_CHANNELS   1
#define ADC_GRP1_BUF_DEPTH      8

#define ADC_GRP2_NUM_CHANNELS   8
#define ADC_GRP2_BUF_DEPTH      16

static adcsample_t samples1[ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH];
static adcsample_t samples2[ADC_GRP2_NUM_CHANNELS * ADC_GRP2_BUF_DEPTH];

/*
 * ADC streaming callback.
 */
float ad0in = 0;
float ad1in = 0;
uint16_t ion = 0;

char snum0[10];
char snum1[10];
char ionnum[10];

size_t nx = 0, ny = 0;
static void adccallback(ADCDriver *adcp, adcsample_t *buffer, size_t n) {

  (void)adcp;
  if (samples2 == buffer) {
    nx += n;
  }
  else {
    ny += n;
  }

  ion = samples2[6];
  ad0in = (float) samples2[0];
  ad1in = (float) samples2[1];
}

static void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {

  (void)adcp;
  (void)err;
}

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 8 samples of 1 channel, SW triggered.
 * Channels:    IN10.
 */
static const ADCConversionGroup adcgrpcfg1 = {
  FALSE,
  ADC_GRP1_NUM_CHANNELS,
  NULL,
  adcerrorcallback,
  0, 0,                         /* CR1, CR2 */
  ADC_SMPR1_SMP_AN10(ADC_SAMPLE_1P5),
  0,                            /* SMPR2 */
  ADC_SQR1_NUM_CH(ADC_GRP1_NUM_CHANNELS),
  0,                            /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN6)
};

/*
 * ADC conversion group.
 * Mode:        Continuous, 16 samples of 8 channels, SW triggered.
 * Channels:    IN10, IN11, IN10, IN11, IN10, IN11, Sensor, VRef.
 */
static const ADCConversionGroup adcgrpcfg2 = {
  TRUE,
  ADC_GRP2_NUM_CHANNELS,
  adccallback,
  adcerrorcallback,
  0, ADC_CR2_TSVREFE,           /* CR1, CR2 */
  ADC_SMPR1_SMP_AN11(ADC_SAMPLE_41P5) | ADC_SMPR1_SMP_AN10(ADC_SAMPLE_41P5) |
  ADC_SMPR1_SMP_SENSOR(ADC_SAMPLE_239P5) | ADC_SMPR1_SMP_VREF(ADC_SAMPLE_239P5),
  0,                            /* SMPR2 */
  ADC_SQR1_NUM_CH(ADC_GRP2_NUM_CHANNELS),
  ADC_SQR2_SQ8_N(ADC_CHANNEL_SENSOR) | ADC_SQR2_SQ7_N(ADC_CHANNEL_VREFINT),
  ADC_SQR3_SQ6_N(ADC_CHANNEL_IN11)   | ADC_SQR3_SQ5_N(ADC_CHANNEL_IN10) |
  ADC_SQR3_SQ4_N(ADC_CHANNEL_IN11)   | ADC_SQR3_SQ3_N(ADC_CHANNEL_IN10) |
  ADC_SQR3_SQ2_N(ADC_CHANNEL_IN7)   | ADC_SQR3_SQ1_N(ADC_CHANNEL_IN6)
};


BaseSequentialStream* chp = (BaseSequentialStream*) &SD2;

/*
 * Blinker thread.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;

  chRegSetThreadName("blinker");
  while (true) {
    palClearPad(GPIOC, 13);
    chThdSleepMilliseconds(50);
    palSetPad(GPIOC, 13);
    chThdSleepMilliseconds(50);
  }
}

static const I2CConfig i2ccfg = {
  OPMODE_I2C,
  400000,
  FAST_DUTY_CYCLE_2,
};

static const SSD1306Config ssd1306cfg = {
  &I2CD1,
  &i2ccfg,
  SSD1306_SAD_0X78,
};

static SSD1306Driver SSD1306D1;

static void __attribute__((unused)) delayUs(uint32_t val) {
  (void)val;
}

static void __attribute__((unused)) delayMs(uint32_t val) {
  chThdSleepMilliseconds(val);
}


static THD_WORKING_AREA(waOledDisplay, 512);
static __attribute__((noreturn)) THD_FUNCTION(OledDisplay, arg) {
  (void)arg;
  float fADCreading = 0.0;

  chRegSetThreadName("OledDisplay");

  ssd1306ObjectInit(&SSD1306D1);
  ssd1306Start(&SSD1306D1, &ssd1306cfg);

  ssd1306FillScreen(&SSD1306D1, 0x00);

  while (TRUE) {
//	ssd1306FillScreen(&SSD1306D1, 0x00);
    ssd1306GotoXy(&SSD1306D1, 0, 1);

    fADCreading = (ad0in / 4095) * 3.3;
//	sprintf(snum0, "PA6 %d", samples2[6]);
    sprintf(snum0, "PA6 %d.%.2d", (int16_t) fADCreading,(int8_t) ((fADCreading-(int16_t)fADCreading)*100.0));  // gives 2 decimal places
    ssd1306Puts(&SSD1306D1, snum0, &ssd1306_font_11x18, SSD1306_COLOR_WHITE);
    ssd1306UpdateScreen(&SSD1306D1);

    chThdSleepMilliseconds(100);
    fADCreading = (ad1in / 4095) * 3.3;
//	chsnprintf(&snum1[0],sizeof(snum1), "%02f", ad0in);
    sprintf(snum1, "PA7 %d.%.2d", (int16_t) fADCreading,(int8_t) ((fADCreading-(int16_t)fADCreading)*100.0));  // gives 2 decimal places

    ssd1306GotoXy(&SSD1306D1, 0, 19);
    ssd1306Puts(&SSD1306D1, snum1, &ssd1306_font_11x18, SSD1306_COLOR_WHITE);
    ssd1306UpdateScreen(&SSD1306D1);


    chThdSleepMilliseconds(100);
    fADCreading = ion;
    sprintf(ionnum, "VREF %d", ion);
//	sprintf(ionnum, "PA7 %d.%.2d", (int16_t) fADCreading,(int8_t) ((fADCreading-(int16_t)fADCreading)*100.0));  // gives 2 decimal places

    ssd1306GotoXy(&SSD1306D1, 0, 39);
    ssd1306Puts(&SSD1306D1, ionnum, &ssd1306_font_11x18, SSD1306_COLOR_WHITE);
    ssd1306UpdateScreen(&SSD1306D1);


    chThdSleepMilliseconds(100);
  }

  ssd1306Stop(&SSD1306D1);
}


/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  palSetPadMode(IOPORT2, 6, PAL_MODE_STM32_ALTERNATE_PUSHPULL);    // SCL
  palSetPadMode(IOPORT2, 7, PAL_MODE_STM32_ALTERNATE_PUSHPULL);    // SDA
  palSetPadMode(IOPORT3, 13, PAL_MODE_OUTPUT_PUSHPULL);            // LED on PC13

  palSetPadMode(IOPORT1, 6, PAL_MODE_INPUT_ANALOG);            // PA6 ADC IN
  palSetPadMode(IOPORT1, 7, PAL_MODE_INPUT_ANALOG);            // PA7 ADC IN

  chThdSleepMilliseconds(10);

  adcStart(&ADCD1, NULL);

  /*
   * Linear conversion.
   */
  adcConvert(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);
  chThdSleepMilliseconds(100);

  /*
   * Starts an ADC continuous conversion.
   */
  adcStartConversion(&ADCD1, &adcgrpcfg2, samples2, ADC_GRP2_BUF_DEPTH);

  /*
   * Activates the serial driver 1 using the driver default configuration.
   * PA9(TX) and PA10(RX) are routed to USART1.
   */
  sdStart(&SD2, NULL);

  /*
   * Creates the example thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO+1, Thread1, NULL);

// chprintf(chp, "Hello world\n\r");

  chThdCreateStatic(waOledDisplay, sizeof(waOledDisplay), NORMALPRIO, OledDisplay, NULL);


  while (true) {
    chThdSleepMilliseconds(500);
  }
}

#ifndef BAIDU_ASR_H
#define BAIDU_ASR_H

#include <Arduino.h>

String recognizeSpeechWithBaidu(String accessToken, uint8_t *audioData, int audioDataSize);

#endif // BAIDU_ASR_H

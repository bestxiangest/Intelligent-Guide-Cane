#include "baidu_asr.h"

#include "voice.h"

String recognizeSpeechWithBaidu(String accessToken, uint8_t *audioData, int audioDataSize)
{
  return baidu_voice_recognition(accessToken, audioData, audioDataSize);
}

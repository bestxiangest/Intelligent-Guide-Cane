#include "baidu_tts.h"

#include "voice.h"

bool speakTextWithBaidu(String accessToken, String text)
{
  return baiduTTS_Send(accessToken, text);
}

#include "baidu_tts.h"

#include "voice.h"

void speakTextWithBaidu(String accessToken, String text)
{
  baiduTTS_Send(accessToken, text);
}

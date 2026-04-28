#ifndef LOCAL_AUDIO_H
#define LOCAL_AUDIO_H

#include <Arduino.h>

bool initLocalAudioStorage();
bool playLocalAudioById(const String &audioId);

#endif // LOCAL_AUDIO_H

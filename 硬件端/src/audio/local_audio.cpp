#include "local_audio.h"

#include <FS.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

#include "../voice.h"

namespace
{
constexpr const char *kLocalAudioDir = "/audio";

bool storageMounted = false;
bool storageMountAttempted = false;

bool isValidAudioId(const String &audioId)
{
  if (audioId.length() == 0 || audioId.length() > 48)
  {
    return false;
  }

  for (size_t i = 0; i < audioId.length(); ++i)
  {
    char c = audioId.charAt(i);
    bool ok = (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' ||
              c == '-';
    if (!ok)
    {
      return false;
    }
  }
  return true;
}

bool ensureStorageMounted()
{
  if (storageMounted)
  {
    return true;
  }

  if (storageMountAttempted)
  {
    return false;
  }

  storageMountAttempted = true;
  storageMounted = LittleFS.begin(false);
  if (!storageMounted)
  {
    Serial.println("[LocalAudio] LittleFS mount failed; local audio fallback will use TTS.");
    Serial.println("[LocalAudio] If the log shows 'Corrupted dir pair', erase flash, upload firmware, then uploadfs.");
  }
  else
  {
    Serial.println("[LocalAudio] LittleFS mounted.");
  }
  return storageMounted;
}

uint8_t *allocateAudioBuffer(size_t size)
{
  uint8_t *buffer = static_cast<uint8_t *>(
      heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer == nullptr)
  {
    buffer = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_8BIT));
  }
  return buffer;
}

} // namespace

bool initLocalAudioStorage()
{
  return ensureStorageMounted();
}

bool playLocalAudioById(const String &audioId)
{
  if (audioId.length() == 0)
  {
    return false;
  }

  if (!isValidAudioId(audioId))
  {
    Serial.printf("[LocalAudio] Invalid audio id: %s\n", audioId.c_str());
    return false;
  }

  if (!ensureStorageMounted())
  {
    return false;
  }

  String path = String(kLocalAudioDir) + "/" + audioId + ".wav";
  if (!LittleFS.exists(path))
  {
    Serial.printf("[LocalAudio] File not found: %s\n", path.c_str());
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file)
  {
    Serial.printf("[LocalAudio] Failed to open: %s\n", path.c_str());
    return false;
  }

  size_t fileSize = file.size();
  if (fileSize == 0)
  {
    Serial.printf("[LocalAudio] Empty file: %s\n", path.c_str());
    file.close();
    return false;
  }

  uint8_t *buffer = allocateAudioBuffer(fileSize);
  if (buffer == nullptr)
  {
    Serial.printf("[LocalAudio] Not enough memory for %s (%u bytes)\n",
                  path.c_str(),
                  static_cast<unsigned>(fileSize));
    file.close();
    return false;
  }

  size_t bytesRead = file.read(buffer, fileSize);
  file.close();
  if (bytesRead != fileSize)
  {
    Serial.printf("[LocalAudio] Short read for %s: %u/%u bytes\n",
                  path.c_str(),
                  static_cast<unsigned>(bytesRead),
                  static_cast<unsigned>(fileSize));
    free(buffer);
    return false;
  }

  Serial.printf("[LocalAudio] Playing %s through %u Hz local output path (%u bytes)\n",
                path.c_str(),
                static_cast<unsigned>(LOCAL_AUDIO_PLAYBACK_SAMPLE_RATE),
                static_cast<unsigned>(fileSize));
  bool ok = playLocalAudioBuffer(buffer, fileSize);
  free(buffer);
  return ok;
}

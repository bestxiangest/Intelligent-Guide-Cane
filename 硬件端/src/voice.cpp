#include "voice.h"
#include "audio/local_audio.h"
#include "esp_heap_caps.h"
#include <Preferences.h>
#include <stdlib.h>
#include <time.h>

i2s_config_t i2sIn_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512};

const i2s_pin_config_t i2sIn_pin_config = {
    .bck_io_num = INMP441_SCK,
    .ws_io_num = INMP441_WS,
    .data_out_num = -1,
    .data_in_num = INMP441_SD};

i2s_config_t i2sOut_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 10,
    .dma_buf_len = 1024};

const i2s_pin_config_t i2sOut_pin_config = {
    .bck_io_num = MAX98357_BCLK,
    .ws_io_num = MAX98357_LRC,
    .data_out_num = MAX98357_DIN,
    .data_in_num = -1};

static uint32_t currentSpeakerSampleRate = 0;
static uint16_t currentSpeakerChannels = 0;

// 初始化I2S
void set_i2s()
{
  esp_err_t err;
  
  // 先卸载可能存在的I2S驱动
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_1);
  
  // 添加延迟确保驱动完全卸载
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // 初始化I2S输入 (麦克风)
  Serial.println("正在初始化I2S输入驱动...");
  err = i2s_driver_install(I2S_NUM_0, &i2sIn_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("I2S输入驱动安装失败: %s\n", esp_err_to_name(err));
    return;
  }
  
  err = i2s_set_pin(I2S_NUM_0, &i2sIn_pin_config);
  if (err != ESP_OK) {
    Serial.printf("I2S输入引脚配置失败: %s\n", esp_err_to_name(err));
    return;
  }
  Serial.println("I2S输入驱动初始化成功");
  
  // 添加延迟
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // 初始化I2S输出 (扬声器)
  Serial.println("正在初始化I2S输出驱动...");
  err = i2s_driver_install(I2S_NUM_1, &i2sOut_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("I2S输出驱动安装失败: %s\n", esp_err_to_name(err));
    return;
  }
  
  err = i2s_set_pin(I2S_NUM_1, &i2sOut_pin_config);
  if (err != ESP_OK) {
    Serial.printf("I2S输出引脚配置失败: %s\n", esp_err_to_name(err));
    return;
  }
  Serial.println("I2S输出驱动初始化成功");
  
  // 清空DMA缓冲区
  i2s_zero_dma_buffer(I2S_NUM_0);
  err = i2s_set_clk(I2S_NUM_1, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  if (err != ESP_OK) {
    Serial.printf("[音频播放] 播放器16k初始化失败: %s\n", esp_err_to_name(err));
    return;
  }
  currentSpeakerSampleRate = SAMPLE_RATE;
  currentSpeakerChannels = 1;
  Serial.println("[音频播放] 播放器固定初始化为16kHz mono");
  i2s_zero_dma_buffer(I2S_NUM_1);
  
  Serial.println("I2S音频系统初始化完成");
}

// 播放声音
// audioData: PCM原始数据的指针
// audioDataSize: size of PCM data
void playAudio(uint8_t *audioData, size_t audioDataSize)
{
  if (audioDataSize > 0)
  {
    size_t bytes_written = 0;
    size_t total_written = 0;
    
    // 确保所有数据都被写入
    while (total_written < audioDataSize)
    {
      esp_err_t result = i2s_write(I2S_NUM_1, 
                                   (int16_t *)(audioData + total_written), 
                                   audioDataSize - total_written, 
                                   &bytes_written, 
                                   pdMS_TO_TICKS(100));
      
      if (result != ESP_OK)
      {
        Serial.printf("[音频播放] I2S写入错误: %s\n", esp_err_to_name(result));
        break;
      }
      
      total_written += bytes_written;
      
      // 如果没有写入任何数据，稍作延迟避免忙等待
      if (bytes_written == 0)
      {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }
}

// 清空I2S DMA缓冲区
static void playAudioStable(uint8_t *audioData, size_t audioDataSize)
{
  if (audioData == nullptr || audioDataSize == 0)
  {
    return;
  }

  constexpr size_t kI2sWriteChunkBytes = 2048;
  size_t totalWritten = 0;
  while (totalWritten < audioDataSize)
  {
    size_t chunkSize = audioDataSize - totalWritten;
    if (chunkSize > kI2sWriteChunkBytes)
    {
      chunkSize = kI2sWriteChunkBytes;
    }
    chunkSize &= ~static_cast<size_t>(0x01);
    if (chunkSize == 0)
    {
      break;
    }

    size_t bytesWritten = 0;
    esp_err_t result = i2s_write(I2S_NUM_1,
                                 audioData + totalWritten,
                                 chunkSize,
                                 &bytesWritten,
                                 portMAX_DELAY);
    if (result != ESP_OK)
    {
      Serial.printf("[音频播放] I2S写入错误: %s\n", esp_err_to_name(result));
      break;
    }

    totalWritten += bytesWritten;
    if (bytesWritten == 0)
    {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    taskYIELD();
  }
}

void clearAudio(void)
{
  // 清空I2S DMA缓冲区
  i2s_zero_dma_buffer(I2S_NUM_1);
  Serial.print("clearAudio");
}

struct WavPlaybackInfo
{
  size_t payloadOffset;
  size_t payloadLength;
  uint16_t channels;
  uint16_t bitsPerSample;
};

static bool configureSpeakerSampleRate(uint32_t sampleRate, uint16_t channels)
{
  const uint32_t targetSampleRate = sampleRate > 0 ? sampleRate : SAMPLE_RATE;
  const uint16_t targetChannels = 1;
  (void)channels;

  if (currentSpeakerSampleRate == targetSampleRate && currentSpeakerChannels == targetChannels)
  {
    return true;
  }

  esp_err_t err = i2s_set_clk(I2S_NUM_1, targetSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  if (err != ESP_OK)
  {
    Serial.printf("[语音合成] 错误: 设置扬声器采样率失败: %s\n", esp_err_to_name(err));
    return false;
  }

  currentSpeakerSampleRate = targetSampleRate;
  currentSpeakerChannels = targetChannels;
  Serial.printf("[音频播放] 播放器设置为 %lu Hz mono\n",
                static_cast<unsigned long>(targetSampleRate));
  return true;
}

static bool downloadHttpBody(HTTPClient &http, uint8_t **outBuffer, size_t *outLength)
{
  if (outBuffer == nullptr || outLength == nullptr)
  {
    return false;
  }

  *outBuffer = nullptr;
  *outLength = 0;

  WiFiClient *stream = http.getStreamPtr();
  if (stream == nullptr)
  {
    return false;
  }

  int contentLength = http.getSize();
  size_t capacity = contentLength > 0 ? static_cast<size_t>(contentLength) : 4096;
  uint8_t *buffer = static_cast<uint8_t *>(
      heap_caps_malloc(capacity + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer == nullptr)
  {
    buffer = static_cast<uint8_t *>(heap_caps_malloc(capacity + 1, MALLOC_CAP_8BIT));
  }
  if (buffer == nullptr)
  {
    Serial.println("[语音合成] 错误: 无法分配音频下载缓冲区");
    return false;
  }

  size_t totalRead = 0;
  unsigned long lastDataTime = millis();

  while ((http.connected() || stream->available()) && (contentLength > 0 || contentLength == -1))
  {
    size_t availableBytes = stream->available();
    if (availableBytes == 0)
    {
      if (millis() - lastDataTime > BAIDU_TTS_DOWNLOAD_IDLE_TIMEOUT_MS)
      {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    if (contentLength > 0 && availableBytes > static_cast<size_t>(contentLength))
    {
      availableBytes = static_cast<size_t>(contentLength);
    }

    if (totalRead + availableBytes > capacity)
    {
      size_t newCapacity = capacity;
      while (totalRead + availableBytes > newCapacity)
      {
        newCapacity *= 2;
      }
      uint8_t *newBuffer = static_cast<uint8_t *>(
          heap_caps_realloc(buffer, newCapacity + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
      if (newBuffer == nullptr)
      {
        newBuffer = static_cast<uint8_t *>(
            heap_caps_realloc(buffer, newCapacity + 1, MALLOC_CAP_8BIT));
      }
      if (newBuffer == nullptr)
      {
        free(buffer);
        Serial.println("[语音合成] 错误: 扩展音频缓冲区失败");
        return false;
      }
      buffer = newBuffer;
      capacity = newCapacity;
    }

    int bytesRead = stream->readBytes(buffer + totalRead, availableBytes);
    if (bytesRead <= 0)
    {
      break;
    }

    totalRead += static_cast<size_t>(bytesRead);
    lastDataTime = millis();

    if (contentLength > 0)
    {
      contentLength -= bytesRead;
    }
  }

  if (totalRead == 0)
  {
    free(buffer);
    Serial.println("[语音合成] 错误: 未下载到任何音频数据");
    return false;
  }

  buffer[totalRead] = '\0';
  *outBuffer = buffer;
  *outLength = totalRead;
  return true;
}

static bool getWavPlaybackInfo(const uint8_t *buffer, size_t bufferLength, WavPlaybackInfo *info)
{
  if (info == nullptr || buffer == nullptr || bufferLength < 12)
  {
    return false;
  }

  info->payloadOffset = 0;
  info->payloadLength = 0;
  info->channels = 1;
  info->bitsPerSample = 16;

  if (memcmp(buffer, "RIFF", 4) != 0 || memcmp(buffer + 8, "WAVE", 4) != 0)
  {
    info->payloadOffset = 0;
    info->payloadLength = bufferLength;
    return true;
  }

  size_t offset = 12;
  while (offset + 8 <= bufferLength)
  {
    const uint8_t *chunkId = buffer + offset;
    uint32_t chunkSize = static_cast<uint32_t>(buffer[offset + 4]) |
                         (static_cast<uint32_t>(buffer[offset + 5]) << 8) |
                         (static_cast<uint32_t>(buffer[offset + 6]) << 16) |
                         (static_cast<uint32_t>(buffer[offset + 7]) << 24);

    if (memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16 && offset + 8 + 16 <= bufferLength)
    {
      const uint8_t *fmt = buffer + offset + 8;
      uint16_t audioFormat = static_cast<uint16_t>(fmt[0]) |
                             (static_cast<uint16_t>(fmt[1]) << 8);
      info->channels = static_cast<uint16_t>(fmt[2]) |
                       (static_cast<uint16_t>(fmt[3]) << 8);
      info->bitsPerSample = static_cast<uint16_t>(fmt[14]) |
                            (static_cast<uint16_t>(fmt[15]) << 8);

      Serial.printf("[音频播放] WAV fmt: format=%u channels=%u bits=%u\n",
                    static_cast<unsigned>(audioFormat),
                    static_cast<unsigned>(info->channels),
                    static_cast<unsigned>(info->bitsPerSample));
    }
    else if (memcmp(chunkId, "data", 4) == 0)
    {
      size_t dataOffset = offset + 8;
      if (dataOffset > bufferLength)
      {
        return false;
      }

      size_t dataLength = min(static_cast<size_t>(chunkSize), bufferLength - dataOffset);
      info->payloadOffset = dataOffset;
      info->payloadLength = dataLength;
      return true;
    }

    offset += 8 + chunkSize + (chunkSize & 0x01);
  }

  return false;
}

static String buildBaiduTokenRequestBody()
{
  String body = "grant_type=client_credentials";
  body += "&client_id=" + urlEncode(String(BAIDU_CLIENT_ID));
  body += "&client_secret=" + urlEncode(String(BAIDU_CLIENT_SECRET));
  return body;
}

static uint64_t parseUint64String(const String &value)
{
  if (value.length() == 0)
  {
    return 0;
  }

  char *endPtr = nullptr;
  unsigned long long parsed = strtoull(value.c_str(), &endPtr, 10);
  if (endPtr == value.c_str())
  {
    return 0;
  }
  return static_cast<uint64_t>(parsed);
}

static uint64_t parseBaiduAccessTokenExpiresAt(const String &token)
{
  int firstDot = token.indexOf('.');
  int secondDot = firstDot >= 0 ? token.indexOf('.', firstDot + 1) : -1;
  int thirdDot = secondDot >= 0 ? token.indexOf('.', secondDot + 1) : -1;
  int fourthDot = thirdDot >= 0 ? token.indexOf('.', thirdDot + 1) : -1;
  if (thirdDot < 0 || fourthDot < 0)
  {
    return 0;
  }

  return parseUint64String(token.substring(thirdDot + 1, fourthDot));
}

static uint64_t getCurrentUnixTime()
{
  time_t now = time(nullptr);
  if (now < 1700000000)
  {
    return 0;
  }
  return static_cast<uint64_t>(now);
}

static bool isBaiduTokenFresh(const String &token, uint64_t expiresAt, const char *source)
{
  if (token.length() == 0)
  {
    return false;
  }

  if (expiresAt == 0)
  {
    expiresAt = parseBaiduAccessTokenExpiresAt(token);
  }

  if (expiresAt == 0)
  {
    Serial.printf("[voice] %s Baidu token has no parseable expiry; using it optimistically\n", source);
    return true;
  }

  uint64_t now = getCurrentUnixTime();
  if (now == 0)
  {
    Serial.printf("[voice] using %s Baidu token, expires_at=%llu (system time not synced)\n",
                  source,
                  static_cast<unsigned long long>(expiresAt));
    return true;
  }

  if (now + BAIDU_TOKEN_REFRESH_MARGIN_SEC >= expiresAt)
  {
    Serial.printf("[voice] %s Baidu token is stale, now=%llu expires_at=%llu\n",
                  source,
                  static_cast<unsigned long long>(now),
                  static_cast<unsigned long long>(expiresAt));
    return false;
  }

  Serial.printf("[voice] using %s Baidu token, now=%llu expires_at=%llu\n",
                source,
                static_cast<unsigned long long>(now),
                static_cast<unsigned long long>(expiresAt));
  return true;
}

static bool saveBaiduAccessTokenCache(const String &token, uint64_t expiresAt)
{
  if (token.length() == 0)
  {
    return false;
  }

  if (expiresAt == 0)
  {
    expiresAt = parseBaiduAccessTokenExpiresAt(token);
  }

  Preferences prefs;
  if (!prefs.begin("baidu_token", false))
  {
    Serial.println("[voice] failed to open Baidu token NVS namespace for writing");
    return false;
  }

  char expiresBuffer[24];
  snprintf(expiresBuffer, sizeof(expiresBuffer), "%llu",
           static_cast<unsigned long long>(expiresAt));

  size_t tokenBytes = prefs.putString("access", token);
  size_t expiresBytes = prefs.putString("expires_at", expiresBuffer);
  prefs.end();

  if (tokenBytes == 0)
  {
    Serial.println("[voice] failed to save Baidu token to NVS");
    return false;
  }

  Serial.printf("[voice] saved Baidu token to NVS, expires_at=%s, meta_bytes=%u\n",
                expiresBuffer,
                static_cast<unsigned>(expiresBytes));
  return true;
}

static String loadBaiduAccessTokenCache()
{
  Preferences prefs;
  if (!prefs.begin("baidu_token", true))
  {
    Serial.println("[voice] Baidu token NVS namespace is not ready");
    return "";
  }

  String token = prefs.getString("access", "");
  String expiresAtText = prefs.getString("expires_at", "");
  prefs.end();

  uint64_t expiresAt = parseUint64String(expiresAtText);
  if (!isBaiduTokenFresh(token, expiresAt, "cached"))
  {
    return "";
  }

  return token;
}

static String loadBaiduAccessTokenSeed()
{
  String token = String(BAIDU_ACCESS_TOKEN_SEED);
  if (token.length() == 0)
  {
    return "";
  }

  uint64_t expiresAt = static_cast<uint64_t>(BAIDU_ACCESS_TOKEN_EXPIRES_AT_SEED);
  if (!isBaiduTokenFresh(token, expiresAt, "seed"))
  {
    return "";
  }

  saveBaiduAccessTokenCache(token, expiresAt);
  return token;
}

static String buildBaiduTtsRequestBody(const String &accessToken, const String &text)
{
  int ttsPer = BAIDU_TTS_PER;
  int ttsAue = BAIDU_TTS_AUE;

#if BAIDU_TTS_FORCE_16K_WAV
  ttsAue = 6;
#endif

  String body = "tex=" + urlEncode(text);
  body += "&tok=" + urlEncode(accessToken);
  body += "&cuid=" + urlEncode(String(BAIDU_CUID));
  body += "&ctp=1";
  body += "&lan=zh";
  body += "&spd=" + String(BAIDU_TTS_SPD);
  body += "&pit=" + String(BAIDU_TTS_PIT);
  body += "&vol=" + String(BAIDU_TTS_VOL);
  body += "&per=" + String(ttsPer);
  body += "&aue=" + String(ttsAue);
#if BAIDU_TTS_FORCE_16K_WAV
  String audioCtrl = "{\"sampling_rate\":" + String(BAIDU_TTS_PLAYBACK_SAMPLE_RATE) + "}";
  body += "&audio_ctrl=" + urlEncode(audioCtrl);
#endif
  return body;
}

static bool playDecodedAudioBufferAtRate(uint8_t *audioBuffer, size_t audioLength, uint32_t playbackSampleRate)
{
  if (audioBuffer == nullptr || audioLength == 0)
  {
    return false;
  }

  WavPlaybackInfo info;
  if (!getWavPlaybackInfo(audioBuffer, audioLength, &info))
  {
    Serial.println("[语音合成] 错误: 无法解析音频数据");
    return false;
  }

  if (info.bitsPerSample != 16)
  {
    Serial.printf("[语音合成] 错误: 当前仅支持16位PCM，收到 bits=%u\n",
                  static_cast<unsigned>(info.bitsPerSample));
    return false;
  }

  if (info.channels != 1)
  {
    Serial.printf("[音频播放] 错误: 当前仅支持单声道WAV，收到 channels=%u\n",
                  static_cast<unsigned>(info.channels));
    return false;
  }

  uint8_t *playPayload = audioBuffer + info.payloadOffset;
  size_t playLength = info.payloadLength;

  if (!configureSpeakerSampleRate(playbackSampleRate, 1))
  {
    return false;
  }

  playLength &= ~static_cast<size_t>(0x01);
  Serial.printf("[音频播放] 音频播放准备完成，rate=%lu，总长度=%u，PCM偏移=%u，PCM长度=%u\n",
                static_cast<unsigned long>(playbackSampleRate),
                static_cast<unsigned>(audioLength),
                static_cast<unsigned>(info.payloadOffset),
                static_cast<unsigned>(playLength));
  clearAudio();
  playAudioStable(playPayload, playLength);
  delay(200);
  clearAudio();
  return true;
}

bool playAudioBuffer(uint8_t *audioBuffer, size_t audioLength)
{
  return playDecodedAudioBufferAtRate(audioBuffer, audioLength, BAIDU_TTS_PLAYBACK_SAMPLE_RATE);
}

bool playLocalAudioBuffer(uint8_t *audioBuffer, size_t audioLength)
{
  return playDecodedAudioBufferAtRate(audioBuffer, audioLength, LOCAL_AUDIO_PLAYBACK_SAMPLE_RATE);
}

static uint16_t readLe16(const uint8_t *buffer)
{
  return static_cast<uint16_t>(buffer[0]) |
         (static_cast<uint16_t>(buffer[1]) << 8);
}

static uint32_t readLe32(const uint8_t *buffer)
{
  return static_cast<uint32_t>(buffer[0]) |
         (static_cast<uint32_t>(buffer[1]) << 8) |
         (static_cast<uint32_t>(buffer[2]) << 16) |
         (static_cast<uint32_t>(buffer[3]) << 24);
}

static bool readExact(Stream &stream, uint8_t *buffer, size_t length)
{
  size_t totalRead = 0;
  unsigned long lastDataTime = millis();
  while (totalRead < length)
  {
    size_t bytesRead = stream.readBytes(buffer + totalRead, length - totalRead);
    if (bytesRead == 0)
    {
      if (millis() - lastDataTime > 1000)
      {
        return false;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    totalRead += bytesRead;
    lastDataTime = millis();
  }
  return true;
}

static bool skipStreamBytes(Stream &stream, size_t length)
{
  uint8_t discard[64];
  size_t remaining = length;
  while (remaining > 0)
  {
    size_t chunk = min(remaining, sizeof(discard));
    if (!readExact(stream, discard, chunk))
    {
      return false;
    }
    remaining -= chunk;
  }
  return true;
}

static bool playPcmPayloadFromStream(Stream &stream, size_t payloadLength)
{
  payloadLength &= ~static_cast<size_t>(0x01);
  if (payloadLength == 0)
  {
    return false;
  }

  constexpr size_t kAudioChunkSize = 4096;
  uint8_t *buffer = static_cast<uint8_t *>(
      heap_caps_malloc(kAudioChunkSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer == nullptr)
  {
    buffer = static_cast<uint8_t *>(heap_caps_malloc(kAudioChunkSize, MALLOC_CAP_8BIT));
  }
  if (buffer == nullptr)
  {
    Serial.println("[LocalAudio] Failed to allocate stream playback buffer.");
    return false;
  }

  size_t remaining = payloadLength;
  while (remaining > 0)
  {
    size_t chunk = min(remaining, kAudioChunkSize);
    chunk &= ~static_cast<size_t>(0x01);
    if (chunk == 0)
    {
      break;
    }

    if (!readExact(stream, buffer, chunk))
    {
      Serial.println("[LocalAudio] Failed to read audio stream chunk.");
      free(buffer);
      return false;
    }

    playAudio(buffer, chunk);
    remaining -= chunk;
  }

  free(buffer);
  return true;
}

bool playAudioStream(Stream &audioStream, size_t audioLength)
{
  if (audioLength < 12)
  {
    return false;
  }

  uint8_t riffHeader[12];
  if (!readExact(audioStream, riffHeader, sizeof(riffHeader)))
  {
    Serial.println("[LocalAudio] Failed to read WAV header.");
    return false;
  }

  if (memcmp(riffHeader, "RIFF", 4) != 0 || memcmp(riffHeader + 8, "WAVE", 4) != 0)
  {
    Serial.println("[LocalAudio] Only WAV local audio is supported.");
    return false;
  }

  WavPlaybackInfo info;
  info.payloadOffset = 0;
  info.payloadLength = 0;
  info.channels = 1;
  info.bitsPerSample = 16;

  size_t consumed = sizeof(riffHeader);
  while (consumed + 8 <= audioLength)
  {
    uint8_t chunkHeader[8];
    if (!readExact(audioStream, chunkHeader, sizeof(chunkHeader)))
    {
      return false;
    }
    consumed += sizeof(chunkHeader);

    uint32_t chunkSize = readLe32(chunkHeader + 4);
    size_t paddedChunkSize = static_cast<size_t>(chunkSize) + (chunkSize & 0x01);
    size_t availableChunkSize = min(paddedChunkSize, audioLength - consumed);

    if (memcmp(chunkHeader, "fmt ", 4) == 0)
    {
      uint8_t fmtBuffer[32] = {0};
      size_t fmtRead = min(static_cast<size_t>(chunkSize), sizeof(fmtBuffer));
      if (!readExact(audioStream, fmtBuffer, fmtRead))
      {
        return false;
      }

      if (fmtRead >= 16)
      {
        uint16_t audioFormat = readLe16(fmtBuffer);
        info.channels = readLe16(fmtBuffer + 2);
        info.bitsPerSample = readLe16(fmtBuffer + 14);
        Serial.printf("[LocalAudio] WAV fmt: format=%u channels=%u bits=%u, playback=%lu Hz mono\n",
                      static_cast<unsigned>(audioFormat),
                      static_cast<unsigned>(info.channels),
                      static_cast<unsigned>(info.bitsPerSample),
                      static_cast<unsigned long>(LOCAL_AUDIO_PLAYBACK_SAMPLE_RATE));
      }

      size_t rest = availableChunkSize > fmtRead ? availableChunkSize - fmtRead : 0;
      if (rest > 0 && !skipStreamBytes(audioStream, rest))
      {
        return false;
      }
      consumed += availableChunkSize;
      continue;
    }

    if (memcmp(chunkHeader, "data", 4) == 0)
    {
      info.payloadLength = min(static_cast<size_t>(chunkSize), audioLength - consumed);
      if (info.bitsPerSample != 16)
      {
        Serial.printf("[LocalAudio] Unsupported WAV bits=%u, only 16-bit PCM is supported.\n",
                      static_cast<unsigned>(info.bitsPerSample));
        return false;
      }

      if (info.channels != 1)
      {
        Serial.printf("[LocalAudio] Stream playback requires mono WAV, got channels=%u.\n",
                      static_cast<unsigned>(info.channels));
        return false;
      }

      if (!configureSpeakerSampleRate(LOCAL_AUDIO_PLAYBACK_SAMPLE_RATE, 1))
      {
        return false;
      }

      Serial.printf("[LocalAudio] Stream playback start, payload=%u bytes\n",
                    static_cast<unsigned>(info.payloadLength));
      clearAudio();
      bool ok = playPcmPayloadFromStream(audioStream, info.payloadLength);
      delay(200);
      clearAudio();
      return ok;
    }

    if (!skipStreamBytes(audioStream, availableChunkSize))
    {
      return false;
    }
    consumed += availableChunkSize;
  }

  Serial.println("[LocalAudio] WAV data chunk was not found.");
  return false;
}

static size_t findFirstBodyChar(const uint8_t *buffer, size_t bufferLength)
{
  size_t offset = 0;
  while (offset < bufferLength)
  {
    uint8_t c = buffer[offset];
    if (c != ' ' && c != '\r' && c != '\n' && c != '\t')
    {
      break;
    }
    ++offset;
  }
  return offset;
}

static bool bodyLooksLikeJson(const uint8_t *buffer, size_t bufferLength)
{
  if (buffer == nullptr || bufferLength == 0)
  {
    return false;
  }

  size_t offset = findFirstBodyChar(buffer, bufferLength);
  return offset < bufferLength && (buffer[offset] == '{' || buffer[offset] == '[');
}

static bool bodyLooksLikeWav(const uint8_t *buffer, size_t bufferLength)
{
  return buffer != nullptr &&
         bufferLength >= 12 &&
         memcmp(buffer, "RIFF", 4) == 0 &&
         memcmp(buffer + 8, "WAVE", 4) == 0;
}

static bool handleBaiduTtsJsonBody(uint8_t *responseBuffer, size_t responseLength)
{
  if (responseBuffer == nullptr || responseLength == 0)
  {
    return false;
  }

  responseBuffer[responseLength] = '\0';
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, reinterpret_cast<char *>(responseBuffer));
  if (error)
  {
    Serial.printf("[语音合成] JSON解析失败: %s\n", error.c_str());
    return false;
  }

  if (!doc.containsKey("binary"))
  {
    Serial.println("[语音合成] JSON中未找到 binary 字段");
    serializeJson(doc, Serial);
    Serial.println();
    return false;
  }

  const char *binary = doc["binary"].as<const char *>();
  if (binary == nullptr || binary[0] == '\0')
  {
    Serial.println("[语音合成] binary 字段为空");
    return false;
  }

  unsigned int binaryLength = strlen(binary);
  unsigned int decodedLength = decode_base64_length(
      reinterpret_cast<const unsigned char *>(binary),
      binaryLength);
  uint8_t *audioBuffer = static_cast<uint8_t *>(ps_malloc(decodedLength));
  if (audioBuffer == nullptr)
  {
    audioBuffer = static_cast<uint8_t *>(malloc(decodedLength));
  }
  if (audioBuffer == nullptr)
  {
    Serial.println("[语音合成] 错误: 无法为binary音频分配缓冲区");
    return false;
  }

  unsigned int actualLength = decode_base64(
      reinterpret_cast<const unsigned char *>(binary),
      binaryLength,
      audioBuffer);

  Serial.printf("[语音合成] 已从JSON binary解码音频，长度=%u\n",
                static_cast<unsigned>(actualLength));

  bool ok = playAudioBuffer(audioBuffer, actualLength);
  free(audioBuffer);
  return ok;
}

static void printBodyPreview(const uint8_t *buffer, size_t bufferLength)
{
  Serial.print("[voice][tts] body preview: ");
  size_t previewLength = min(bufferLength, static_cast<size_t>(160));
  for (size_t i = 0; i < previewLength; ++i)
  {
    char c = static_cast<char>(buffer[i]);
    if (c >= 32 && c <= 126)
    {
      Serial.print(c);
    }
    else
    {
      Serial.print('.');
    }
  }
  Serial.println();
}

static bool handleBaiduTtsJsonBody(const String &response)
{
  size_t responseLength = response.length();
  uint8_t *responseBuffer = static_cast<uint8_t *>(malloc(responseLength + 1));
  if (responseBuffer == nullptr)
  {
    return false;
  }

  memcpy(responseBuffer, response.c_str(), responseLength);
  responseBuffer[responseLength] = '\0';
  bool ok = handleBaiduTtsJsonBody(responseBuffer, responseLength);
  free(responseBuffer);
  return ok;
}


// 获取百度云平台的AccessToken
static bool waitForBaiduWiFiConnection(uint32_t timeoutMs)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.printf("[语音服务] WiFi 未连接，准备重新连接到 %s\n", WIFI_SSID);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  unsigned long lastLogMs = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs)
  {
    delay(250);

    if (millis() - lastLogMs >= 1000)
    {
      Serial.printf("[语音服务] 等待 WiFi 连通中... status=%d elapsed=%lu ms\n",
                    WiFi.status(),
                    millis() - startMs);
      lastLogMs = millis();
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("[语音服务] WiFi 已连接，IP=%s DNS1=%s DNS2=%s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str());
    return true;
  }

  Serial.printf("[语音服务] WiFi 仍未连接，status=%d，稍后继续重试\n", WiFi.status());
  return false;
}

#if 0
static String getAccessToken_baidu_legacy()
{
  Serial.println("[语音服务] 开始获取百度AccessToken...");

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[语音服务] 错误：当前WiFi未连接，无法获取百度AccessToken");
    return "";
  }
  
  // 创建安全客户端
  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client)
  {
    Serial.println("[语音服务] 错误：无法创建HTTPS客户端");
    return "";
  }
  
  // 跳过证书验证
  client->setInsecure();
  
  // 设置连接超时
  client->setTimeout(BAIDU_TOKEN_CLIENT_TIMEOUT_SEC);
  
  // 创建HTTP客户端
  HTTPClient https;
  HTTPClient https;
  String accessToken = "";
  const char *baiduHost = "aip.baidubce.com";
  IPAddress resolvedIp;
  unsigned long requestStartMs = millis();

  // 百度API请求地址
  String target_url = String("https://aip.baidubce.com/oauth/2.0/token?client_id=") + BAIDU_CLIENT_ID + "&client_secret=" + BAIDU_CLIENT_SECRET + "&grant_type=client_credentials";

  bool dnsResolved = false;
  for (int attempt = 1; attempt <= BAIDU_TOKEN_DNS_RETRY_COUNT; ++attempt)
  {
    Serial.printf("[语音服务] 正在解析域名 %s，第 %d/%d 次...\n",
                  baiduHost, attempt, BAIDU_TOKEN_DNS_RETRY_COUNT);
    if (WiFi.hostByName(baiduHost, resolvedIp))
    {
      dnsResolved = true;
      Serial.printf("[语音服务] 域名解析成功: %s -> %s\n",
                    baiduHost, resolvedIp.toString().c_str());
      break;
    }

    Serial.printf("[语音服务] 域名解析失败，第 %d/%d 次，%d ms 后重试\n",
                  attempt, BAIDU_TOKEN_DNS_RETRY_COUNT, BAIDU_TOKEN_DNS_RETRY_DELAY_MS);
    delay(BAIDU_TOKEN_DNS_RETRY_DELAY_MS);
  }

  if (!dnsResolved)
  {
    Serial.println("[语音服务] 错误：百度域名解析持续失败，请检查热点/路由器 DNS 或外网连通性");
    delete client;
    return "";
  }

  Serial.println("[语音服务] 连接到百度API服务器...");
  #if 0
  if (!https.begin(*client, target_url))
  {
    #if 0
    #if 0
    Serial.println("[语音服务] 错误：HTTPClient 初始化失败");
    #endif
    Serial.println("[voice] HTTPClient init failed");
    #endif
    Serial.println("[voice] HTTPClient init failed");
    delete client;
    return "";
  }
  
  // 设置HTTP超时
  #endif
  if (!https.begin(*client, target_url))
  {
    #if 0
    Serial.println("[语音服务] 错误：HTTPClient 初始化失败");
    #endif
    Serial.println("[voice] HTTPClient init failed");
    delete client;
    return "";
  }
  https.setConnectTimeout(BAIDU_TOKEN_CONNECT_TIMEOUT_MS);
  https.setTimeout(BAIDU_TOKEN_HTTP_TIMEOUT_MS);
  
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Accept", "application/json");

  // 构造空JSON字符串负载
  String payload = "\"\"";
  
  Serial.printf("[语音服务] 等待百度Token响应，客户端超时=%d秒，HTTP超时=%d毫秒\n",
                BAIDU_TOKEN_CLIENT_TIMEOUT_SEC, BAIDU_TOKEN_HTTP_TIMEOUT_MS);
  int httpCode = https.POST(payload);
  
  // Serial.printf("[语音服务] HTTP响应代码: %d\n", httpCode);

  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
    {
      String response = https.getString();
      // Serial.println("[语音服务] 收到响应: " + response);
      
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      
      if (error) {
        Serial.printf("[语音服务] JSON解析错误: %s\n", error.c_str());
      } else {
        accessToken = doc["access_token"].as<String>();
        if (accessToken.length() > 0) {
          Serial.printf("[语音服务] AccessToken 获取成功，耗时 %lu ms\n", millis() - requestStartMs);
          Serial.println("[语音服务] ✓ 获取AccessToken成功");
        } else {
          Serial.printf("[语音服务] 错误：响应中没有access_token，body=%s\n", response.c_str());
          Serial.println("[语音服务] 错误：响应中没有access_token");
        }
      }
    }
    else
    {
      String response = https.getString();
      Serial.printf("[语音服务] HTTP 请求失败，错误代码: %d, 错误信息: %s, body=%s\n",
                    httpCode, https.errorToString(httpCode).c_str(), response.c_str());
      Serial.printf("[语音服务] HTTP请求失败，错误代码: %d, 错误信息: %s\n", 
                    httpCode, https.errorToString(httpCode).c_str());
    }
  }
  else
  {
    Serial.printf("[语音服务] 连接失败，错误代码: %d, 错误信息: %s\n", 
                  httpCode, https.errorToString(httpCode).c_str());
  }
  
  https.end();
  delete client;
  
  if (accessToken.length() == 0) {
    Serial.println("[语音服务] ✗ AccessToken获取失败，语音服务将无法使用");
  }
  Serial.println("token为:" + accessToken);

  return accessToken;
}

// 百度语音识别
// accessToken: 百度云平台的AccessToken
// audioData: PCM原始数据的指针
// audioDataSize: size of PCM data
static String waitForAccessToken_baidu_legacy()
{
  uint32_t attempt = 0;

  while (true)
  {
    ++attempt;
    Serial.printf("[语音服务] 阻塞等待百度AccessToken，第 %lu 次尝试\n",
                  static_cast<unsigned long>(attempt));

    if (!waitForBaiduWiFiConnection(BAIDU_TOKEN_WIFI_WAIT_TIMEOUT_MS))
    {
      delay(BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
      continue;
    }

    String token = getAccessToken_baidu();
    if (token.length() > 0)
    {
      return token;
    }

    Serial.printf("[语音服务] AccessToken 仍未获取到，%d ms 后继续阻塞重试\n",
                  BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
    delay(BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
  }
}

#endif
static bool ensureWiFiForBaiduToken(uint32_t timeoutMs)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.printf("[voice] WiFi not connected, reconnecting to %s\n", WIFI_SSID);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs)
  {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("[voice] WiFi ready, ip=%s dns1=%s dns2=%s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str());
    return true;
  }

  Serial.printf("[voice] WiFi still not ready, status=%d\n", WiFi.status());
  return false;
}

String getAccessToken_baidu()
{
  if (!ensureWiFiForBaiduToken(BAIDU_TOKEN_WIFI_WAIT_TIMEOUT_MS))
  {
    return "";
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(BAIDU_TOKEN_CLIENT_TIMEOUT_SEC);
  client.setHandshakeTimeout(BAIDU_HTTPS_HANDSHAKE_TIMEOUT_SEC);

  HTTPClient https;
  IPAddress resolvedIp;
  const char *baiduHost = "aip.baidubce.com";
  String accessToken = "";
  String targetUrl = "https://aip.baidubce.com/oauth/2.0/token";
  String requestBody = buildBaiduTokenRequestBody();

  bool dnsResolved = false;
  for (int attempt = 1; attempt <= BAIDU_TOKEN_DNS_RETRY_COUNT; ++attempt)
  {
    Serial.printf("[voice] resolving %s (%d/%d)\n",
                  baiduHost,
                  attempt,
                  BAIDU_TOKEN_DNS_RETRY_COUNT);

    if (WiFi.hostByName(baiduHost, resolvedIp))
    {
      dnsResolved = true;
      Serial.printf("[voice] dns ok: %s -> %s\n",
                    baiduHost,
                    resolvedIp.toString().c_str());
      break;
    }

    delay(BAIDU_TOKEN_DNS_RETRY_DELAY_MS);
  }

  if (!dnsResolved)
  {
    Serial.println("[voice] dns resolve failed");
    return "";
  }

  if (!https.begin(client, targetUrl))
  {
    Serial.println("[voice] HTTPClient begin failed");
    return "";
  }

  https.setReuse(false);
  https.useHTTP10(true);
  https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  https.setConnectTimeout(BAIDU_TOKEN_CONNECT_TIMEOUT_MS);
  https.setTimeout(BAIDU_TOKEN_HTTP_TIMEOUT_MS);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  https.addHeader("Accept", "application/json");

  unsigned long requestStartMs = millis();
  int httpCode = https.POST(requestBody);

  if (httpCode > 0)
  {
    String response = https.getString();

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
    {
      DynamicJsonDocument filter(64);
      filter["access_token"] = true;

      DynamicJsonDocument doc(768);
      DeserializationError error = deserializeJson(
          doc,
          response,
          DeserializationOption::Filter(filter));
      if (!error)
      {
        accessToken = doc["access_token"].as<String>();
      }
      else
      {
        Serial.printf("[voice] token json parse failed: %s\n", error.c_str());
      }

      if (accessToken.length() > 0)
      {
        Serial.printf("[voice] token ok in %lu ms\n", millis() - requestStartMs);
      }
      else
      {
        Serial.printf("[voice] token missing in response: %s\n", response.c_str());
      }
    }
    else
    {
      Serial.printf("[voice] token http failed: code=%d err=%s body=%s\n",
                    httpCode,
                    https.errorToString(httpCode).c_str(),
                    response.c_str());
    }
  }
  else
  {
    Serial.printf("[voice] token request failed: code=%d err=%s\n",
                  httpCode,
                  https.errorToString(httpCode).c_str());
  }

  https.end();
  return accessToken;
}

String waitForAccessToken_baidu()
{
  String cachedToken = loadBaiduAccessTokenCache();
  if (cachedToken.length() > 0)
  {
    Serial.println("[voice] Baidu token loaded from NVS cache");
    return cachedToken;
  }

  String seedToken = loadBaiduAccessTokenSeed();
  if (seedToken.length() > 0)
  {
    Serial.println("[voice] Baidu token loaded from config seed and persisted to NVS");
    return seedToken;
  }

  uint32_t attempt = 0;

  while (true)
  {
    ++attempt;
    if (attempt == 1)
    {
      Serial.println("[voice] Baidu token is required; startup will block here until it is ready");
    }
    Serial.printf("[voice] waiting for Baidu token, attempt=%lu\n",
                  static_cast<unsigned long>(attempt));

    String token = getAccessToken_baidu();
    if (token.length() > 0)
    {
      saveBaiduAccessTokenCache(token, 0);
      return token;
    }

    Serial.printf("[voice] token unavailable, retry in %d ms\n",
                  BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
    delay(BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
  }
}

String baidu_voice_recognition(String accessToken, uint8_t *audioData, int audioDataSize)
{
  String recognizedText = "";

  if (accessToken == "")
  {
    Serial.println("access_token is null");
    return recognizedText;
  }

  // audio数据包许愿哦进行Base64编码，数据量会增大1/3
  int audio_data_len = audioDataSize * sizeof(char) * 1.4;
  unsigned char *audioDataBase64 = (unsigned char *)ps_malloc(audio_data_len);
  if (!audioDataBase64)
  {
    Serial.println("Failed to allocate memory for audioDataBase64");
    return recognizedText;
  }

  // json包大小，由于需要将audioData数据进行Base64的编码，数据量会增大1/3
  int data_json_len = audioDataSize * sizeof(char) * 1.4;
  char *data_json = (char *)ps_malloc(data_json_len);
  if (!data_json)
  {
    Serial.println("Failed to allocate memory for data_json");
    return recognizedText;
  }

  // Base64 encode audio data
  encode_base64(audioData, audioDataSize, audioDataBase64);

  memset(data_json, '\0', data_json_len);
  strcat(data_json, "{");
  strcat(data_json, "\"format\":\"pcm\",");
  strcat(data_json, "\"rate\":16000,");
  strcat(data_json, "\"dev_pid\":1537,");
  strcat(data_json, "\"channel\":1,");
  strcat(data_json, "\"cuid\":\"57722200\",");
  strcat(data_json, "\"token\":\"");
  strcat(data_json, accessToken.c_str());
  strcat(data_json, "\",");
  sprintf(data_json + strlen(data_json), "\"len\":%d,", audioDataSize);
  strcat(data_json, "\"speech\":\"");
  strcat(data_json, (const char *)audioDataBase64);
  strcat(data_json, "\"");
  strcat(data_json, "}");

  // 创建http请求
  HTTPClient http_client;

  http_client.begin("http://vop.baidu.com/server_api");
  http_client.addHeader("Content-Type", "application/json");
  int httpCode = http_client.POST(data_json);

  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK)
    {
      // 获取返回结果
      String response = http_client.getString();
      Serial.println(response);

      // 从json中解析对应的result
      DynamicJsonDocument responseDoc(1024);
      deserializeJson(responseDoc, response);

      // 假设 responseDoc["result"] 是一个 JSON 数组，并且你想要第一个元素
      if (responseDoc["result"].is<JsonArray>())
      {
        JsonArray resultArray = responseDoc["result"].as<JsonArray>();
        if (resultArray.size() > 0)
        {
          recognizedText = resultArray[0].as<String>();
        }
        else
        {
          Serial.println("Error: 'result' 数组为空。");
          recognizedText = ""; // 或者其他你希望的默认值
        }
      }
      else if (responseDoc["result"].is<String>())
      {
        // 如果 "result" 直接就是一个字符串，则直接赋值
        recognizedText = responseDoc["result"].as<String>();
      }
      else
      {
        Serial.println("Error: 'result' 不是一个字符串或字符串数组。");
        recognizedText = ""; // 或者其他你希望的默认值
      }
    }
  }
  else
  {
    Serial.printf("[HTTP] POST failed, error: %s\n", http_client.errorToString(httpCode).c_str());
  }

  // 释放内存
  if (audioDataBase64)
  {
    free(audioDataBase64);
  }

  if (data_json)
  {
    free(data_json);
  }

  http_client.end();

  return recognizedText;
}

// Play zai audio data using MAX98357A
void playAudio_Zai(void)
{
  if (!playLocalAudioById("record_start_001"))
  {
    Serial.println("[LocalAudio] record_start_001 unavailable; start prompt skipped.");
  }
}


void baiduTTS_Send(String access_token, String text)
{
  if (access_token == "")
  {
    Serial.println("access_token is null");
    return;
  }

  if (text.length() == 0)
  {
    Serial.println("text is null");
    return;
  }
  String url = "https://tsn.baidu.com/text2audio";
  String requestBody = buildBaiduTtsRequestBody(access_token, text);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout((BAIDU_TTS_HTTP_TIMEOUT_MS + 999) / 1000);
  client.setHandshakeTimeout(BAIDU_HTTPS_HANDSHAKE_TIMEOUT_SEC);

  HTTPClient http;
  if (!http.begin(client, url))
  {
    Serial.println("[voice][tts] HTTPClient begin failed");
    return;
  }

  const char *headerKeys[] = {"Content-Type"};
  http.collectHeaders(headerKeys, 1);
  http.setReuse(false);
  http.useHTTP10(true);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setConnectTimeout(BAIDU_TOKEN_CONNECT_TIMEOUT_MS);
  http.setTimeout(BAIDU_TTS_HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Accept", "*/*");
  http.addHeader("cuid", String(BAIDU_CUID));

  Serial.printf("[语音合成] 调用新版百度TTS接口，请求体长度=%u\n",
                static_cast<unsigned>(requestBody.length()));
  Serial.printf("[语音合成] 文本内容: %s\n", text.c_str());

  int httpResponseCode = http.POST(requestBody);
  Serial.printf("[语音合成] HTTP状态码=%d\n", httpResponseCode);
  if (httpResponseCode > 0)
  {
    String contentType = http.header("Content-Type");
    if (httpResponseCode == HTTP_CODE_OK)
    {
      Serial.printf("[voice][tts] Content-Type=%s\n", contentType.c_str());

      uint8_t *responseBuffer = nullptr;
      size_t responseLength = 0;
      if (!downloadHttpBody(http, &responseBuffer, &responseLength))
      {
        Serial.println("[voice][tts] response body download failed");
        http.end();
        return;
      }

      Serial.printf("[voice][tts] response bytes=%u\n", static_cast<unsigned>(responseLength));
      bool handled = false;

      if (contentType.startsWith("audio") || bodyLooksLikeWav(responseBuffer, responseLength))
      {
        handled = playAudioBuffer(responseBuffer, responseLength);
      }

      if (!handled && (contentType.indexOf("json") >= 0 || bodyLooksLikeJson(responseBuffer, responseLength)))
      {
        handled = handleBaiduTtsJsonBody(responseBuffer, responseLength);
      }

      if (!handled)
      {
        Serial.println("[voice][tts] unhandled response body");
        printBodyPreview(responseBuffer, responseLength);
      }

      free(responseBuffer);
      http.end();
      return;
    }
    Serial.printf("[语音合成] Content-Type=%s\n", contentType.c_str());

    if (httpResponseCode == HTTP_CODE_OK)
    {
      if (contentType.startsWith("audio"))
      {
        Serial.println("[语音合成] 收到原始音频流响应");
        uint8_t *audioBuffer = nullptr;
        size_t audioLength = 0;
        if (downloadHttpBody(http, &audioBuffer, &audioLength))
        {
          playAudioBuffer(audioBuffer, audioLength);
          free(audioBuffer);
        }
      }
      else if (contentType.indexOf("json") >= 0)
      {
        Serial.println("[语音合成] 收到JSON响应，尝试解析");
        String response = http.getString();
        Serial.println(response);
        if (!handleBaiduTtsJsonBody(response))
        {
          Serial.println("[语音合成] JSON响应未能转换为可播放音频");
        }
      }
      else
      {
        Serial.println("[语音合成] 未知的Content-Type: " + contentType);
      }
    }
    else
    {
      Serial.print("[语音合成] Error code: ");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.println(response);
    }
  }
  else
  {
    Serial.print("[语音合成] Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include <Arduino.h>

struct ServerResponse
{
  bool jsonValid = false;
  bool ok = false;
  String intent = "";
  String response = "";
  String error = "";
  String speakMode = "tts_text";
  String speakText = "";
  String audioId = "";
  String audioUrl = "";
  bool navigationActive = false;
  bool navigationComplete = false;
  bool navigationStarted = false;
  bool navigationExited = false;
  String destination = "";
  String nextInstruction = "";
  long remainingDistance = -1;
  long totalDuration = -1;
};

ServerResponse parseServerResponse(const String &payload);
String getSpeakText(const ServerResponse &response);

#endif // JSON_HELPER_H

#include "json_helper.h"

#include <ArduinoJson.h>

ServerResponse parseServerResponse(const String &payload)
{
  ServerResponse parsed;
  if (payload.length() == 0)
  {
    parsed.response = "";
    return parsed;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    parsed.response = payload;
    return parsed;
  }

  parsed.jsonValid = true;
  parsed.ok = doc["ok"] | true;
  parsed.intent = doc["intent"] | "";
  parsed.response = doc["response"] | "";
  parsed.error = doc["error"]["message"] | "";

  if (doc.containsKey("speak"))
  {
    JsonObject speak = doc["speak"].as<JsonObject>();
    parsed.speakMode = speak["mode"] | "tts_text";
    parsed.speakText = speak["text"] | "";
    parsed.audioId = speak["audio_id"] | "";
    parsed.audioUrl = speak["audio_url"] | "";
  }

  if (doc.containsKey("navigation"))
  {
    JsonObject navigation = doc["navigation"].as<JsonObject>();
    parsed.navigationActive = navigation["active"] | false;
    parsed.destination = navigation["destination"] | "";
    parsed.remainingDistance = navigation["remaining_distance"] | -1;
    parsed.totalDuration = navigation["total_duration"] | -1;
    parsed.nextInstruction = navigation["next_instruction"] | "";
  }

  parsed.navigationStarted = doc["navigation_started"] | false;
  parsed.navigationComplete = doc["navigation_complete"] | false;
  parsed.navigationExited = doc["navigation_exited"] | false;

  if (parsed.destination.length() == 0 && doc.containsKey("destination"))
  {
    parsed.destination = doc["destination"].as<String>();
  }
  if (parsed.nextInstruction.length() == 0 && doc.containsKey("next_instruction"))
  {
    parsed.nextInstruction = doc["next_instruction"].as<String>();
  }

  if (parsed.response.length() == 0 && doc.containsKey("error") && doc["error"].is<const char *>())
  {
    parsed.response = doc["error"].as<String>();
  }
  return parsed;
}

String getSpeakText(const ServerResponse &response)
{
  if (response.speakText.length() > 0)
  {
    return response.speakText;
  }
  if (response.response.length() > 0)
  {
    return response.response;
  }
  if (response.nextInstruction.length() > 0)
  {
    return response.nextInstruction;
  }
  return "";
}

#include "Control.h"

namespace studio {
const char* statusLine(const Status& s) {
    switch (s.link) {
    case Link::Offline: return "OBS offline / WebSocket off";
    case Link::Authentication: return "OBS password rejected";
    case Link::Settings: return "Check OBS connection settings";
    case Link::Failed: return "OBS connection lost";
    case Link::Waiting: return "Connecting to OBS...";
    case Link::Ready:
        if (s.clip == Clip::Saving) return "Saving replay...";
        if (s.clip == Clip::Saved) return "Clip saved";
        if (s.clip == Clip::Unconfirmed) return "Clip save unconfirmed";
        if (s.busy) return "Waiting for OBS...";
        if (s.rejected) return "OBS declined the last action";
        if (s.reconnecting) return "Stream reconnecting";
        if (s.paused) return s.live ? "LIVE  /  Recording paused" : "Recording paused";
        if (s.recording) return s.live ? "LIVE  /  Recording" : "Recording";
        return s.live ? "LIVE" : "OBS ready";
    }
    return "";
}
const char* recordButton(const Status& s) {
    if (s.link != Link::Ready) return "Recording unavailable";
    return s.recording ? "Stop recording" : "Start recording";
}
const char* replayButton(const Status& s) {
    if (s.link != Link::Ready) return "Replay unavailable";
    if (s.replay == Replay::Unavailable) return "Enable Replay Buffer in OBS";
    return s.replay == Replay::Running ? "Save replay" : "Start replay buffer";
}
const char* streamButton(const Status& s) {
    if (s.link != Link::Ready) return "Streaming unavailable";
    return s.live ? "Stop stream" : "Start stream";
}
}

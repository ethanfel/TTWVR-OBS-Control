#pragma once
#include <cstdint>

namespace studio {
enum class Link { Waiting, Ready, Offline, Authentication, Settings, Failed };
enum class Action { Record, Stream, Replay };
enum class Replay { Unavailable, Stopped, Running };
enum class Clip { Idle, Saving, Saved, Unconfirmed };
struct Status {
    Link link = Link::Waiting;
    bool recording = false, paused = false, live = false, reconnecting = false;
    Replay replay = Replay::Unavailable;
    Clip clip = Clip::Idle;
    bool busy = false, rejected = false;
};
const char* statusLine(const Status&);
const char* recordButton(const Status&);
const char* streamButton(const Status&);
const char* replayButton(const Status&);
void visit();
Status read();
void press(Action, bool expectedActive);
void stop();
}

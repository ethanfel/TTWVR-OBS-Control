#pragma once
#include <cstdint>

namespace studio {
enum class Link { Waiting, Ready, Offline, Authentication, Settings, Failed };
enum class Action { Record, Stream };
struct Status {
    Link link = Link::Waiting;
    bool recording = false, paused = false, live = false, reconnecting = false;
    bool busy = false, rejected = false;
};
const char* statusLine(const Status&);
const char* recordButton(const Status&);
const char* streamButton(const Status&);
void visit();
Status read();
void press(Action, bool expectedActive);
void stop();
}

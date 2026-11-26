#pragma once
#include <cstdint>
#include "Control.h"
#include "Preview.h"

struct Surface { std::uint32_t* pixels; int width, height; };
bool drawObsPanel(Surface&, const studio::Status&, const studio::Picture& picture=studio::preview());

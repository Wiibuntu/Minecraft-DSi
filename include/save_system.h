#pragma once

#include "player.h"

bool saveGame(const Player& player);
bool loadGame(Player& player);
const char* getSaveStatusMessage();

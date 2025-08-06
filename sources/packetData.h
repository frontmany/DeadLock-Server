#pragma once
#include <string>
#include <vector>
#include "queryType.h"

struct PacketData {
    std::string loginHashTo;
    std::string loginHashFrom;
    std::string packet;
    QueryType type;
};

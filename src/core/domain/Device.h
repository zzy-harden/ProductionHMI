#pragma once

#include "Tag.h"
#include <vector>

namespace ProductionHMI::Core {

struct DeviceConfig {
    DeviceId id;
    std::string name;

    std::string ip;
    uint16_t port{502};
    uint8_t unitId{1};

    uint32_t timeoutMs{1000};
    uint32_t retryCount{3};

    std::vector<TagDefinition> tags;
};

struct DeviceRuntimeState {
    DeviceId id;
    DeviceState state{DeviceState::Offline};
};

}

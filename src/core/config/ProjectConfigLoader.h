#pragma once

#include "../domain/Device.h"

#include <string>
#include <vector>

namespace ProductionHMI::Core {

struct ConfigLoadResult {
    bool success{false};
    DeviceConfig device;
    std::vector<std::string> errors;
};

class ProjectConfigLoader {
public:
    ConfigLoadResult loadFromFile(const std::string& path) const;
};

}

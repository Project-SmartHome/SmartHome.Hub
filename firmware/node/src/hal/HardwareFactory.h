
#pragma once

#include "INodeTransport.h"
#include "INodeStorage.h"
#include "INodeGpio.h"

class HardwareFactory {
public:
    static void begin();

    static INodeTransport& transport();

    static INodeStorage& storage();

    static INodeGpio& gpio();

    static void update();
};

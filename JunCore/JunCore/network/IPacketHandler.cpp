#include "IPacketHandler.h"

// Thread-local 정적 변수 정의
thread_local IPacketHandler* PacketHandlerRegistry::currentHandler = nullptr;
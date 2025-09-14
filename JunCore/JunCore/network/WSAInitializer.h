#pragma once
#include "../core/WindowsIncludes.h"
#include "../../JunCommon/synchronization/OnceInitializer.h"

//------------------------------
// WSA 전역 초기화 클래스
// NetBase 객체가 여러 개 생성되어도 WSAStartup은 한 번만 호출됨
//------------------------------
class WSAInitializer
{
private:
    // WSA 전용 OnceInitializer - 스핀락 방식
    using WSAOnceInit = DefaultOnceInitializer;

public:
    static bool Initialize()
    {
        return WSAOnceInit::Initialize([]() -> bool {
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                LOG_ERROR("[WSAInitializer] WSAStartup failed with error: %d", result);
                return false;
            }
            LOG_INFO("[WSAInitializer] WSAStartup succeeded (first call)");
            return true;
        });
    }
    
    static void Cleanup()
    {
        WSAOnceInit::Cleanup([]() {
            WSACleanup();
            LOG_INFO("[WSAInitializer] WSACleanup called (last reference)");
        });
    }
    
    static bool IsInitialized()
    {
        return WSAOnceInit::IsInitialized();
    }
    
    //------------------------------
    // 디버깅/테스트용 추가 기능
    //------------------------------
    static int GetReferenceCount()
    {
        return WSAOnceInit::GetReferenceCount();
    }
    
    static int GetState()
    {
        return WSAOnceInit::GetState();
    }
    
    static void ForceReset()
    {
        WSAOnceInit::ForceReset();
    }
};
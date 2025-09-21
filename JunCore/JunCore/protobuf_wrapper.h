#pragma once

// Protobuf만 워닝 억제하는 래퍼
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267)  // size_t to int conversion
#pragma warning(disable: 4251)  // DLL interface warning
#endif

// 모든 protobuf 헤더를 여기서 포함
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/arena.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// 생성된 protobuf 파일들을 위한 매크로
#define PROTOBUF_SUPPRESS_WARNINGS_START \
    __pragma(warning(push)) \
    __pragma(warning(disable: 4267)) \
    __pragma(warning(disable: 4251))

#define PROTOBUF_SUPPRESS_WARNINGS_END \
    __pragma(warning(pop))
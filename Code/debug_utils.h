#pragma once
#include <cstdio>

// Consider NDEBUG as the authoritative indicator of Release builds.
// In typical MSVC setups, NDEBUG is defined for Release and _DEBUG for Debug.
// We enable debug helpers only when NDEBUG is NOT defined.
#if !defined(NDEBUG)
    #define DEBUG_ACTIVE 1
#else
    #define DEBUG_ACTIVE 0
#endif

#if DEBUG_ACTIVE
    #define DEBUG_ONLY(code) do { code } while(0)
    #define DBG_PRINTF(...) std::printf(__VA_ARGS__)
    #define DBG_DRAWFMT(...) DrawFormatString(__VA_ARGS__)
#else
    #define DEBUG_ONLY(code) do { (void)0; } while(0)
    #define DBG_PRINTF(...) ((void)0)
    #define DBG_DRAWFMT(...) ((void)0)
#endif

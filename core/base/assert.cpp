#include "core/base/assert.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace trion {
namespace {

void defaultHandler(const char* expr, const char* file, int line)
{
    std::fprintf(stderr, "TRION assert failed: %s at %s:%d\n", expr, file, line);
    std::abort();
}

std::atomic<AssertHandler> g_handler{&defaultHandler};

} // namespace

AssertHandler setAssertHandler(AssertHandler handler) noexcept
{
    return g_handler.exchange(handler ? handler : &defaultHandler);
}

void assertFailed(const char* expr, const char* file, int line)
{
    g_handler.load()(expr, file, line);
}

} // namespace trion

#pragma once

#if __x86_64__
#include <async/platform/x86_64.h>
#else
#error "Unsupported platform"
#endif

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>

#include "physics/jolt_init.h"

#include <cstdarg>
#include <cstdio>

namespace ae::physics {
namespace {
void trace(const char *format, ...) {
  va_list args;va_start(args,format);char buffer[1024];
  std::vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);
  std::fprintf(stderr,"[Jolt] %s\n",buffer);std::fflush(stderr);
}
#ifdef JPH_ENABLE_ASSERTS
bool assertion(const char *expression,const char *message,const char *file,JPH::uint line) {
  std::fprintf(stderr,"[Jolt] asserção falhou: %s\n  motivo: %s\n  em %s:%u\n",expression,
               message?message:"(sem mensagem)",file,line);std::fflush(stderr);return true;
}
#endif
}
void ensureJoltInitialized() {
  static const bool registered=[] {
    JPH::Trace=trace;JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed=assertion;)
    JPH::RegisterDefaultAllocator();JPH::Factory::sInstance=new JPH::Factory();JPH::RegisterTypes();return true;
  }();
  (void)registered;
}
} // namespace ae::physics

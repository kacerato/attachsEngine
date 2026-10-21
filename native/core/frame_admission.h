#pragma once
#include <cstdint>
namespace ae {
// Keep the ideal deadline phase across display callbacks. Rounding each20ms
// interval up to a120Hz VSYNC separately would incorrectly turn50FPS into40.
// Long gaps coalesce into one frame; there is no catch-up submission burst.
inline bool admitPresentationFrame(std::int64_t now,std::int64_t interval,
    std::int64_t &phase,std::int64_t tolerance=500000) noexcept {
 if(now<=0 || interval<=0)return false;
 if(phase==0 || now+tolerance<phase){phase=now;return true;}
 const auto elapsed=now-phase;
 if(elapsed+tolerance<interval)return false;
 const auto steps=(elapsed+tolerance)/interval;
 phase+=steps*interval;
 return true;
}
}

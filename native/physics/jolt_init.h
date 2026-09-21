#pragma once

namespace ae::physics {

// Factory e registro de tipos são globais no Jolt. Cooking autoral pode ocorrer
// antes de existir um mundo; todos os consumidores entram por esta função única.
void ensureJoltInitialized();

} // namespace ae::physics

#include "harness.h"
#include "platform/process_memory.h"

using namespace ae;
using namespace ae::platform;
using namespace ae::test;

AE_TEST(process_memory_parseia_status_android_em_bytes) {
  constexpr auto status =
      "Name:\taether\nVmPeak:\t  999999 kB\nVmSize:\t  400000 kB\n"
      "VmHWM:\t  120000 kB\nVmRSS:\t  100000 kB\nRssAnon:\t 70000 kB\n"
      "RssFile:\t 25000 kB\nRssShmem:\t 5000 kB\nVmSwap:\t 2048 kB\n";
  ProcessMemorySnapshot snapshot{};
  AE_EXPECT_TRUE(parseProcStatus(status, snapshot), "VmRSS torna o snapshot valido");
  AE_EXPECT_EQ(snapshot.residentBytes, 100000u * 1024u, "RSS convertido para bytes");
  AE_EXPECT_EQ(snapshot.peakResidentBytes, 120000u * 1024u, "pico RSS preservado");
  AE_EXPECT_EQ(snapshot.anonymousBytes + snapshot.fileBytes + snapshot.sharedBytes,
               snapshot.residentBytes, "decomposicao RSS preservada");
  AE_EXPECT_EQ(snapshot.swapBytes, 2048u * 1024u, "swap do processo preservado");
}

AE_TEST(process_memory_rejeita_status_sem_rss_e_nao_preserva_lixo) {
  ProcessMemorySnapshot snapshot{};
  snapshot.valid = true;
  snapshot.residentBytes = 42;
  AE_EXPECT_TRUE(!parseProcStatus("VmSize: 100 kB\n", snapshot), "RSS e obrigatorio");
  AE_EXPECT_TRUE(!snapshot.valid && snapshot.residentBytes == 0,
                 "falha publica snapshot zerado");
}

AE_TEST(system_memory_exige_total_e_disponivel_consistentes) {
  SystemMemorySnapshot snapshot{};
  AE_EXPECT_TRUE(parseProcMemInfo("MemTotal: 8000000 kB\nMemAvailable: 2000000 kB\n",
                                 snapshot),
                 "meminfo completo e valido");
  AE_EXPECT_EQ(snapshot.totalBytes, static_cast<u64>(8000000) * 1024u,
               "RAM total em bytes");
  AE_EXPECT_TRUE(!parseProcMemInfo("MemTotal: 100 kB\nMemAvailable: 101 kB\n", snapshot),
                 "disponivel acima do total e invalido");
}

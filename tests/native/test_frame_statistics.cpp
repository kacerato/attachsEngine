#include "harness.h"
#include "profiler/frame_statistics.h"
#include <cmath>
#include <limits>

using namespace ae::profiler;
using ae::u64;

AE_TEST(frame_statistics_warmup_temporal_preserva_pico_separado) {
  FrameStatistics statistics;
  statistics.configure(5, 1);
  statistics.record({}, {});
  statistics.record({3'000'000, 4'000'000, 2'000'000}, {});
  AE_EXPECT_EQ(statistics.record({5'000'000, 10'000'000, 3'000'000}, {}), FrameSampleResult::WarmingUp, "intervalo que cruza fim do warmup ainda excluido");
  AE_EXPECT_EQ(statistics.record({6'000'000, 11'000'000, 4'000'000}, {}), FrameSampleResult::WindowReady, "primeiro intervalo completamente estavel");
  FrameProfileSummary summary;
  statistics.summarize(summary);
  AE_EXPECT_EQ(summary.warmupSamples, 2u, "contagem de warmup");
  AE_EXPECT_EQ(summary.warmupProcessCpuMaxMs, 6.0, "pico frio nao pode desaparecer");
  AE_EXPECT_EQ(summary.metrics[1].maximum, 1.0, "nao misturar warmup e janela estavel");
}

AE_TEST(frame_statistics_janelas_consecutivas_incluem_intervalo_do_relatorio) {
  FrameStatistics statistics;
  statistics.configure(0, 1);
  statistics.record({}, {});
  statistics.record({10'000'000, 1'000'000, 1'000'000}, {});
  statistics.record({40'000'000, 4'000'000, 2'000'000}, {});
  FrameProfileSummary summary;
  statistics.summarize(summary);
  AE_EXPECT_EQ(summary.samples, 1u, "storage reutilizado");
  AE_EXPECT_EQ(summary.elapsedMs, 30.0, "gap entre janelas deve aparecer");
  AE_EXPECT_EQ(summary.metrics[1].mean, 3.0, "CPU entre reports deve aparecer");
}

AE_TEST(frame_statistics_reset_descarta_janela_e_tempo_suspenso) {
  FrameStatistics statistics;
  statistics.configure(0, 2);
  statistics.record({}, {});
  statistics.record({10'000'000, 1'000'000, 1'000'000}, {});
  statistics.reset();
  statistics.record({1'000'000'000, 90'000'000, 80'000'000}, {});
  statistics.record({1'010'000'000, 91'000'000, 81'000'000}, {});
  FrameProfileSummary summary;
  AE_EXPECT_TRUE(!statistics.summarize(summary), "nao combinar epochs");
  statistics.record({1'020'000'000, 92'000'000, 82'000'000}, {});
  AE_EXPECT_TRUE(statistics.summarize(summary), "nova janela completa");
  AE_EXPECT_EQ(summary.elapsedMs, 20.0, "tempo suspenso nao entra em FPS");
}

AE_TEST(frame_statistics_rejeita_relogio_regressivo_sem_underflow) {
  const FrameCounters invalid[] = {{10, 9, 10}, {10, 10, 9}, {9, 10, 10}, {10, 10, 10}};
  for (const auto counters : invalid) {
    FrameStatistics statistics;
    statistics.configure(0, 1);
    statistics.record({10, 10, 10}, {});
    AE_EXPECT_EQ(statistics.record(counters, {}), FrameSampleResult::Invalid, "relogio invalido deve falhar");
    FrameProfileSummary summary;
    AE_EXPECT_TRUE(!statistics.summarize(summary), "amostra invalida nao gera relatorio");
  }
}

AE_TEST(frame_statistics_rejeita_nan_infinito_e_fase_negativa) {
  const double invalid[] = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -1};
  for (double value : invalid) {
    FrameStatistics statistics;
    statistics.configure(0, 1);
    statistics.record({}, {});
    AE_EXPECT_EQ(statistics.record({1, 1, 1}, {0, value, 0, 0}), FrameSampleResult::Invalid, "nao emitir dados invalidos");
  }
}

AE_TEST(frame_statistics_inclui_timestamp_gpu_sem_confundir_com_wall_cpu) {
  FrameStatistics statistics;
  statistics.configure(0, 1);
  statistics.record({}, {});
  RenderPhaseTimings phases{};
  phases.gpuFrameMs = 7.25;
  phases.gpuPassMs[static_cast<ae::u32>(ae::GpuPassClass::Opaque)] = 3.5;
  phases.gpuPassMs[static_cast<ae::u32>(ae::GpuPassClass::Coverage)] = 1.5;
  phases.gpuPassMs[static_cast<ae::u32>(ae::GpuPassClass::Sky)] = 1.25;
  phases.gpuPassMs[static_cast<ae::u32>(ae::GpuPassClass::Transparent)] = 1.0;
  phases.gpuPassMs[static_cast<ae::u32>(ae::GpuPassClass::Ui)] = 0.25;
  phases.gpuPassMs[static_cast<ae::u32>(ae::GpuPassClass::Hzb)] = 0.5;
  statistics.record({10'000'000, 2'000'000, 1'000'000}, phases);
  FrameProfileSummary summary;
  AE_EXPECT_TRUE(statistics.summarize(summary), "janela GPU pronta");
  AE_EXPECT_EQ(summary.metrics[static_cast<ae::u32>(FrameMetric::GpuFrame)].mean, 7.25,
               "timestamp GPU preservado separadamente");
  AE_EXPECT_EQ(summary.metrics[static_cast<ae::u32>(FrameMetric::ProcessCpu)].mean, 2.0,
               "GPU nao altera relogio CPU");
  AE_EXPECT_EQ(summary.metrics[frameMetricIndex(ae::GpuPassClass::Opaque)].mean, 3.5,
               "opaco solido separado");
  AE_EXPECT_EQ(summary.metrics[frameMetricIndex(ae::GpuPassClass::Coverage)].mean, 1.5,
               "folhagem alpha-mask separada do opaco");
  AE_EXPECT_EQ(summary.metrics[frameMetricIndex(ae::GpuPassClass::Sky)].mean, 1.25,
               "ceu separado");
  AE_EXPECT_EQ(summary.metrics[frameMetricIndex(ae::GpuPassClass::Transparent)].mean, 1.0,
               "transparencia separada");
  AE_EXPECT_EQ(summary.metrics[frameMetricIndex(ae::GpuPassClass::Ui)].mean, 0.25,
               "HUD deixa de cair em intervalo nao atribuido");
  AE_EXPECT_EQ(summary.metrics[frameMetricIndex(ae::GpuPassClass::Hzb)].mean, 0.5,
               "cadeia HZB medida fora do render pass principal");
}

// O nome da métrica e o rótulo do marcador de captura precisam vir da mesma
// tabela: se divergirem, uma captura AGI aponta para uma região com nome que
// não existe no relatório e a atribuição de custo deixa de ser verificável.
AE_TEST(frame_metric_nomeia_todas_as_classes_de_passe_na_ordem_do_frame) {
  AE_EXPECT_EQ(FrameMetricCount, FrameLevelMetricCount + ae::GpuPassClassCount,
               "espaco de metricas cobre frame e passes");
  for (ae::u32 pass = 0; pass < ae::GpuPassClassCount; ++pass) {
    const auto passClass = static_cast<ae::GpuPassClass>(pass);
    const ae::u32 metric = frameMetricIndex(passClass);
    AE_EXPECT_TRUE(metric >= FrameLevelMetricCount && metric < FrameMetricCount,
                   "indice de passe cai dentro do espaco de metricas");
    AE_EXPECT_TRUE(frameMetricName(metric) == ae::gpuPassClassMetricName(passClass),
                   "nome da metrica vem da tabela canonica");
    AE_EXPECT_TRUE(ae::gpuPassClassLabel(passClass) != nullptr,
                   "toda classe medida tem rotulo de captura");
  }
}

AE_TEST(frame_statistics_cpu_multithread_pode_exceder_wall_time) {
  FrameStatistics statistics;
  statistics.configure(0, 1);
  statistics.record({}, {});
  statistics.record({1'000'000, 4'000'000, 1'000'000}, {});
  FrameProfileSummary summary;
  AE_EXPECT_TRUE(statistics.summarize(summary), "processo pode usar varios cores");
  AE_EXPECT_EQ(summary.metrics[1].mean, 4.0, "nao truncar CPU ao wall time");
}

AE_TEST(frame_statistics_summary_nao_muta_amostras) {
  FrameStatistics statistics;
  statistics.configure(0, 1);
  FrameProfileSummary first, second;
  AE_EXPECT_TRUE(!statistics.summarize(first), "sem amostras nao ha estatistica");
  statistics.record({}, {});
  statistics.record({10'000'000, 2'000'000, 1'000'000}, {});
  statistics.summarize(first);
  statistics.summarize(second);
  AE_EXPECT_EQ(first.elapsedMs, second.elapsedMs, "ler nao consome janela");
  AE_EXPECT_EQ(first.metrics[1].p99, second.metrics[1].p99, "percentil repetivel");
}

AE_TEST(frame_statistics_configuracao_rejeita_capacidade_invalida) {
  FrameStatistics statistics;
  AE_EXPECT_TRUE(!statistics.configure(0, 0), "janela vazia deve falhar");
  AE_EXPECT_TRUE(!statistics.configure(0, FrameStatistics::Capacity + 1), "nao pode ultrapassar storage");
  AE_EXPECT_TRUE(statistics.configure(0, FrameStatistics::Capacity), "capacidade maxima valida");
}

AE_TEST(frame_statistics_baseline_nao_conta_cpu_anterior) {
  FrameStatistics statistics;
  statistics.configure(0, 1);
  AE_EXPECT_EQ(statistics.record({1'000'000, 90'000'000, 70'000'000}, {}), FrameSampleResult::WarmingUp, "baseline nao e frame");
  AE_EXPECT_EQ(statistics.record({17'000'000, 92'000'000, 71'000'000}, {}), FrameSampleResult::WindowReady, "um intervalo completo");
  FrameProfileSummary summary;
  AE_EXPECT_TRUE(statistics.summarize(summary), "janela pronta");
  AE_EXPECT_EQ(summary.metrics[1].mean, 2.0, "CPU deve ser delta, nao contador absoluto");
  AE_EXPECT_EQ(summary.metrics[2].mean, 1.0, "thread e processo separados");
  AE_EXPECT_EQ(summary.presentFps, 62.5, "FPS deve usar tempo real, nao media de FPS instantaneo");
}

AE_TEST(frame_statistics_percentis_nearest_rank_e_fases) {
  FrameStatistics statistics;
  statistics.configure(0, 100);
  FrameCounters counters{};
  statistics.record(counters, {});
  for (u64 i = 1; i <= 100; ++i) {
    counters.wallNs += i * 1'000'000;
    counters.processCpuNs += 2'000'000;
    counters.threadCpuNs += 1'000'000;
    statistics.record(counters, {1, 2, 3, 4});
  }
  FrameProfileSummary summary;
  AE_EXPECT_TRUE(statistics.summarize(summary), "janela completa");
  const auto &interval = summary.metrics[0];
  AE_EXPECT_EQ(interval.mean, 50.5, "media exata");
  AE_EXPECT_EQ(interval.p50, 50.0, "p50 nearest-rank");
  AE_EXPECT_EQ(interval.p95, 95.0, "p95 nearest-rank");
  AE_EXPECT_EQ(interval.p99, 99.0, "p99 nearest-rank");
  AE_EXPECT_EQ(interval.maximum, 100.0, "max preserva outlier");
  AE_EXPECT_EQ(summary.elapsedMs, 5050.0, "tempo total correto");
  AE_EXPECT_EQ(summary.metrics[3].mean, 1.0, "acquire separado");
  AE_EXPECT_EQ(summary.metrics[6].mean, 4.0, "present separado");
}

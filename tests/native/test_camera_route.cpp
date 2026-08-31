#include "harness.h"
#include "platform/camera_route.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

using namespace ae;
using namespace ae::platform;

namespace {
struct Fixture {
  std::string path = (std::filesystem::temp_directory_path() /
      ("aether-route-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".aeroute")).string();
  ~Fixture() {
    std::remove(path.c_str());
    std::remove((path + ".aether-tmp").c_str());
  }
};

CameraRouteSample makeSample(float x, float y, float z, float yaw, float pitch) {
  CameraRouteSample sample{};
  sample.position[0] = x;
  sample.position[1] = y;
  sample.position[2] = z;
  sample.yaw = yaw;
  sample.pitch = pitch;
  return sample;
}
} // namespace

AE_TEST(Camera_route_encode_decode_round_trips_samples) {
  const std::vector<CameraRouteSample> samples = {
      makeSample(1.0f, 2.0f, 3.0f, 0.1f, -0.2f),
      makeSample(4.0f, 5.0f, 6.0f, 0.3f, -0.4f),
      makeSample(7.0f, 8.0f, 9.0f, 0.5f, -0.6f),
  };
  std::vector<u8> buffer(encodedCameraRouteSize(static_cast<u32>(samples.size())));
  AE_EXPECT_TRUE(encodeCameraRoute(samples.data(), static_cast<u32>(samples.size()), 0xABCDEF01ull,
                                   60, buffer), "encode válido");

  CameraRouteData decoded;
  AE_EXPECT_TRUE(decodeCameraRoute(buffer, decoded), "decode válido");
  AE_EXPECT_EQ(decoded.sceneFingerprint, 0xABCDEF01ull, "fingerprint preservado");
  AE_EXPECT_EQ(decoded.tickRateHz, 60u, "tickRateHz preservado (metadado, não usado na leitura)");
  AE_EXPECT_EQ(decoded.samples.size(), samples.size(), "contagem de amostras preservada");
  for (usize index = 0; index < samples.size(); ++index) {
    AE_EXPECT_TRUE(decoded.samples[index].position[0] == samples[index].position[0] &&
                       decoded.samples[index].yaw == samples[index].yaw &&
                       decoded.samples[index].pitch == samples[index].pitch,
                   "amostra idêntica byte a byte");
  }
}

AE_TEST(Camera_route_decode_rejects_bad_magic_version_or_size) {
  const std::vector<CameraRouteSample> samples = {makeSample(0, 0, 0, 0, 0)};
  std::vector<u8> buffer(encodedCameraRouteSize(1));
  AE_EXPECT_TRUE(encodeCameraRoute(samples.data(), 1, 1, 60, buffer), "encode base válido");

  std::vector<u8> badMagic = buffer;
  badMagic[0] ^= 0xFF;
  CameraRouteData out;
  AE_EXPECT_TRUE(!decodeCameraRoute(badMagic, out), "magic corrompido rejeitado");

  std::vector<u8> badVersion = buffer;
  badVersion[4] = 0xFF;
  AE_EXPECT_TRUE(!decodeCameraRoute(badVersion, out), "versão desconhecida rejeitada");

  std::vector<u8> truncated(buffer.begin(), buffer.end() - 1);
  AE_EXPECT_TRUE(!decodeCameraRoute(truncated, out), "tamanho menor que o declarado rejeitado");

  std::vector<u8> tooShortHeader(buffer.begin(), buffer.begin() + 10);
  AE_EXPECT_TRUE(!decodeCameraRoute(tooShortHeader, out), "cabeçalho incompleto rejeitado");
}

AE_TEST(Camera_route_decode_rejects_non_finite_sample) {
  std::vector<CameraRouteSample> samples = {makeSample(0, 0, 0, 0, 0),
                                             makeSample(1, 1, 1, std::numeric_limits<float>::quiet_NaN(), 0)};
  std::vector<u8> buffer(encodedCameraRouteSize(2));
  // O encode em si também deve recusar dado não finito.
  AE_EXPECT_TRUE(!encodeCameraRoute(samples.data(), 2, 1, 60, buffer), "encode recusa amostra não finita");

  samples[1].yaw = 0.0f;
  AE_EXPECT_TRUE(encodeCameraRoute(samples.data(), 2, 1, 60, buffer), "encode válido após correção");
  // Corrompe um float diretamente nos bytes decodificados para simular um
  // arquivo malformado que passou pelo encode de outra versão.
  const usize secondSampleOffset = CameraRouteHeaderSize + sizeof(CameraRouteSample);
  const u32 nan = 0x7FC00000u;
  std::memcpy(buffer.data() + secondSampleOffset + 12, &nan, sizeof(nan)); // yaw do segundo registro
  CameraRouteData out;
  AE_EXPECT_TRUE(!decodeCameraRoute(buffer, out), "decode recusa amostra não finita nos bytes");
}

AE_TEST(Camera_route_encode_rejects_empty_oversized_or_wrong_buffer) {
  const CameraRouteSample sample = makeSample(0, 0, 0, 0, 0);
  std::vector<u8> buffer(encodedCameraRouteSize(1));
  AE_EXPECT_TRUE(!encodeCameraRoute(&sample, 0, 1, 60, buffer), "zero amostras rejeitado");
  AE_EXPECT_TRUE(!encodeCameraRoute(nullptr, 1, 1, 60, buffer), "ponteiro nulo rejeitado");
  AE_EXPECT_TRUE(!encodeCameraRoute(&sample, CameraRouteMaximumTicks + 1, 1, 60, buffer),
                 "acima do teto de segurança rejeitado");
  std::vector<u8> wrongSize(buffer.size() + 1);
  AE_EXPECT_TRUE(!encodeCameraRoute(&sample, 1, 1, 60, wrongSize), "buffer de saída com tamanho errado rejeitado");
}

AE_TEST(Camera_route_recorder_writes_and_player_loads_with_frame_ordinal_looping) {
  Fixture fixture;
  CameraRouteRecorder recorder;
  recorder.reserve(4);
  recorder.pushSample(FreeCameraState{{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f});
  recorder.pushSample(FreeCameraState{{1.0f, 0.0f, 0.0f}, 0.1f, 0.0f});
  recorder.pushSample(FreeCameraState{{2.0f, 0.0f, 0.0f}, 0.2f, 0.0f});
  recorder.pushSample(FreeCameraState{{3.0f, 0.0f, 0.0f}, 0.3f, 0.0f});
  AE_EXPECT_EQ(recorder.sampleCount(), 4u, "quatro amostras registradas");
  AE_EXPECT_TRUE(recorder.writeToFile(fixture.path.c_str(), 0x1122334455667788ull, 60), "gravação bem-sucedida");

  CameraRoutePlayer player;
  AE_EXPECT_TRUE(player.loadFromFile(fixture.path.c_str()), "leitura bem-sucedida");
  AE_EXPECT_TRUE(player.loaded(), "rota carregada");
  AE_EXPECT_EQ(player.data().sceneFingerprint, 0x1122334455667788ull, "fingerprint sobrevive ao arquivo");
  AE_EXPECT_EQ(player.data().samples.size(), 4u, "quatro amostras lidas do arquivo");

  AE_EXPECT_TRUE(player.sample(0).position[0] == 0.0f, "frame 0 amostra 0");
  AE_EXPECT_TRUE(player.sample(3).position[0] == 3.0f, "frame 3 amostra 3");
  AE_EXPECT_TRUE(player.sample(4).position[0] == 0.0f, "looping por módulo: frame 4 volta à amostra 0");
  AE_EXPECT_TRUE(player.sample(7).position[0] == 3.0f, "looping por módulo: frame 7 é amostra 3");
  AE_EXPECT_TRUE(player.sample(4004).position[0] == 0.0f, "looping estável após muitas voltas (soak longo)");
}

AE_TEST(Camera_route_player_fails_closed_for_missing_or_corrupt_file) {
  CameraRoutePlayer player;
  AE_EXPECT_TRUE(!player.loadFromFile("caminho/que/nao/existe.aeroute"), "arquivo ausente rejeitado");
  AE_EXPECT_TRUE(!player.loaded(), "não carregado após falha");

  Fixture fixture;
  std::FILE *garbage = std::fopen(fixture.path.c_str(), "wb");
  AE_EXPECT_TRUE(garbage != nullptr, "consegue criar arquivo de lixo para o teste");
  const char bytes[] = "isto nao e uma rota valida";
  std::fwrite(bytes, 1, sizeof(bytes), garbage);
  std::fclose(garbage);
  AE_EXPECT_TRUE(!player.loadFromFile(fixture.path.c_str()), "conteúdo corrompido rejeitado");
}

AE_TEST(Camera_route_player_sample_on_unloaded_route_returns_default_state) {
  CameraRoutePlayer player;
  const FreeCameraState state = player.sample(42);
  AE_EXPECT_TRUE(!player.loaded(), "player recém-criado não está carregado");
  AE_EXPECT_TRUE(state.position[0] == 0.0f && state.position[1] == 0.0f && state.position[2] == 0.0f &&
                     state.yaw == 0.0f && state.pitch == 0.0f,
                 "amostra sem rota carregada falha aberta com pose neutra, nunca crasha");
}

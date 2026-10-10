#pragma once
#include "resources/image_decode.h"
#include <atomic>

namespace ae::resources {
// Classic TIFF baseline, not a Photoshop document reader. Source preservation
// is owned by the import transaction; these choices describe prepared texels.
enum class TiffUndefinedExtra : u8 { Reject, Ignore, Alpha };
struct TiffDecodePolicy {
  TiffUndefinedExtra undefinedExtra=TiffUndefinedExtra::Reject;
  bool quantize16=false;
  // ICC bytes remain in the original. This explicit approximation does not
  // claim to perform a color-management transform.
  bool ignoreColorProfile=false;
};
struct TiffInfo {
  u32 width=0,height=0;
  u16 bits=0,samples=0,orientation=1,extra=0;
  bool littleEndian=false,undefinedExtra=false,matrixSrgb=false;
  std::span<const u8> colorProfile;
};
bool isTiff(std::span<const u8> bytes);
bool inspectTiff(std::span<const u8> bytes,const ImageDecodeLimits &limits,TiffInfo &out,std::string &diagnostic);
bool tiffPreparationLosses(const TiffInfo &,const TiffDecodePolicy &,std::vector<std::string> &losses,std::string &diagnostic);
// Losses must be displayed and persisted by the authoring consumer.
bool decodeTiffRgba8(std::span<const u8> bytes,const ImageDecodeLimits &limits,
                    const TiffDecodePolicy &,DecodedImage &out,std::vector<std::string> &losses,
                    std::string &diagnostic,const std::atomic<bool> *cancel=nullptr);
}

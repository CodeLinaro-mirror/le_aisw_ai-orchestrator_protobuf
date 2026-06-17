
#include "upb/wire/decode_fast/cardinality.h"

#include "upb/wire/internal/decoder.h"
#include "upb/wire/internal/eps_copy_input_stream.h"

UPB_PRESERVE_MOST
const char* upb_DecodeFast_IsDoneFallback(upb_Decoder* d, const char* ptr) {
  int overrun;
  upb_IsDoneStatus status = UPB_PRIVATE(upb_EpsCopyInputStream_IsDoneStatus)(
      &d->input, ptr, &overrun);
  UPB_ASSERT(status == kUpb_IsDoneStatus_NeedFallback);
  return UPB_PRIVATE(upb_EpsCopyInputStream_IsDoneFallback)(EPS(d), ptr,
                                                            overrun);
}

UPB_PRESERVE_MOST
bool _upb_DecodeFast_DecodeSizeSlow(const char** pp, int* size) {
  const char* ptr = *pp;
  uint32_t val = (ptr[0] & 0x7f) | ((ptr[1] & 0x7f) << 7);
  for (int i = 2; i < 5; i++) {
    uint32_t byte = (uint8_t)ptr[i];
    val |= (byte & 0x7f) << (i * 7);
    if (!(byte & 0x80)) {
      if (UPB_UNLIKELY(val > INT32_MAX)) return false;
      *pp = ptr + i + 1;
      *size = val;
      return true;
    }
  }
  return false;
}

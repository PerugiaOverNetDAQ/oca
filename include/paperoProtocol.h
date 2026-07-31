#ifndef PAPERO_PROTOCOL_H
#define PAPERO_PROTOCOL_H

#include <cstdint>

namespace paperoProtocol {

// PAPERO register-0 command bits shared by the Calib and HEF_Calib gateware.
constexpr uint32_t kCounterResetBit = 1u << 1;
constexpr uint32_t kRunRequestBit  = 1u << 4;
constexpr uint32_t kEventEnableBit = 1u << 16;
constexpr uint32_t kForceCalibBit  = 1u << 17;
constexpr uint32_t kThresholdBit   = 1u << 18;
constexpr uint32_t kAutoCalibBit   = 1u << 19;
constexpr uint32_t kSaveCalibBit   = 1u << 20;
constexpr uint32_t kDaqModeShift   = 24u;
constexpr uint32_t kDaqModeMask    = 3u << kDaqModeShift;

constexpr uint16_t kDefaultLowThreshold = 0x0030u;
constexpr uint16_t kDefaultHighThreshold = 0x0070u;

constexpr uint32_t kStopCommand = 0u;
constexpr uint32_t kBeamCommand =
    kRunRequestBit | kEventEnableBit;
constexpr uint32_t kCalibrationCommand =
    kRunRequestBit | kForceCalibBit | kSaveCalibBit;
constexpr uint32_t kMixedCommand =
    kRunRequestBit | kEventEnableBit | kForceCalibBit | kSaveCalibBit;

constexpr uint32_t BuildAcquisitionCommand(bool eventEnable,
                                            bool forceCalibration,
                                            bool autoCalibration,
                                            bool saveCalibration,
                                            bool applyThresholds,
                                            uint32_t daqMode) {
  return kRunRequestBit |
         (eventEnable ? kEventEnableBit : 0u) |
         (forceCalibration ? kForceCalibBit : 0u) |
         (applyThresholds ? kThresholdBit : 0u) |
         (autoCalibration ? kAutoCalibBit : 0u) |
         (saveCalibration ? kSaveCalibBit : 0u) |
         ((daqMode & 3u) << kDaqModeShift);
}

constexpr uint32_t ConfigureAcquisition(uint32_t command,
                                        uint32_t daqMode) {
  return (command & ~kDaqModeMask) |
         ((daqMode & 3u) << kDaqModeShift) |
         kThresholdBit;
}

constexpr uint32_t PackThresholds(uint16_t low, uint16_t high) {
  return (static_cast<uint32_t>(high) << 16) |
         static_cast<uint32_t>(low);
}

// FPGA readback register 11 is exposed at global register-array address 27.
constexpr uint32_t kCalibrationStatusRegister = 27u;
constexpr uint32_t kCalibrationValidMask = 1u << 0;
constexpr uint32_t kRunIdleMask = 1u << 1;

// HEF raw/calibration packet geometry.
constexpr uint32_t kHefPayloadWords = 896u;
constexpr uint32_t kHefPacketLength = kHefPayloadWords + 12u;
constexpr uint32_t kHefPacketWords = kHefPacketLength + 1u;
constexpr uint32_t kHefMaxPayloadWords = 2u * kHefPayloadWords;
constexpr uint32_t kHefMaxPacketLength = kHefMaxPayloadWords + 12u;
constexpr uint32_t kHefMaxPacketWords = kHefMaxPacketLength + 1u;
constexpr uint32_t kFastFifoDepth = 4096u;
constexpr uint32_t kFastFifoAlmostEmpty = 1u;
constexpr uint32_t kFastFifoAlmostFull =
    kFastFifoDepth - kHefMaxPacketWords - 3u;

static_assert(kBeamCommand == 0x00010010u, "Unexpected BEAM command");
static_assert(kCalibrationCommand == 0x00120010u,
              "Unexpected CAL command");
static_assert(kMixedCommand == 0x00130010u, "Unexpected MIX command");
static_assert(ConfigureAcquisition(kBeamCommand, 3u) == 0x03050010u,
              "Unexpected configured BEAM command");
static_assert(PackThresholds(kDefaultLowThreshold,
                             kDefaultHighThreshold) == 0x00700030u,
              "Unexpected default threshold word");
static_assert(kHefPacketLength == 908u, "Unexpected HEF packet length");
static_assert(kHefPacketWords == 909u, "Unexpected HEF packet size");
static_assert(kHefMaxPacketWords == 1805u,
              "Unexpected maximum HEF packet size");

}  // namespace paperoProtocol

#endif

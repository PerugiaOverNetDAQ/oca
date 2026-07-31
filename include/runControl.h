#ifndef RUN_CONTROL_H
#define RUN_CONTROL_H

#include <cstdint>

namespace run_control {

enum class RunMode : uint16_t {
  Cal = 0u,
  Daq = 1u,
  Mix = 2u,
  Dump = 3u
};

struct Command {
  RunMode mode;
  bool externalTrigger;
  bool saveCalibration;
};

constexpr uint16_t kExtendedMarker = 0x8000u;
constexpr uint16_t kSaveCalibration = 0x0008u;
constexpr uint16_t kExternalTrigger = 0x0004u;
constexpr uint16_t kModeMask = 0x0003u;
constexpr uint16_t kReservedMask = 0x7ff0u;

inline uint16_t Encode(RunMode mode, bool externalTrigger,
                       bool saveCalibration) {
  return static_cast<uint16_t>(
    kExtendedMarker |
    (saveCalibration ? kSaveCalibration : 0u) |
    (externalTrigger ? kExternalTrigger : 0u) |
    static_cast<uint16_t>(mode));
}

inline bool Decode(uint16_t value, Command& command) {
  if ((value & kExtendedMarker) != 0u) {
    if ((value & kReservedMask) != 0u) {
      return false;
    }

    command.mode = static_cast<RunMode>(value & kModeMask);
    command.externalTrigger = (value & kExternalTrigger) != 0u;
    command.saveCalibration = (value & kSaveCalibration) != 0u;

    // DUMP has neither a trigger-source nor a NOSAVE variant.
    if (command.mode == RunMode::Dump &&
        (command.externalTrigger || !command.saveCalibration)) {
      return false;
    }
    return true;
  }

  switch (value) {
    case 0x0000u:
    case 0x0004u:
      command = {RunMode::Cal, false, true};
      return true;
    case 0x0002u:
      command = {RunMode::Daq, true, false};
      return true;
    case 0x0001u:
      command = {RunMode::Mix, false, true};
      return true;
    default:
      return false;
  }
}

inline const char* Name(RunMode mode) {
  switch (mode) {
    case RunMode::Cal:
      return "CAL";
    case RunMode::Daq:
      return "DAQ";
    case RunMode::Mix:
      return "MIX";
    case RunMode::Dump:
      return "DUMP";
  }
  return "UNKNOWN";
}

inline uint16_t ByteSwap16(uint16_t value) {
  return static_cast<uint16_t>((value << 8) | (value >> 8));
}

inline uint32_t PackStartWord(uint16_t runNumber, uint16_t controlWord) {
  return static_cast<uint32_t>(ByteSwap16(runNumber)) |
         (static_cast<uint32_t>(ByteSwap16(controlWord)) << 16);
}

}  // namespace run_control

#endif  // RUN_CONTROL_H

#ifndef JT808_COMMON_JT808_UPGRADE_H_
#define JT808_COMMON_JT808_UPGRADE_H_

#include <stdint.h>


enum Jt808UpgradeType {
  kDeviceUpgrade = 0x0,
  kGpsUpgrade,
};

#pragma pack(push, 1)

struct UpgradeInfo {
  int upgrade_type;
  char version_id[16];
  char file_path[256];
};

#pragma pack(pop)

#endif // JT808_COMMON_JT808_UPGRADE_H_

#pragma once

/// Canonical metric key names used by MetricsService and all bridges.
/// Include this header instead of scattering string literals.

namespace embed::metrics_keys {

inline constexpr const char* kCpuUsagePercent  = "cpuUsagePercent";
inline constexpr const char* kFreeHeap         = "freeHeap";
inline constexpr const char* kMinFreeHeap      = "minFreeHeap";
inline constexpr const char* kFreePsram        = "freePsram";
inline constexpr const char* kMinFreePsram     = "minFreePsram";
inline constexpr const char* kLargestFreeBlock = "largestFreeBlock";
inline constexpr const char* kUptimeSeconds    = "uptimeSeconds";
inline constexpr const char* kStorageTotalBytes = "storageTotalBytes";
inline constexpr const char* kStorageUsedBytes = "storageUsedBytes";
inline constexpr const char* kWifiConnected    = "wifiConnected";
inline constexpr const char* kWifiRssi         = "wifiRssi";
inline constexpr const char* kWifiIp           = "wifiIp";
inline constexpr const char* kVersion          = "version";

} // namespace embed::metrics_keys

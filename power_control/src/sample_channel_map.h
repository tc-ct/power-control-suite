#ifndef SAMPLE_CHANNEL_MAP_H
#define SAMPLE_CHANNEL_MAP_H

#include <array>

#include "proto_pkg.h"

namespace sample_channel_map {

inline constexpr std::array<int, SAMPLE_DATA_COUNT> kDisplayToRaw = {
    7, 8, 9, 16, 25, 17, 3, 1, 0, 2, 26, 14, 13, 5, 6, 15, 12, 10, 11, 4, 18, 19, 20, 21, 22, 23, 24
};

inline constexpr std::array<int, SAMPLE_DATA_COUNT> buildRawToDisplay() {
    std::array<int, SAMPLE_DATA_COUNT> rawToDisplay{};
    for (int i = 0; i < SAMPLE_DATA_COUNT; ++i) {
        rawToDisplay[i] = -1;
    }
    for (int displayId = 0; displayId < SAMPLE_DATA_COUNT; ++displayId) {
        const int rawId = kDisplayToRaw[displayId];
        if (rawId >= 0 && rawId < SAMPLE_DATA_COUNT) {
            rawToDisplay[rawId] = displayId;
        }
    }
    return rawToDisplay;
}

inline constexpr std::array<int, SAMPLE_DATA_COUNT> kRawToDisplay = buildRawToDisplay();

inline constexpr int displayToRaw(int displayId) {
    return (displayId >= 0 && displayId < SAMPLE_DATA_COUNT) ? kDisplayToRaw[displayId] : -1;
}

inline constexpr int rawToDisplay(int rawId) {
    return (rawId >= 0 && rawId < SAMPLE_DATA_COUNT) ? kRawToDisplay[rawId] : -1;
}

} // namespace sample_channel_map

#endif // SAMPLE_CHANNEL_MAP_H


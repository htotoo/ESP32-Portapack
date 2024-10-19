
#ifndef FEATURES_HPP
#define FEATURES_HPP

#include <iostream>
#include <bitset>
#include <cstdint>

enum class SupportedFeatures : uint64_t
{
    FEAT_NONE = 0,
    FEAT_EXT_APP = 1 << 0,
    FEAT_UART = 1 << 1,
    FEAT_GPS = 1 << 2,
    FEAT_ORIENTATION = 1 << 3,
    FEAT_ENVIRONMENT = 1 << 4,
    FEAT_LIGHT = 1 << 5,
    FEAT_DISPLAY = 1 << 6
};

class ChipFeatures
{
private:
    uint64_t features; // Store the features as bitmask

public:
    ChipFeatures() : features(static_cast<uint64_t>(SupportedFeatures::FEAT_NONE)) {}

    void reset()
    {
        features = static_cast<uint64_t>(SupportedFeatures::FEAT_NONE);
    }

    // Enable a feature
    void enableFeature(SupportedFeatures feature)
    {
        features |= static_cast<uint64_t>(feature);
    }

    // Disable a feature
    void disableFeature(SupportedFeatures feature)
    {
        features &= ~static_cast<uint64_t>(feature);
    }

    // Check if a feature is enabled
    bool hasFeature(SupportedFeatures feature) const
    {
        return (features & static_cast<uint64_t>(feature)) != 0;
    }

    // Toggle a feature on/off
    void toggleFeature(SupportedFeatures feature)
    {
        features ^= static_cast<uint64_t>(feature);
    }

    uint64_t getFeatures() const
    {
        return features;
    }
};

#endif // FEATURES_HPP
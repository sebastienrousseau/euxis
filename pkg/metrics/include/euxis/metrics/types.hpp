#pragma once

#include <string>
#include <map>

namespace euxis::metrics {

/**
 * @brief Raw data point for a single measurement.
 */
struct MetricRecord {
    std::string name;           ///< Measurement name (e.g. "cpu_usage").
    double value;               ///< Numeric value.
    long timestamp;             ///< Generation time.
    std::map<std::string, std::string> labels; ///< Dimensions (e.g. agent="code").
};

} // namespace euxis::metrics

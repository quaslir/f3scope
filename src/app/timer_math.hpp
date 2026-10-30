#pragma once
#include <cstdint>
#include <optional>
namespace app::timer_math {

    struct Result {
        uint32_t arr{}, psc{}, sample_rate{};
    };

    [[nodiscard]] std::optional<Result> calculate(uint32_t rate_hz, uint32_t target_rate_hz);

}

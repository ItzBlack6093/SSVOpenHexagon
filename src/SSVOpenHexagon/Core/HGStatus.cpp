// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

#include "SSVOpenHexagon/Core/HGStatus.hpp"

#include "SSVOpenHexagon/Utils/Clock.hpp"

#include <chrono>

namespace hg {

void HexagonGameStatus::start() noexcept
{
    // Reset time and custom score:
    resetTime();
    customScore = 0;

    // Signal that we started:
    started = true;
}

[[nodiscard]] double HexagonGameStatus::getIncrementTimeSeconds() const noexcept
{
    return currentIncrementTime / 60.0;
}

[[nodiscard]] double HexagonGameStatus::getTimeSeconds() const noexcept
{
    return getPlayedAccumulatedFrametimeInSeconds();
}

[[nodiscard]] HRTimePoint HexagonGameStatus::getCurrentTP() const noexcept
{
    return HRTimePoint{std::chrono::milliseconds{
        (int64_t)(getTotalAccumulatedFrametimeInSeconds() * 1000.0)}};
}

[[nodiscard]] HRTimePoint HexagonGameStatus::getTimeTP() const noexcept
{
    return HRTimePoint{std::chrono::milliseconds{
        (int64_t)(getPlayedAccumulatedFrametimeInSeconds() * 1000.0)}};
}

[[nodiscard]] HRTimePoint HexagonGameStatus::getLevelStartTP() const noexcept
{
    return HRTimePoint{};
}

[[nodiscard]] bool HexagonGameStatus::isTimePaused() const noexcept
{
    return currentPause > 0.0;
}

void HexagonGameStatus::pauseTime(const double seconds) noexcept
{
    currentPause += seconds * 60.0;
}

void HexagonGameStatus::resetIncrementTime() noexcept
{
    currentIncrementTime = 0.0;
}

void HexagonGameStatus::resetTime()
{
    totalFrametimeAccumulator = 0.0;
    playedFrametimeAccumulator = 0.0;
    pausedFrametimeAccumulator = 0.0;
    currentPause = 0.1 * 60;
    currentIncrementTime = 0.0;
}

void HexagonGameStatus::accumulateFrametime(const double ft) noexcept
{
    // TODO (P2): double-check what to do with remainder

    totalFrametimeAccumulator += ft;

    // double pauseRemainder = 0.0;
    if (currentPause > 0.0)
    {
        currentPause -= ft;
        // if(currentPause < 0.0)
        // {
        //     pauseRemainder = -currentPause;
        // }
    }

    // if(currentPause <= 0.0)
    else
    {
        playedFrametimeAccumulator += ft;
        currentIncrementTime += ft;
        // playedFrametimeAccumulator += pauseRemainder;
    }
}

void HexagonGameStatus::updateCustomScore(const float score) noexcept
{
    customScore = score;
}

[[nodiscard]] double
HexagonGameStatus::getTotalAccumulatedFrametime() const noexcept
{
    return totalFrametimeAccumulator;
}

[[nodiscard]] double
HexagonGameStatus::getTotalAccumulatedFrametimeInSeconds() const noexcept
{
    return getTotalAccumulatedFrametime() / 60.0;
}

[[nodiscard]] double
HexagonGameStatus::getPlayedAccumulatedFrametime() const noexcept
{
    return playedFrametimeAccumulator;
}

[[nodiscard]] double
HexagonGameStatus::getPlayedAccumulatedFrametimeInSeconds() const noexcept
{
    return getPlayedAccumulatedFrametime() / 60.0;
}

[[nodiscard]] double
HexagonGameStatus::getPausedAccumulatedFrametime() const noexcept
{
    return pausedFrametimeAccumulator;
}

[[nodiscard]] double
HexagonGameStatus::getPausedAccumulatedFrametimeInSeconds() const noexcept
{
    return getPausedAccumulatedFrametime() / 60.0;
}

[[nodiscard]] float HexagonGameStatus::getCustomScore() const noexcept
{
    return customScore;
}

} // namespace hg

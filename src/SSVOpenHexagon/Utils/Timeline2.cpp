// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

#include "SSVOpenHexagon/Utils/Timeline2.hpp"

#include "SSVOpenHexagon/Global/Assert.hpp"
#include "SSVOpenHexagon/Utils/TinyVariant.hpp"

#include <chrono>
#include <SFML/Base/Optional.hpp>

namespace hg::Utils {

void timeline2::clear()
{
    _actions.clear();
}


void timeline2::append_wait_for(const duration d)
{
    _actions.emplace_back(
        vittorioromeo::impl::tinyvariant_inplace_type_t<action_wait_for>{}, d);
}

void timeline2::append_wait_for_seconds(const double s)
{
    append_wait_for(std::chrono::milliseconds(static_cast<int>(s * 1000.0)));
}

void timeline2::append_wait_for_sixths(const double s)
{
    append_wait_for_seconds(s / 60.0);
}

void timeline2::append_wait_until(const time_point tp)
{
    _actions.emplace_back(
        vittorioromeo::impl::tinyvariant_inplace_type_t<action_wait_until>{},
        tp);
}

[[nodiscard]] std::size_t timeline2::size() const noexcept
{
    return _actions.size();
}

[[nodiscard]] timeline2::action& timeline2::action_at(
    const std::size_t i) noexcept
{
    SSVOH_ASSERT(i < size());
    return _actions[i];
}

timeline2_runner::outcome timeline2_runner::update(
    timeline2& timeline, const time_point tp)
{
    if (_current_idx >= timeline.size())
    {
        // Empty timeline or reached the end.
        return outcome::finished;
    }

    while (_current_idx < timeline.size())
    {
        timeline2::action& a = timeline.action_at(_current_idx);

        const outcome o = a.linear_match(
            [&](timeline2::action_do& x)
            {
                x._func();
                return outcome::proceed;
            },
            [&](timeline2::action_wait_for& x)
            {
                if (!_wait_start_tp.hasValue())
                {
                    // Just started waiting.
                    _wait_start_tp.emplace(tp);
                }

                const auto elapsed = tp - _wait_start_tp.value();
                if (elapsed < x._duration)
                {
                    // Still waiting.
                    return outcome::waiting;
                }

                // Finished waiting.
                _wait_start_tp.reset();
                return outcome::proceed;
            },
            [&](timeline2::action_wait_until& x)
            {
                if (tp < x._time_point)
                {
                    // Still waiting.
                    return outcome::waiting;
                }

                // Finished waiting.
                return outcome::proceed;
            }, //
            [&](timeline2::action_wait_until_fn& x)
            {
                if (tp < x._time_point_fn())
                {
                    // Still waiting.
                    return outcome::waiting;
                }

                // Finished waiting.
                return outcome::proceed;
            } //
        );

        if (o == outcome::proceed)
        {
            ++_current_idx;
            continue;
        }

        if (o == outcome::waiting)
        {
            return o;
        }

        SSVOH_ASSERT(false);
    }

    return outcome::finished;
}

} // namespace hg::Utils

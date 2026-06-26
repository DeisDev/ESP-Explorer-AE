#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ESPExplorerAE
{
    class TextInputSession
    {
    public:
        bool Begin(std::uint32_t target)
        {
            if (waiting) return false;
            owner = target;
            waiting = true;
            abandoned = false;
            result.reset();
            return true;
        }

        // Steam has no supported close call for this dialog. Keep an abandoned
        // request outstanding until its callback so it cannot satisfy a new one.
        void Abandon() { abandoned = true; result.reset(); }
        void Unavailable() { waiting = false; result.reset(); }
        bool IsWaiting() const { return waiting; }
        bool IsOpen() const { return waiting || result.has_value(); }

        template <class ReadSubmittedText>
        void Dismiss(bool submitted, ReadSubmittedText&& read)
        {
            if (!waiting) return;
            waiting = false;
            if (submitted && !abandoned) result = read();
        }

        std::optional<std::string> Take(std::uint32_t target)
        {
            if (target != owner) return std::nullopt;
            auto value = std::move(result);
            result.reset();
            return value;
        }

    private:
        std::uint32_t owner{};
        bool waiting{};
        bool abandoned{};
        std::optional<std::string> result;
    };
}

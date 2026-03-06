#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace ESPExplorerAE::MainWindowPopups
{
    using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

    void RequestActionConfirmation(std::string title, std::string message, std::function<void()> callback);
    void OpenGlobalValuePopup(std::uint32_t formID);
    void Draw(const LocalizeFn& localize);
}
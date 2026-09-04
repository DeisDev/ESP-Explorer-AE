#pragma once

#include "Core/FormEntry.h"
#include "Core/Actions.h"

#include <functional>
#include <vector>

namespace ESPExplorerAE
{
    class ItemGrantPopup
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

        struct View
        {
            LocalizeFn localize;
            std::uint64_t session{};
            bool gameplayReady{};
            float fontSize{ 20.0f };
        };
        struct Submission { std::uint64_t revision{}; std::vector<ActionRequest> actions; };
        struct Requests { std::optional<Submission> submit; };

        void Open(const FormEntry& entry, std::uint64_t session);
        void Open(const std::vector<FormEntry>& entries, std::uint64_t session);
        void Close();
        void Draw(const View& view, Requests& requests);
        void ResolveSubmit(std::uint64_t revision, ActionAdmission admission);
        bool Visible() const { return state.visible || state.openRequested; }

    private:
        struct State
        {
            struct ItemState
            {
                FormEntry entry{};
                int quantity{ 1 };
                int ammoQuantity{ 100 };
                std::uint32_t ammoFormID{ 0 };
                bool includeAmmo{ false };
            };

            bool openRequested{ false };
            bool visible{ false };
            bool closeRequested{};
            bool pending{};
            std::uint64_t revision{};
            std::uint64_t session{};
            ActionAdmission admission{ ActionAdmission::Accepted };
            std::vector<ItemState> items{};
        };

        State state;
    };
}

#pragma once

#include "Core/Actions.h"

#include <deque>
#include <functional>
#include <string>
#include <string_view>

namespace ESPExplorerAE
{
    class MainWindowPopups
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;
        enum class Origin { Browser, Inventory, Global };
        struct View
        {
            LocalizeFn localize;
            std::uint64_t session{};
            bool gameplayReady{};
            float fontSize{ 20.0f };
            std::string toggleKeyName;
        };
        struct Submission
        {
            std::uint64_t revision{};
            Origin origin{};
            std::vector<ActionRequest> actions;
        };
        struct Requests
        {
            std::vector<Submission> submissions;
            bool helpDismissed{};
        };

        void RequestActionConfirmation(std::string title, std::string message,
            std::vector<ActionRequest> actions, Origin origin = Origin::Browser);
        void OpenGlobalValuePopup(std::uint32_t formID, std::string editorID, std::uint64_t session);
        void OpenHelpOverlay();
        void OpenFirstRunHelpOverlay(bool dismissed);
        void HandleMenuVisibilityChanged(bool visible);
        void Draw(const View& view, Requests& requests);
        void ResolveSubmit(std::uint64_t revision, ActionAdmission admission);

    private:
        struct Confirmation
        {
            std::string title;
            std::string message;
            Submission submission;
            bool openRequested{ true };
            bool visible{ true };
            bool pending{};
            ActionAdmission admission{ ActionAdmission::Accepted };
        };
        struct GlobalValue
        {
            std::uint32_t formID{};
            std::string editorID;
            float value{};
            std::uint64_t session{};
            std::uint64_t revision{};
            bool openRequested{};
            bool visible{};
            bool pending{};
            ActionAdmission admission{ ActionAdmission::Accepted };
        };
        struct HelpOverlay { bool openRequested{}; bool visible{}; };

        void CloseConfirmation();
        void CloseGlobal();
        void RenderConfirmActionPopup(const View& view, Requests& requests);
        void RenderGlobalValuePopup(const View& view, Requests& requests);
        void RenderHelpOverlay(const View& view, Requests& requests);

        std::deque<Confirmation> confirmations;
        GlobalValue globalValuePopup;
        HelpOverlay helpOverlay;
        std::uint64_t nextRevision{};
        bool closeConfirmationRequested{};
        bool closeGlobalRequested{};
    };
}

// The conversation's "Pins" tab: its pinned messages in the message area's
// place, drawn by a MessageList in pins mode — so each looks and behaves as
// it does in the Messages tab (text, files and pictures, link previews,
// reactions, the reply bar, the hover toolbar and the message menu), under
// the heading "Pinned messages": oldest first, each in a card of its own with
// its day and time beside the name, without day dividers or the "Pinned"
// strip (everything here is pinned). A click on a message, or the menu's first item "Go to
// message", opens it in the Messages tab (inside its thread for a reply); "Unpin from channel"
// takes it off the list.
//
// The list is the service's (Backend::loadPins), so it holds the pins the
// loaded history doesn't; it is asked for on every open and again when a
// message of the conversation is pinned or unpinned while the tab is up.
// While it loads, when there are none, or when it couldn't be loaded, a
// status line shows instead.
#pragma once

#include "screens/common/context.h"
#include "ui/ui.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace screens {
class MessageList;
}

namespace shell {

class PinsPage : public ui::View {
public:
    explicit PinsPage(screens::Context &ctx);
    ~PinsPage() override;

    // Shows `conv`'s pins: "Loading…" until the list is in (the messages it
    // already shows of the same conversation stay meanwhile).
    void           open(model::ConvRef conv);
    // Drops them (another conversation, a workspace switch).
    void           clear();
    model::ConvRef conv() const { return _conv; }

    // "Go to message": the Messages tab, then the jump.
    std::function<void(model::ConvRef, model::Ts ts, model::Ts thread)> onOpenMessage;

    // Tests: what the page shows, oldest first.
    size_t                count() const { return _pins.size(); }
    const std::string    &statusText() const { return _statusText; }
    model::Ts             ts(size_t i) const { return i < _pins.size() ? _pins[i].ts : 0; }
    std::string           author(size_t i) const;
    void                  goTo(size_t i);  // the menu's "Go to message"
    void                  unpin(size_t i); // the menu's "Unpin from channel"
    // The list drawing them (null in a build without the messages screens).
    screens::MessageList *list() const { return _messages; }

private:
    void load();
    void show(); // the list, or the status line when there is nothing to list
    void setStatus(std::string text);
    void onChange(const model::Change &ch);
    void dropped(model::Ts ts); // unpinned from here
    bool shows(model::Ts ts) const;
    void later(std::function<void()> fn); // on the next loop turn, if the page still exists

    screens::Context           &_ctx;
    ui::Label                  *_status   = nullptr;
    ui::Label                  *_heading  = nullptr; // "Pinned messages", over the list
    screens::MessageList       *_messages = nullptr;
    std::vector<model::Message> _pins; // oldest first
    std::string                 _statusText;
    model::ConvRef              _conv    = model::kNoConv;
    int                         _request = 0; // the newest loadPins: older answers are dropped
    bool                        _loaded = false, _reloadQueued = false;
    model::Store::ObserverId    _observer = 0;
    std::shared_ptr<int>        _alive    = std::make_shared<int>(0);
};

} // namespace shell

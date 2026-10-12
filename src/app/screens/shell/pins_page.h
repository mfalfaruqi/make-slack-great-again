// The conversation's "Pins" tab: its pinned messages as cards in the
// message area's place, the latest pin first — each the message as a chat
// row (avatar, name, time, its text; a click jumps to it in the Messages tab,
// inside its thread for a reply) over a footer with "Pinned by …" and
// "Unpin". The list is the service's (Backend::loadPins), so it holds the
// pins the loaded history doesn't; it is asked for on every open and again
// when a message of the conversation is pinned or unpinned while the tab is
// up.
#pragma once

#include "screens/common/context.h"
#include "screens/shell/avatars.h"
#include "ui/ui.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace shell {

class PinsPage : public ui::View {
public:
    PinsPage(screens::Context &ctx, Avatars &avatars);
    ~PinsPage() override;

    // Shows `conv`'s pins: "Loading…" until the list is in (the cards it
    // already shows of the same conversation stay meanwhile).
    void           open(model::ConvRef conv);
    // Drops the cards (another conversation, a workspace switch).
    void           clear();
    model::ConvRef conv() const { return _conv; }

    // A card's message: the Messages tab, then the jump.
    std::function<void(model::ConvRef, model::Ts ts, model::Ts thread)> onOpenMessage;

    // Tests: what the page shows.
    size_t             cardCount() const { return _cards.size(); }
    const std::string &statusText() const { return _statusText; }
    std::string        author(size_t i) const;   // the row's name
    std::string        pinnedBy(size_t i) const; // the footer's line
    void               activate(size_t i);       // a click on the card's message
    void               unpin(size_t i);          // the card's "Unpin"

    void paint(gfx::Painter &p) override; // one grey surface; the cards are white

private:
    class Card;
    void load();
    void rebuild();
    void setStatus(std::string text);
    void onChange(const model::Change &ch);
    void later(std::function<void()> fn); // on the next loop turn, if the page still exists
    bool shows(model::Ts ts) const;

    screens::Context           &_ctx;
    Avatars                    &_avatars;
    ui::ScrollView             *_scroll = nullptr;
    ui::Label                  *_status = nullptr;
    ui::View                   *_list   = nullptr; // the cards
    std::vector<Card *>         _cards;
    std::vector<model::Message> _pins; // what the cards show, in order
    std::string                 _statusText;
    model::ConvRef              _conv    = model::kNoConv;
    int                         _request = 0; // the newest loadPins: older answers are dropped
    bool                        _loaded = false, _reloadQueued = false;
    model::Store::ObserverId    _observer = 0;
    std::shared_ptr<int>        _alive    = std::make_shared<int>(0);
};

} // namespace shell

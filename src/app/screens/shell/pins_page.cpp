#include "screens/shell/pins_page.h"

#include "base/i18n.h"
#ifdef MSGA_HAVE_MESSAGES
#include "screens/messages/message_list.h"
#endif

#include <algorithm>

using namespace ui;
using i18n::arg;
using i18n::tr;
using model::ConvRef;
using model::Ts;

namespace shell {

PinsPage::PinsPage(screens::Context &ctx) : _ctx(ctx) {
    style().column().flex(1);
    setBackground(C::Surface);
    _status = add<Label>("", Font::Body, C::TextMuted);
    _status->setAlign(text::LayoutOptions::Align::Center);
    _status->style().padding(48, 48, 48, 24);
    _status->setVisible(false);
#ifdef MSGA_HAVE_MESSAGES
    _messages = add<screens::MessageList>(_ctx);
    _messages->style().flex(1);
    _messages->setTypingRow(false);
    _messages->setVisible(false);
    _messages->onGoTo = [this](Ts ts, Ts thread) {
        if (onOpenMessage)
            onOpenMessage(_conv, ts, thread);
    };
    _messages->onUnpinned = [this](Ts ts) { dropped(ts); };
#endif
    _observer = ctx.store.observe(model::Store::kAnyConv, [this](const model::Change &ch) {
        onChange(ch);
    });
}

PinsPage::~PinsPage() {
    _ctx.store.unobserve(_observer);
}

bool PinsPage::shows(Ts ts) const {
    return std::any_of(_pins.begin(), _pins.end(), [ts](const model::Message &m) {
        return m.ts == ts;
    });
}

void PinsPage::later(std::function<void()> fn) {
    std::weak_ptr<int> alive = _alive;
    _ctx.app.platform().post([alive, fn = std::move(fn)] {
        if (!alive.expired())
            fn();
    });
}

// A pin or an unpin in this conversation while the tab is up (from a row's
// menu here, in the thread panel beside it, or by someone else): the list
// again. Any other change of a message (a reaction, an edit) is the rows' own
// business. Asked for on the next loop turn, once per burst.
void PinsPage::onChange(const model::Change &ch) {
    if (!visible() || _conv == model::kNoConv || _reloadQueued)
        return;
    bool mine = false;
    if (ch.kind == model::ChangeKind::Reset)
        mine = ch.conv == _conv || ch.conv == model::kNoConv;
    else if (ch.conv != _conv)
        return;
    else if (ch.kind == model::ChangeKind::Remove)
        mine = shows(ch.ts);
    else if (ch.kind == model::ChangeKind::Update || ch.kind == model::ChangeKind::Insert) {
        const model::Message *m = _ctx.store().findMessage(_conv, ch.ts);
        mine                    = m && !m->pending && m->pinned != shows(ch.ts);
    }
    if (!mine)
        return;
    _reloadQueued = true;
    later([this] {
        _reloadQueued = false;
        if (visible())
            load();
    });
}

void PinsPage::open(ConvRef conv) {
    if (conv != _conv)
        clear();
    _conv = conv;
    load();
}

void PinsPage::clear() {
    ++_request; // an answer on its way is for nothing now
    _pins.clear();
    _conv   = model::kNoConv;
    _loaded = false;
#ifdef MSGA_HAVE_MESSAGES
    _messages->clear();
    _messages->setVisible(false);
#endif
    setStatus({});
}

void PinsPage::load() {
    if (_conv == model::kNoConv || _conv >= _ctx.store().conversationCount())
        return;
    if (!_loaded)
        setStatus(tr("Loading pinned messages\xE2\x80\xA6"));
    const int          request = ++_request;
    std::weak_ptr<int> alive   = _alive;
    _ctx.backend.loadPins(
        _conv, [this, alive, request](std::vector<model::Message> pins, std::string err) {
            if (alive.expired() || request != _request)
                return;
            _loaded = true;
            _pins   = std::move(pins);
            model::sortByTs(_pins);
            show();
            if (_pins.empty() && !err.empty())
                setStatus(arg(tr("Couldn't load the pinned messages (%1)."), err));
        }
    );
}

void PinsPage::show() {
#ifdef MSGA_HAVE_MESSAGES
    _messages->setVisible(!_pins.empty());
    if (_pins.empty()) {
        _messages->clear();
    } else {
        std::vector<model::Message> copies;
        copies.reserve(_pins.size());
        for (const model::Message &m : _pins)
            copies.push_back(m.clone());
        _messages->showPinned(_conv, std::move(copies));
    }
#endif
    setStatus(
        _pins.empty() ? std::string(
                            tr("No pinned messages here yet. Pin a message from its menu to keep "
                               "it at hand for everyone in the conversation.")
                        )
                      : std::string()
    );
}

void PinsPage::setStatus(std::string text) {
    _statusText = std::move(text);
    _status->setText(_statusText);
    _status->setVisible(!_statusText.empty());
    invalidateLayout();
}

// Off the list without waiting for the service: a message the loaded
// history doesn't hold changes nothing in the Store, so nothing else would
// take it away. On the next loop turn: this may run inside the list's menu.
void PinsPage::dropped(Ts ts) {
    later([this, ts] {
        if (std::erase_if(_pins, [ts](const model::Message &m) { return m.ts == ts; }))
            show();
    });
}

std::string PinsPage::author(size_t i) const {
    if (i >= _pins.size())
        return {};
    const model::Message &m = _pins[i];
    if (m.extra && !m.extra->botName.empty())
        return m.extra->botName;
    return m.user != model::kNoUser ? std::string(_ctx.store().user(m.user).label())
                                    : std::string();
}

void PinsPage::goTo(size_t i) {
    if (i < _pins.size() && onOpenMessage)
        onOpenMessage(_conv, _pins[i].ts, _pins[i].isReply() ? _pins[i].threadTs : 0);
}

void PinsPage::unpin(size_t i) {
    if (i >= _pins.size())
        return;
    const Ts ts = _pins[i].ts;
    _ctx.backend.setPinned(_conv, ts, false);
    dropped(ts);
}

} // namespace shell

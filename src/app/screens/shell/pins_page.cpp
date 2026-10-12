#include "screens/shell/pins_page.h"

#include "base/i18n.h"
#include "base/time.h"
#include "gfx/icons_generated.h"
#include "screens/shell/message_search.h"
#include "screens/shell/overview_page.h"

#include <algorithm>

using namespace ui;
using i18n::arg;
using i18n::tr;
using model::ConvRef;
using model::Ts;

namespace shell {

namespace {

std::string pinnedByLabel(const model::Store &st, const model::Message &m) {
    return m.pinnedBy != model::kNoUser ? arg(tr("Pinned by %1"), st.user(m.pinnedBy).label())
                                        : std::string(tr("Pinned"));
}

std::string authorLabel(const model::Store &st, const model::Message &m) {
    if (m.extra && !m.extra->botName.empty())
        return m.extra->botName;
    return m.user != model::kNoUser ? std::string(st.user(m.user).label())
                                    : std::string(tr("Message"));
}

} // namespace

// One pinned message: the message row in the bordered body, the footer.
class PinsPage::Card final : public View {
public:
    Card(PinsPage &page, const model::Message &m)
        : _page(page), _ts(m.ts), _thread(m.isReply() ? m.threadTs : 0) {
        screens::Context   &ctx  = page._ctx;
        const model::Store &st   = ctx.store;
        auto               *body = cardBody(this);
        // The message, chat-style; a click anywhere jumps to it.
        auto               *row  = body->add<Clickable>();
        row->setLook({C::None, C::None, C::None, C::None, 0});
        row->setCursor(plat::Cursor::Hand);
        row->onClick = [this] { open(); };
        View *col    = fillMessageRow(
            row,
            page._avatars,
            authorLabel(st, m),
            m.extra && !m.extra->botAvatar.empty() ? m.extra->botAvatar : st.user(m.user).avatar,
            base::dateTimeLabel(model::tsSecs(m.ts), ctx.backend.nowSecs())
        );
        const std::string text = searchPreview(st, m.text);
        auto             *msg  = col->add<Label>(
            !text.empty()        ? text
            : !m.files().empty() ? m.files().front().name
                                 : std::string(tr("No preview available")),
            Font::Body
        );
        msg->setMaxLines(3);
        msg->setHitTransparent(true);

        auto *footer = body->add<View>();
        footer->style().row().spacing(8).items(Align::Center);
        footer->add<IconView>(gfx::Icon::Pin, 13, C::TextMuted);
        footer->add<Label>(pinnedByLabel(st, m), Font::Caption, C::TextMuted)->style().flex(1);
        auto *unpinBtn    = footer->add<TextLink>(tr("Unpin"), Font::Control, C::Link, false);
        unpinBtn->onClick = [this] { unpin(); };
    }

    void open() {
        if (_page.onOpenMessage)
            _page.onOpenMessage(_page._conv, _ts, _thread);
    }
    // The card goes without waiting for the list: a message the loaded
    // history doesn't hold changes nothing in the Store, so nothing else
    // would take it away. On the next loop turn: this runs inside the card's
    // own click.
    void unpin() {
        PinsPage &page = _page;
        const Ts  ts   = _ts;
        page._ctx.backend.setPinned(page._conv, ts, false);
        page.later([&page, ts] {
            std::erase_if(page._pins, [ts](const model::Message &m) { return m.ts == ts; });
            page.rebuild();
        });
    }

private:
    PinsPage &_page;
    Ts        _ts, _thread;
};

PinsPage::PinsPage(screens::Context &ctx, Avatars &avatars) : _ctx(ctx), _avatars(avatars) {
    style().column().flex(1);
    _scroll = add<ScrollView>();
    _scroll->style().flex(1);
    View *content = _scroll->content();
    content->style().padding(24).spacing(24);
    _status = content->add<Label>("", Font::Body, C::TextMuted);
    _status->setAlign(text::LayoutOptions::Align::Center);
    _status->style().padding(24);
    _status->setVisible(false);
    _list = content->add<View>();
    _list->style().spacing(16);
    _observer = ctx.store.observe(model::Store::kAnyConv, [this](const model::Change &ch) {
        onChange(ch);
    });
}

PinsPage::~PinsPage() {
    _ctx.store.unobserve(_observer);
}

void PinsPage::paint(gfx::Painter &p) {
    p.fillRect(bounds(), color(C::FormSunken));
    View::paint(p);
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

// A pin or an unpin in this conversation while the tab is up (here, from the
// message menu in a thread beside it, or by someone else): the list again.
// Any other change of a message (a reaction, an edit) leaves it alone. Asked
// for on the next loop turn, once per burst: the change may come from inside
// a card's own click.
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
    _list->clearChildren();
    _cards.clear();
    _pins.clear();
    _conv   = model::kNoConv;
    _loaded = false;
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
            if (pins.empty() && !err.empty()) {
                _pins.clear();
                rebuild();
                setStatus(arg(tr("Couldn't load the pinned messages (%1)."), err));
                return;
            }
            _pins = std::move(pins);
            rebuild();
        }
    );
}

void PinsPage::rebuild() {
    _list->clearChildren();
    _cards.clear();
    for (const model::Message &m : _pins)
        _cards.push_back(_list->add<Card>(*this, m));
    setStatus(
        _cards.empty() ? std::string(
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
}

std::string PinsPage::author(size_t i) const {
    return i < _pins.size() ? authorLabel(_ctx.store(), _pins[i]) : std::string();
}

std::string PinsPage::pinnedBy(size_t i) const {
    return i < _pins.size() ? pinnedByLabel(_ctx.store(), _pins[i]) : std::string();
}

void PinsPage::activate(size_t i) {
    if (i < _cards.size())
        _cards[i]->open();
}

void PinsPage::unpin(size_t i) {
    if (i < _cards.size())
        _cards[i]->unpin();
}

} // namespace shell

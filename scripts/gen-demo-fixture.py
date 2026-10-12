#!/usr/bin/env python3
"""The demo workspace `make run-demo` shows: the tests' fixture plus a busy channel.

    scripts/gen-demo-fixture.py <out dir>

Writes <out dir>/fixture.json: tests/assets/fixture.json with a channel
#updates added — about a hundred messages from the fixture's people over the
last ten days, about a quarter of them pinned — for trying long histories and
the Pins tab. The fixture's media (avatars, images, files…) is linked into
<out dir>, so the paths in the file resolve as they do in tests/assets.

The tests' fixture itself is not touched: the suites count its conversations
and messages. The same seed gives the same channel on every run.
"""
import json
import os
import random
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, 'tests', 'assets')

CHANNEL = 'C0UPDATES'
MESSAGES = 100
PINNED = 25
DAYS = 10  # the channel's history: days -10 … -1, so nothing lies in the future
SEED = 2026

SUBJECTS = [
    'the onboarding flow', 'the billing page', 'the Atlas 2.0 changelog', 'the mobile build',
    'the search index', 'the export job', 'the status page', 'the pricing experiment',
    'the support macros', 'the design tokens', 'the staging database', 'the weekly report',
    'the signup funnel', 'the notification service', 'the release checklist', 'the docs site',
]
UPDATES = [
    'Update on {s}: shipped to staging this morning, production after lunch.',
    'Quick status on {s}: about 80% done, the last part is waiting on review.',
    '{s} is live. Numbers look normal so far, I will keep an eye on it today.',
    'Heads up: {s} will be read-only for about ten minutes at 15:00.',
    'Finished the first pass on {s}. Notes are in the doc, comments welcome.',
    'Blocked on {s}: I need a decision on the rollout order before I continue.',
    'Rolled back the change to {s}. The fix is ready, I will retry tomorrow.',
    'Weekly numbers for {s} are in: up 6% on last week, no regressions.',
    'Reminder: the review of {s} is on Thursday. Add your points to the agenda.',
    'I moved the deadline for {s} to Friday so QA gets a full day with it.',
    'Small win: {s} loads in under a second now, down from almost three.',
    'Handing {s} over to whoever is on call next week. Runbook is updated.',
]
REPLIES = [
    'Thanks for the update, that unblocks me.',
    'Nice, great work on this.',
    'Can you share the link to the dashboard?',
    'I can review it this afternoon.',
    'Sounds good. Ping me if the numbers move.',
    'Do we need to tell support before it goes out?',
    'Tested on my side, works as expected.',
    'I saw one odd result on mobile, sending a screenshot in a minute.',
    'Agreed, Friday is safer.',
    'Who owns the follow-up?',
    'I will take the follow-up.',
    'Added it to the release notes.',
    'Is this behind the feature flag still?',
    'Yes, the flag is at 25% since yesterday.',
    'Good call rolling it back.',
    'Let me know if you want a second pair of eyes.',
    'Noted, I will adjust the campaign dates.',
    'That matches what we see in the support queue.',
    ':tada: finally!',
    'Same here, no issues.',
]
REACTIONS = ['+1', 'tada', 'eyes', 'rocket', 'white_check_mark', 'raised_hands']


def channel_messages(people):
    rng = random.Random(SEED)
    # Ten working days, 09:00–17:59, oldest first: minutes of the day, sorted.
    per_day = [MESSAGES // DAYS] * DAYS
    for i in range(MESSAGES % DAYS):
        per_day[i] += 1
    pinned = set(rng.sample(range(MESSAGES), PINNED))
    out = []
    last = None
    for day, count in zip(range(-DAYS, 0), per_day):
        minutes = sorted(rng.sample(range(9 * 60, 18 * 60), count))
        for i, minute in enumerate(minutes):
            # A day opens with an update; after that mostly replies, each from
            # someone other than the previous speaker.
            user = rng.choice([p for p in people if p != last])
            last = user
            if i == 0 or rng.random() < 0.3:
                text = rng.choice(UPDATES).format(s=rng.choice(SUBJECTS))
                text = text[0].upper() + text[1:]
            else:
                text = rng.choice(REPLIES)
            msg = {'conv': CHANNEL, 'user': user,
                   'time': '%dd %02d:%02d' % (day, minute // 60, minute % 60), 'text': text}
            if len(out) in pinned:
                msg['pinned'] = True
            if rng.random() < 0.2:
                others = [p for p in people if p != user]
                msg['reactions'] = [{'name': rng.choice(REACTIONS),
                                     'users': rng.sample(others, rng.randint(1, 3))}]
            out.append(msg)
    return out


def link_media(out_dir):
    for name in os.listdir(ASSETS):
        if name == 'fixture.json':
            continue
        link = os.path.join(out_dir, name)
        if os.path.lexists(link):
            os.remove(link)
        os.symlink(os.path.join(ASSETS, name), link)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out_dir = sys.argv[1]
    with open(os.path.join(ASSETS, 'fixture.json'), encoding='utf-8') as f:
        fixture = json.load(f)
    people = [u['id'] for u in fixture['users'] if not u.get('bot')]
    fixture['conversations'].append({
        'id': CHANNEL, 'kind': 'channel', 'name': 'updates',
        'topic': 'Status updates from every team', 'memberCount': len(people)})
    messages = channel_messages(people)
    fixture['messages'].extend(messages)
    os.makedirs(out_dir, exist_ok=True)
    link_media(out_dir)
    with open(os.path.join(out_dir, 'fixture.json'), 'w', encoding='utf-8') as f:
        json.dump(fixture, f, ensure_ascii=False, indent=1)
        f.write('\n')
    print('%s: #updates with %d messages, %d pinned' % (
        os.path.join(out_dir, 'fixture.json'), len(messages),
        sum(1 for m in messages if m.get('pinned'))))


if __name__ == '__main__':
    main()

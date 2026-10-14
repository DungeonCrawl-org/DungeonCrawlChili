const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

class Element {
    constructor(tag = 'div') { this.tag = tag; this.handlers = {}; this.nodes = []; this[0] = this; }
    clone() { return this; }
    find() { return new Element(); }
    children() { return new Element(); }
    html() { return this; }
    attr() { return this; }
    text() { return this; }
    append(node) { this.nodes.push(node); return this; }
    on(names, fn) { for (const name of names.split(' ')) this.handlers[name] = fn; return this; }
    off() { this.handlers = {}; return this; }
    is(tag) { return this.tag === tag; }
}
let gameOver, watching = false;
const messages = [];
const document = {};
const $ = value => value instanceof Element ? value : new Element(value === '<button>' ? 'button' : 'div');
const source = fs.readFileSync(__dirname + '/../game_data/static/ui-layouts.js', 'utf8')
    .replace('register_handlers: register_ui_handlers,', 'register_handlers: register_ui_handlers, game_over: game_over,');
vm.runInNewContext(source, {
    document,
    define(_, factory) {
        gameOver = factory($, {
            register_handlers() {}, send_message(type, data) { messages.push({type, data}); }
        }, { is_watching: () => watching }, {}, {}, {
            DungeonCellRenderer: class { init() {} draw_from_texture() {} }
        }, { formatted_string_to_html: value => value, init_canvas() {} }, () => ({})).game_over;
    }
});
const popup = gameOver({title: 'Goodbye', body: 'Final score', postmortem_available: true, tile: {}});
const button = popup.nodes[0];
for (const target of [popup, button]) {
    for (const key of ['p', 'P']) {
        const before = messages.length;
        for (const type of ['keydown', 'keypress']) {
            let prevented = false, stopped = false;
            popup.handlers[type]({type, key, target,
                preventDefault() { prevented = true; },
                stopImmediatePropagation() { stopped = true; }});
            assert(prevented && stopped, 'P must not dismiss or scroll the game-over popup');
        }
        assert.equal(messages.length, before + 1, 'Send one analysis request for a keydown/keypress pair');
        assert.equal(messages.at(-1).data.keycode, 112);
    }
}
for (const key of ['Tab', 'Enter', ' ']) {
    let prevented = false, stopped = false;
    popup.handlers.keydown({type: 'keydown', key, target: button,
        preventDefault() { prevented = true; }, stopImmediatePropagation() { stopped = true; }});
    assert(stopped && !prevented, 'Preserve native button activation and focus navigation');
}
button.handlers.click({stopPropagation() {}});
assert.equal(messages.at(-1).data.keycode, 112);
const replayPopup = gameOver({title: 'Goodbye', body: 'Final score',
    postmortem_available: true, death_replay_available: true, tile: {}});
assert.equal(replayPopup.nodes.length, 2);
const replayButton = replayPopup.nodes[0];
for (const key of ['r', 'R']) {
    const before = messages.length;
    for (const type of ['keydown', 'keypress']) {
        replayPopup.handlers[type]({type, key, target: replayButton,
            preventDefault() {}, stopImmediatePropagation() {}});
    }
    assert.equal(messages.length, before + 1);
    assert.equal(messages.at(-1).data.keycode, 114);
}
replayButton.handlers.click({stopPropagation() {}});
assert.equal(messages.at(-1).data.keycode, 114);
const unavailablePopup = gameOver({title: 'Goodbye', body: 'Quit game',
    postmortem_available: false, death_replay_available: false, tile: {}});
assert.equal(unavailablePopup.nodes.length, 0,
    'Quit games and Death Replay sessions have no post-mortem controls');
watching = true;
const spectator = gameOver({title: 'Goodbye', body: 'Final score',
    postmortem_available: true, death_replay_available: true, tile: {}});
assert.equal(spectator.nodes.length, 0, 'Spectators do not get an analysis control');
console.log('Game-over checks passed: P/p with button focus, one request, native activation and spectator controls.');

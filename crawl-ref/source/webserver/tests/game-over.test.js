const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const elementsById = {};

class Element {
    constructor(tag = 'div') { this.tag = tag; this.handlers = {}; this.nodes = []; this.dataValues = {}; this[0] = this; }
    clone() { return this; }
    find() { return new Element(); }
    children() { return new Element(); }
    html() { return this; }
    attr(key, value) {
        const values = typeof key === 'object' ? key : {[key]: value};
        if (values.id) { this.id = values.id; elementsById[this.id] = this; }
        return this;
    }
    text() { return this; }
    addClass() { return this; }
    css() { return this; }
    data(key, value) {
        if (arguments.length > 1) { this.dataValues[key] = value; return this; }
        return this.dataValues[key];
    }
    empty() { this.nodes = []; return this; }
    append(node) { this.nodes.push(node); return this; }
    appendTo(parent) { parent.append(this); return this; }
    prop() { return this; }
    first() { return this.nodes[0] || new Element(); }
    trigger() { return this; }
    remove() { this.removed = true; if (this.id) delete elementsById[this.id]; return this; }
    contains() { return false; }
    on(names, fn) { for (const name of names.split(' ')) this.handlers[name] = fn; return this; }
    off() { this.handlers = {}; return this; }
    is(tag) { return this.tag === tag; }
}
let gameOver, watching = false, hostname = 'dcf.dungeoncrawlforks.org';
const messages = [];
const commHandlers = {};
const document = {
    body: new Element('body'), listeners: {}, contains: node => !node.removed,
    addEventListener(type, fn) { this.listeners[type] = fn; },
    removeEventListener(type, fn) {
        if (this.listeners[type] === fn) delete this.listeners[type];
    }
};
const $ = value => {
    if (value instanceof Element) return value;
    if (typeof value === 'string' && value.startsWith('#')) {
        return elementsById[value.slice(1)] || {length: 0};
    }
    return new Element(value === '<button>' ? 'button' : 'div');
};
const source = fs.readFileSync(__dirname + '/../game_data/static/ui-layouts.js', 'utf8')
    .replace('register_handlers: register_ui_handlers,', 'register_handlers: register_ui_handlers, game_over: game_over,');
vm.runInNewContext(source, {
    document, setTimeout, clearTimeout,
    window: {location: {get hostname() { return hostname; }}},
    define(_, factory) {
        gameOver = factory($, {
            register_handlers(handlers) { Object.assign(commHandlers, handlers); },
            send_message(type, data) { messages.push({type, data}); }
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
const recommendationOverlay = document.body.nodes.at(-1);
const recommendation = recommendationOverlay.nodes[0];
assert.equal(recommendation.nodes.length, 5);
let stopped = false, prevented = false;
document.listeners.mousedown({target: {}, preventDefault() { prevented = true; },
    stopImmediatePropagation() { stopped = true; }});
assert(prevented && stopped, 'Clicks outside the modal cannot dismiss the death dialog');
recommendation.nodes[2].handlers.click({preventDefault() {}, stopPropagation() {}});
assert.equal(messages.at(-1).type, 'record_death_replay_recommendation');
assert.equal(messages.at(-1).data.vote, 1);
assert.equal(recommendationOverlay.removed, undefined, 'Wait for the vote acknowledgement');
commHandlers.death_replay_recommendation_result({ok: true});
assert.equal(recommendationOverlay.removed, true, 'A saved vote closes the modal');
assert.equal(document.listeners.mousedown, undefined, 'Vote removes the outside-click guard');
gameOver({title: 'Goodbye', body: 'Final score', death_replay_available: true, tile: {}});
const ignoredOverlay = document.body.nodes.at(-1);
const messagesBeforeIgnore = messages.length;
document.listeners.keydown({type: 'keydown', key: 'Escape', preventDefault() {}, stopImmediatePropagation() {}});
assert.equal(ignoredOverlay.removed, true, 'Escape ignores the recommendation');
assert.equal(messages.length, messagesBeforeIgnore, 'Ignoring does not submit a vote');
hostname = 'crawl.xtahua.com';
const cxcPopup = gameOver({title: 'Goodbye', body: 'Final score',
    death_replay_available: true, tile: {}});
assert.equal(cxcPopup.nodes.length, 0, 'CXC has no Death Replay button or feedback');
hostname = 'crawl.project357.org';
const cpoPopup = gameOver({title: 'Goodbye', body: 'Final score',
    death_replay_available: true, tile: {}});
assert.equal(cpoPopup.nodes.length, 0, 'CPO has no Death Replay button or feedback');
const unavailablePopup = gameOver({title: 'Goodbye', body: 'Quit game',
    postmortem_available: false, death_replay_available: false, tile: {}});
assert.equal(unavailablePopup.nodes.length, 0,
    'Quit games and Death Replay sessions have no post-mortem controls');
watching = true;
hostname = 'dcf.dungeoncrawlforks.org';
const spectator = gameOver({title: 'Goodbye', body: 'Final score',
    postmortem_available: true, death_replay_available: true, tile: {}});
assert.equal(spectator.nodes.length, 0, 'Spectators do not get an analysis control');
console.log('Game-over checks passed: P/p with button focus, one request, native activation and spectator controls.');

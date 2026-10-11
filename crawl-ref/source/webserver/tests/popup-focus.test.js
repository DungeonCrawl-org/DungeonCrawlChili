// Exercise the capture handler before popup/bubbling keyboard handlers run.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const messages = [];
let handler, tag, watching = false;
const focused = { blur() { this.blurred = true; } };
const document = { activeElement: focused, body: {} };
const empty = { length: 0 };
const popup = { attr() { return tag; } };
const wrapper = { find() { return { children() { return { eq() { return popup; } }; } }; } };
const stack = { length: 1, last() { return wrapper; } };
const events = { off() { return this; }, on() { return this; } };
const $ = value => {
    if (value === document) return events;
    if (value === '#ui-stack') return { children() { return stack; } };
    if (value === focused) return { closest(selector) { return selector === '#ui-stack' ? stack : empty; } };
    throw new Error('Unexpected selector: ' + value);
};
// Expose only this private callback in the test VM; production exports stay unchanged.
const source = fs.readFileSync(__dirname + '/../game_data/static/ui.js', 'utf8')
    .replace('function popup_keydown_handler(ev)', 'captureHandler = popup_keydown_handler; function popup_keydown_handler(ev)');
vm.runInNewContext(source, {
    document, MutationObserver: class { observe() {} },
    define(_, factory) {
        factory($, { register_handlers() {}, send_message(type, data) { messages.push({type, data}); } },
            { is_watching() { return watching; } }, { add_listener() {} }, {});
    },
    set captureHandler(value) { handler = value; }
});
for (tag of ['postmortem_help', 'coaching_help']) {
    let prevented = false, stopped = false;
    handler({ key: 'Escape', preventDefault() { prevented = true; },
        stopImmediatePropagation() { stopped = true; } });
    assert(prevented && stopped);
    assert.equal(focused.blurred, undefined, 'Do not blur and let the focus trap refocus the file input');
    assert.equal(messages.at(-1).data.keycode, 27);
}
assert.equal(messages.length, 2, 'Exactly one close per Escape');
watching = true;
handler({ key: 'Escape', preventDefault() {}, stopImmediatePropagation() {} });
assert.equal(messages.length, 2, 'Spectators cannot send game keys');
console.log('Popup focus checks passed: Escape closes coaching dialogs from focused native controls.');

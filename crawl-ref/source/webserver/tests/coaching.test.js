// Run with node webserver/tests/coaching.test.js from source/.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

class Element {
    constructor(tag = "div") {
        this.tag = tag;
        this.handlers = {};
        this.nodes = [];
        this.attrs = {};
        this[0] = this;
    }
    on(names, callback) {
        for (const name of names.split(" ")) this.handlers[name.split(".")[0]] = callback;
        return this;
    }
    off() { this.handlers = {}; return this; }
    prop(name, value) { this.attrs[name] = value; return this; }
    attr(values, value) {
        Object.assign(this.attrs, typeof values === "string" ? { [values]: value } : values);
        return this;
    }
    text(value) { this.label = value; return this; }
    val(value) { this.value = value; return this; }
    css(values) { Object.assign(this.attrs, values); return this; }
    appendTo(parent) { parent.nodes.push(this); return this; }
    children() { return this.more || (this.more = new Element()); }
    empty() { this.nodes = []; return this; }
    show() { this.visible = true; return this; }
    hide() { this.visible = false; return this; }
    select() { this.selected = true; }
    blur() {}
    is(selector) { return this.tag === selector; }
    trigger(type, event = {}) {
        event = Object.assign({ type, target: this, preventDefault() {},
            stopPropagation() {}, stopImmediatePropagation() {} }, event);
        return this.handlers[type]?.call(this, event);
    }
}

function fixture({ watching = false, clipboardFails = false } = {}) {
    const document = new Element("document");
    document.execCommand = () => false;
    const launch = new Element("button");
    const messages = [], copied = [], opened = [], handlers = {};
    const $ = value => value instanceof Element ? value : value === "#coaching-help" ? launch :
        new Element(value.replace(/[<>]/g, ""));
    let module;
    vm.runInNewContext(fs.readFileSync(path.join(__dirname, "../game_data/static/coaching.js"), "utf8"), {
        document, navigator: { clipboard: { writeText(text) {
            copied.push(text);
            return clipboardFails ? Promise.reject(new Error("Denied")) : Promise.resolve();
        } } },
        window: { open(...args) { opened.push(args); } },
        define(dependencies, factory) {
            module = factory($, { register_handlers(map) { Object.assign(handlers, map); },
                send_message(type, data) { messages.push({ type, data }); } },
                { is_watching: () => watching });
        }
    });
    return { document, launch, messages, copied, opened, handlers, module };
}

async function main() {
    const player = fixture();
    player.document.trigger("game_init");
    player.launch.trigger("click");
    assert.equal(player.messages[0].data.keycode, -500);
    const dump = "Coach this live dump: HP 12/55\nNotes: <script>not markup</script>";
    player.handlers.coaching_context({ prompt: dump });
    const popup = new Element();
    player.module.install_controls(popup);
    popup.more.nodes.find(node => node.label === "Copy and open ChatGPT [B]").trigger("click");
    await Promise.resolve();
    assert.equal(player.copied[0], dump);
    assert.equal(player.opened[0][0], "https://chatgpt.com/");
    assert.equal(player.opened[0][2], "noopener,noreferrer");
    assert.equal(popup.more.nodes.find(node => node.tag === "textarea").value, dump);
    popup.trigger("keydown", { key: "c" });
    await Promise.resolve();
    assert.equal(player.copied.length, 2);
    assert.equal(player.opened.length, 1);
    player.document.trigger("game_cleanup");
    const next = new Element();
    player.module.install_controls(next);
    assert.equal(next.more, undefined, "Never reuse another game's dump");

    const blocked = fixture({ clipboardFails: true });
    blocked.handlers.coaching_context({ prompt: dump });
    const manualPopup = new Element();
    blocked.module.install_controls(manualPopup);
    manualPopup.trigger("keydown", { key: "c" });
    await Promise.resolve();
    const textarea = manualPopup.more.nodes.find(node => node.tag === "textarea");
    assert.equal(textarea.visible, true);
    assert.equal(textarea.selected, true);
    assert.equal(textarea.attrs.readonly, true);

    const watcher = fixture({ watching: true });
    watcher.document.trigger("game_init");
    watcher.launch.trigger("click");
    assert.equal(watcher.launch.attrs.disabled, true);
    assert.equal(watcher.messages.length, 0);
    watcher.handlers.coaching_context({ prompt: dump });
    const watcherPopup = new Element();
    watcher.module.install_controls(watcherPopup);
    assert.equal(watcherPopup.more, undefined);
    console.log("Webtiles coaching checks passed: launch, exact copy, browser open, keyboard, fallback, privacy.");
}
main().catch(error => { console.error(error); process.exitCode = 1; });

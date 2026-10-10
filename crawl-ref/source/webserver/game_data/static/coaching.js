define(["jquery", "comm", "client"], function ($, comm, client) {
    "use strict";

    var prompt = "";
    comm.register_handlers({
        coaching_context: function (message) {
            prompt = typeof message.prompt === "string" ? message.prompt : "";
        }
    });

    $(document).on("game_preinit.coaching game_cleanup.coaching", function () {
        prompt = "";
    });
    $(document).on("game_init.coaching", function () {
        $("#coaching-help").prop("disabled", client.is_watching())
            .off("click.coaching").on("click.coaching", function (event) {
                event.preventDefault();
                if (!client.is_watching())
                    comm.send_message("key", { keycode: -500 });
                this.blur();
            });
    });

    function install_controls(popup)
    {
        if (client.is_watching() || !prompt)
            return;
        var controls = popup.children(".more").empty();
        var status = $("<div>").attr("role", "status").appendTo(controls);
        var manual = $("<textarea>").attr({
            readonly: true, rows: 5, "aria-label": "Coaching prompt and live morgue dump"
        }).css({ width: "95%", display: "none" }).val(prompt).appendTo(controls);

        function fallback_copy()
        {
            manual.show()[0].select();
            try {
                if (document.execCommand("copy")) {
                    status.text("Copied. Paste into ChatGPT to ask for advice.");
                    manual.hide();
                    return;
                }
            } catch (ignored) {}
            status.text("Select the text below and copy it manually.");
        }

        function copy(open_browser)
        {
            // Start copying while the game tab still has focus. Open the new
            // tab synchronously in this same gesture to avoid popup blockers.
            if (navigator.clipboard && navigator.clipboard.writeText) {
                navigator.clipboard.writeText(prompt).then(function () {
                    status.text("Copied. Paste into ChatGPT to ask for advice.");
                }, fallback_copy);
            } else {
                fallback_copy();
            }
            if (open_browser)
                window.open("https://chatgpt.com/", "_blank", "noopener,noreferrer");
        }

        $("<button>").attr("type", "button").text("Copy dump [C]")
            .on("click", function (event) { event.stopPropagation(); copy(false); })
            .appendTo(controls);
        $("<button>").attr("type", "button").text("Copy and open ChatGPT [B]")
            .on("click", function (event) { event.stopPropagation(); copy(true); })
            .appendTo(controls);
        $("<button>").attr("type", "button").text("Return [Esc]")
            .on("click", function (event) {
                event.stopPropagation();
                comm.send_message("key", { keycode: 27 });
            }).appendTo(controls);
        popup.on("keydown.coaching keypress.coaching", function (event) {
            if ($(event.target).is("textarea"))
                return;
            var key = (event.key || String.fromCharCode(event.which)).toLowerCase();
            if (!event.ctrlKey && !event.altKey && !event.metaKey && (key === "b" || key === "c")) {
                event.preventDefault();
                event.stopImmediatePropagation();
                if (event.type === "keydown")
                    copy(key === "b");
                return false;
            }
        });
    }

    return { install_controls: install_controls };
});

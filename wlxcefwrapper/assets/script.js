"use strict";
function wlxcef_change_url(url_uri) {
    get_input_search().value = decodeURIComponent(url_uri);
}
function get_input_search() {
    return document.getElementById("input_search");
}
window.onerror = function (e, url, line, col, err) {
    document.getElementById("body").innerHTML = "Window error: " + JSON.stringify(e) + " at " + url + " " + line + ":" + col + ", error: " + JSON.stringify(err);
};
window.addEventListener("load", function () {
    var search_bar_bg = document.getElementById("search_bar_bg");
    var but_nav_enter = document.getElementById("nav_enter");
    var input_search = document.getElementById("input_search");
    var nav_back = document.getElementById("nav_back");
    var nav_forward = document.getElementById("nav_forward");
    var nav_refresh = document.getElementById("nav_refresh");
    nav_back.addEventListener("click", function () {
        wlxcefpost_nav_back();
    });
    nav_forward.addEventListener("click", function () {
        wlxcefpost_nav_forward();
    });
    nav_refresh.addEventListener("click", function () {
        wlxcefpost_nav_refresh();
    });
    input_search.addEventListener("keydown", function (e) {
        if (e.key == "Enter") {
            wlxcefpost_url_change(input_search.value);
        }
    });
    but_nav_enter.addEventListener("click", function () {
        wlxcefpost_url_change(input_search.value);
    });
    input_search.addEventListener("focusin", function () {
        search_bar_bg.classList.add("search_bar_glimmer");
    });
    input_search.addEventListener("focusout", function () {
        search_bar_bg.classList.remove("search_bar_glimmer");
    });
});

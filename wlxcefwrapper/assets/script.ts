/// WLXCEF INTERFACE
function wlxcef_change_url(url_uri: string) {
  get_input_search().value = decodeURIComponent(url_uri);
}

declare function wlxcefpost_nav_back(): void;
declare function wlxcefpost_nav_forward(): void;
declare function wlxcefpost_nav_refresh(): void;
declare function wlxcefpost_url_change(str: string): void;

/// Navbar

function get_input_search() {
  return document.getElementById("input_search") as HTMLInputElement;
}

window.onerror = (e, url, line, col, err) => {
  document.getElementById("body")!.innerHTML = "Window error: " + JSON.stringify(e) + " at " + url + " " + line + ":" + col + ", error: " + JSON.stringify(err);
}

window.addEventListener("load", () => {
  const search_bar_bg = document.getElementById("search_bar_bg") as HTMLElement;
  const but_nav_enter = document.getElementById("nav_enter") as HTMLElement;
  const input_search = document.getElementById("input_search") as HTMLInputElement;
  const nav_back = document.getElementById("nav_back") as HTMLElement;
  const nav_forward = document.getElementById("nav_forward") as HTMLElement;
  const nav_refresh = document.getElementById("nav_refresh") as HTMLElement;

  nav_back.addEventListener("click", () => {
    wlxcefpost_nav_back();
  });

  nav_forward.addEventListener("click", () => {
    wlxcefpost_nav_forward();
  });

  nav_refresh.addEventListener("click", () => {
    wlxcefpost_nav_refresh();
  });

  input_search.addEventListener("keydown", (e) => {
    if (e.key == "Enter") {
      wlxcefpost_url_change(input_search.value);
    }
  });

  but_nav_enter.addEventListener("click", () => {
    wlxcefpost_url_change(input_search.value);
  });

  input_search.addEventListener("focusin", () => {
    search_bar_bg.classList.add("search_bar_glimmer");
  });

  input_search.addEventListener("focusout", () => {
    search_bar_bg.classList.remove("search_bar_glimmer");
  });
});
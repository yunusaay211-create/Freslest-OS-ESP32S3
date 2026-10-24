# Standalone Browser 6.12

The browser runs on the ESP32-S3 itself. No external PC/Pi, remote renderer,
cloud browser, BrowserServer program or server address/key is required.

Connect using Settings > WiFi, leave that page, then open Apps > Browser.
Wi-Fi remains connected. The home page keeps **Google, YouTube, ChatGPT**
in that order, followed by Open address, Wi-Fi settings and Browser capabilities.

Supported: direct HTTP/HTTPS, readable HTML text, clickable links, relative URL
resolution, basic named input fields and GET form submission, plain text and
JPEG images. CSS layouts and JavaScript are not executed. POST forms,
interactive login flows, MP4/H.264, YouTube video and the full ChatGPT web app
are unsupported. Those bookmarks may show available text or the website's
request to use a full browser. They are not claimed to be fully working apps.

## Controls

| Input | Action |
| --- | --- |
| UP / DOWN | Scroll one text row |
| LEFT / RIGHT | Select previous / next link or form control |
| OK | Open selected link or edit/submit selected form control |
| Mouse left click | Open a link/control on that text row |
| FIRE / Address toolbar button | Enter a complete http:// or https:// URL |
| Back History toolbar button | Go to the previous requested address |
| BACK / Escape | Browser home, then Apps |
| Hold OK | Task Switcher |

Text fields use the existing button/mouse/USB/BLE screen keyboard. Case switches
lower/upper/symbols; Done accepts, BACK cancels, keyboard Enter accepts.
The existing US ASCII keyboard layout is used. Common Turkish UTF-8 characters
in page text are transliterated to their ASCII equivalents for the small font.
Reload fetches the current URL again. Info lists the actual supported features.

JPEG files are limited to 96 KiB. The existing decoder selects 1/2/4/8 scale
to fit the screen without changing the AV signal generator. Larger dimensions,
unsupported JPEG encoding or truncated images produce an explicit message.
Image links open separately rather than recreating a full website layout.

## Network and limits

One background worker owns the HTTP/TLS connection; it never draws on the UI.
Two leased PSRAM page/image buffers prevent a downloaded page overwriting the
one being displayed. Exiting cancels publication of an in-flight request and
keeps Wi-Fi connected. No webpage JavaScript is executed.

Each request has a four-second socket timeout and a bounded body-read deadline.
At most five redirects, 256 rows of 72 characters, 64 links, six GET forms and
12 input fields are retained. HTML transfer is capped at 256 KiB. Oversized
pages are shortened explicitly. Bodies using compressed encodings are rejected;
the browser requests identity encoding. Cookies, downloads, tabs, JavaScript,
CSS layout, select/textarea controls and audio/video codecs are not implemented.

HTTPS verifies hostname/certificate trust using ESP-IDF 5.4.4's filtered common
Mozilla CA bundle, chosen to fit the unchanged A/B OS slot. Unlisted private or
less-common authorities and expired/invalid chains fail normally. Certificate
verification is not disabled. Wi-Fi requests NTP automatically; set Clock
manually if internet time is unavailable, then Reload. A valid clock is needed
for HTTPS, but local plain HTTP pages can be used without time synchronization.

Doom sources/firmware and its behavior are retained exactly from 6.10. AV,
buttons, Wi-Fi/BLE status, Files/USB/NES and OTA partition addresses are retained.

## Verification

The actual parser, HTTP worker and UI run in sanitizer-backed host tests with
fragmented HTML, entities, malformed data, form input/submission, redirect and
transfer limits, JPEG responses, cancellation and LCD/4:3 geometry. The OS is
cross-built with ESP-IDF 5.4.4 and its update image is validated by the production
update parser. The host HTTP tests use SDK doubles; they do not establish
physical ESP32 TLS/radio performance. Board/TV operation still needs testing.

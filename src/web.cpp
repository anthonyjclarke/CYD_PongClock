#include "web.h"
#include "display.h"
#include "clock.h"
#include "config.h"
#include "debug.h"
#include <WebServer.h>
#include <WiFi.h>

static WebServer server(WEB_SERVER_PORT);
static bool      webStarted = false;

// ── HTML page (stored in flash — const char[] on ESP32 lives in flash) ────────
//
// Layout: CSS CYD hardware mockup wrapping the live /screenshot.bmp image.
// The .screen div is 320:240 aspect ratio; the <img> is positioned to match
// the LED_X_OFFSET (16 px → 5%) and LED_Y_OFFSET (24 px → 10%) so the matrix
// sits exactly where it does on the real display.
// Auto-refresh every 2 s via JS src-swap (cache-busted with timestamp).
// Foundation for future WebUI — /api/info endpoint already wired in.
//
static const char HTML_PAGE[] =
R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PongClock</title>
<style>
*,*::before,*::after{box-sizing:border-box}
body{background:#111;display:flex;flex-direction:column;align-items:center;
  min-height:100vh;padding:32px 16px;font-family:'Courier New',monospace;
  color:#666;margin:0}
h1{font-size:11px;letter-spacing:4px;color:#c8a000;margin:0 0 28px;
  text-transform:uppercase}
/* PCB body */
.pcb{background:linear-gradient(150deg,#d4b000 0%,#b89400 50%,#c8a800 100%);
  border-radius:8px;padding:16px 20px 22px;
  box-shadow:0 8px 32px rgba(0,0,0,.8),inset 0 1px 0 rgba(255,255,255,.2),
  inset 0 -1px 0 rgba(0,0,0,.3);position:relative}
.pcb-id{font-size:7px;color:rgba(0,0,0,.35);letter-spacing:1.5px;
  margin-bottom:8px}
/* Bezel */
.bezel{background:#080808;border-radius:3px;padding:6px;
  box-shadow:inset 0 2px 8px rgba(0,0,0,.9),0 1px 0 rgba(255,255,255,.08)}
/* Screen — 320:240 aspect */
.screen{width:480px;height:360px;background:#050200;position:relative;
  overflow:hidden;border-radius:1px}
/* LED matrix image — offsets mirror LED_X_OFFSET/LED_Y_OFFSET */
.screen img{position:absolute;left:5%;top:10%;width:90%;height:80%;
  display:block;image-rendering:pixelated;image-rendering:crisp-edges}
/* Bottom PCB edge */
.pcb-btm{display:flex;align-items:center;justify-content:space-between;
  margin-top:10px;padding:0 2px}
.usb{width:32px;height:10px;background:#888;border-radius:2px;
  border:1px solid #666;box-shadow:inset 0 1px 2px rgba(0,0,0,.6)}
.chips{display:flex;gap:5px}
.chip{width:10px;height:10px;background:#2a2a2a;border:1px solid #444;
  border-radius:1px}
.btn{width:9px;height:9px;
  background:radial-gradient(circle at 35% 35%,#777,#444);
  border-radius:50%;border:1px solid #333}
/* Info bar */
.info{margin-top:22px;font-size:11px;text-align:center;line-height:2}
.info a{color:#c8a000;text-decoration:none}
.info a:hover{text-decoration:underline}
#ts{color:#888}
</style>
</head>
<body>
<h1>Pong Clock</h1>
<div class="pcb">
  <div class="pcb-id">ESP32-2432S028R &nbsp;&middot;&nbsp; ILI9341 320&times;240</div>
  <div class="bezel">
    <div class="screen">
      <img id="ss" src="/screenshot.bmp" alt="display">
    </div>
  </div>
  <div class="pcb-btm">
    <div class="usb"></div>
    <div class="chips">
      <div class="chip"></div><div class="chip"></div><div class="chip"></div>
    </div>
    <div class="btn"></div>
  </div>
</div>
<div class="info">
  refreshed <span id="ts">-</span>
  &nbsp;&middot;&nbsp;<a href="/api/info">api/info</a>
  &nbsp;&middot;&nbsp;<a href="/screenshot.bmp" download>download</a>
</div>
<script>
function refresh(){
  var img=document.getElementById('ss');
  var now=new Date();
  img.onload=function(){document.getElementById('ts').textContent=now.toLocaleTimeString();};
  img.src='/screenshot.bmp?'+now.getTime();
}
setInterval(refresh,2000);
</script>
</body>
</html>
)rawhtml";

// ── /screenshot.bmp ───────────────────────────────────────────────────────────
// Streams the matrixSprite buffer (288×192, 8-bit RGB332) as a 24-bit BMP.
// Converts RGB332 → BGR888 row-by-row using a 864-byte stack buffer —
// no full-file heap allocation needed.
//
// BMP row size: 288 × 3 = 864 bytes — already 4-byte aligned, no padding.
// File size: 54-byte header + 864 × 192 = 165,942 bytes.
//
static void handleScreenshot() {
  const int W        = LED_WIDTH  * PIXEL_SIZE;  // 288
  const int H        = LED_HEIGHT * PIXEL_SIZE;  // 192
  const int rowBytes = W * 3;                    // 864
  const int fileSize = 54 + rowBytes * H;        // 165,942

  // 54-byte BMP header (BITMAPFILEHEADER + BITMAPINFOHEADER)
  uint8_t hdr[54] = {};
  // BITMAPFILEHEADER
  hdr[0] = 'B';  hdr[1] = 'M';
  hdr[2] = (uint8_t)(fileSize);
  hdr[3] = (uint8_t)(fileSize >> 8);
  hdr[4] = (uint8_t)(fileSize >> 16);
  hdr[5] = (uint8_t)(fileSize >> 24);
  hdr[10] = 54;                                  // pixel data offset
  // BITMAPINFOHEADER
  hdr[14] = 40;                                  // DIB header size
  hdr[18] = (uint8_t)(W);
  hdr[19] = (uint8_t)(W >> 8);
  // Negative height = top-down row order (matches sprite layout)
  int32_t negH = -(int32_t)H;
  memcpy(&hdr[22], &negH, 4);
  hdr[26] = 1;                                   // colour planes
  hdr[28] = 24;                                  // bits per pixel

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.setContentLength(fileSize);
  server.send(200, "image/bmp", "");
  server.sendContent((const char*)hdr, 54);

  // Stream rows — RGB332 → BGR888
  // RGB332 bit layout: RRR GGG BB (bits 7..0)
  // Expand each channel by replicating MSBs to fill 8 bits.
  const uint8_t* px = (const uint8_t*)matrixSprite.getPointer();
  uint8_t row[864];
  for (int y = 0; y < H; y++) {
    const uint8_t* src = px + y * W;
    for (int x = 0; x < W; x++) {
      uint8_t p = src[x];
      uint8_t r = (p & 0xE0) | ((p & 0xE0) >> 3) | ((p & 0xC0) >> 6);
      uint8_t g = ((p & 0x1C) << 3) | (p & 0x1C)  | ((p & 0x18) >> 3);
      uint8_t b = ((p & 0x03) << 6) | ((p & 0x03) << 4) | ((p & 0x03) << 2) | (p & 0x03);
      row[x * 3 + 0] = b;   // BMP pixel order is BGR
      row[x * 3 + 1] = g;
      row[x * 3 + 2] = r;
    }
    server.sendContent((const char*)row, rowBytes);
  }
  DBG_VERBOSE("Screenshot served (%d bytes)", fileSize);
}

// ── /api/info ─────────────────────────────────────────────────────────────────
// Returns a JSON object with runtime state — foundation for future WebUI.
//
static const char* const MODE_NAMES[NUM_MODES] = {
  "Slide", "Pong", "Digits", "WordClock"
};

static void handleApiInfo() {
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"firmware\":\"%s\","
    "\"mode\":%d,"
    "\"modeName\":\"%s\","
    "\"brightness\":%d,"
    "\"uptime\":%lu,"
    "\"freeHeap\":%u,"
    "\"ip\":\"%s\"}",
    FW_VERSION,
    (int)clock_mode,
    MODE_NAMES[clock_mode < NUM_MODES ? clock_mode : 0],
    (int)currentBrightness,
    millis() / 1000UL,
    (unsigned)ESP.getFreeHeap(),
    WiFi.localIP().toString().c_str()
  );
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", buf);
}

// ── / ─────────────────────────────────────────────────────────────────────────
static void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ── Public API ────────────────────────────────────────────────────────────────
void initWeb() {
  if (WiFi.status() != WL_CONNECTED) {
    DBG_WARN("Web server skipped — WiFi not connected");
    return;
  }
  server.on("/",               HTTP_GET, handleRoot);
  server.on("/screenshot.bmp", HTTP_GET, handleScreenshot);
  server.on("/api/info",       HTTP_GET, handleApiInfo);
  server.onNotFound(handleNotFound);
  server.begin();
  webStarted = true;
  DBG_INFO("Web server started — http://%s/", WiFi.localIP().toString().c_str());
}

void webLoop() {
  if (webStarted) server.handleClient();
}

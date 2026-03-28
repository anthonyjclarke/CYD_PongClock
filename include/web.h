#pragma once
// web.h — HTTP server: live screenshot, hardware mockup page, /api/info

void initWeb();   // call after initWiFi() — no-op if WiFi not connected
void webLoop();   // call every loop() iteration

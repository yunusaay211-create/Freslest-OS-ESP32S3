# Wi-Fi and status bar in Freslest 6.12

Open Settings > WiFi. **Scan and choose a network** lists nearby networks
without replacing the main menu. Select an open network to join, or enter its
password with buttons, mouse, USB keyboard or BLE keyboard. **Add network
manually** supports a hidden SSID. **Connect saved network** retries the saved
credentials. SSIDs are limited to 32 bytes and passwords to 64 bytes.

Wi-Fi belongs to the system, not the Settings page. BACK, Task Switcher and
opening another normal app leave it connected. Disconnection triggers bounded
background retries. The WiFi OFF option explicitly disables station Wi-Fi and
keeps the saved credentials. The pre-existing Camera app temporarily uses its
own network; the saved system network is restored after leaving Camera.

For phone setup, select **Phone setup portal: ON**. Join **Freslest-Setup**
on your phone and open **http://192.168.4.1/**. Save Wi-Fi credentials there;
Browser now runs directly on ESP32 and needs no server configuration. A blank
Wi-Fi password means an open network. The portal
starts automatically on entering WiFi when no SSID is saved.

The setup AP remains available after leaving the page until you explicitly
switch the portal off or restart. Turning the portal off keeps the station
connection. The setup AP is open; stop it after configuration.

At the top, **FRESLEST is followed by time and DHT11 temperature**. DHT11
continues to use **GPIO38**; an unavailable reading shows `--.- C`.
**Wi-Fi and Bluetooth icons are immediately left of the battery icon**.
Accent colour means connected, amber means enabled/waiting, dim means off.
The existing absent-battery cross is preserved.

When Wi-Fi connects, the system requests NTP automatically and uses Turkey
time (UTC+3). It retries if no valid time has arrived. Internet access and a
reachable time server are required for the first automatic adjustment. After
sync the ESP32 system clock advances without internet; complete power loss
requires another sync. Manual Clock setting remains available.

Wi-Fi radio, BLE reconnect and physical TV operation still require checks on
your board. Host tests verify lifecycle, status placement and retry decisions;
they do not measure radio or analog signal quality.

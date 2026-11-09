# esp32-mqtt-last-will-status — retained JSON online/offline presence with MQTT last will

IoT example by [Fluidwire](https://fluidwire.com/).

An ESP32 sketch that reports device presence on a single retained MQTT topic. It publishes a JSON birth message (IP, RSSI, firmware version, boot count, uptime) when it connects, registers a last-will so the broker publishes `{"state":"offline"}` if the device dies, and supports a clean-shutdown button because a clean MQTT disconnect does not trigger the will.

## Required hardware

- 1x ESP32 dev board (the BOOT button on GPIO0 is used for clean shutdown)
- WiFi and an MQTT broker (public `broker.hivemq.com` works for testing)

## Required libraries

- **PubSubClient** by Nick O'Leary (Library Manager)
- `WiFi` and `Preferences` are built into the ESP32 Arduino core

## Setup

1. Copy `config.h.example` to `config.h` and fill in your WiFi and broker details. Make `MQTT_CLIENT_ID` unique.
2. Flash the sketch and open Serial Monitor at **115200 baud**.
3. Watch the status topic from a PC:

```bash
mosquitto_sub -h broker.hivemq.com -t 'fluidwire/demo/presence/#' -v
```

## Expected serial output

```
[boot] #1, firmware 1.0.0
[wifi] connecting to your-wifi-ssid
.....
[wifi] IP: 192.168.1.50
[mqtt] connecting with last will... ok
[mqtt] status -> {"state":"online","ip":"192.168.1.50","rssi":-58,"fw":"1.0.0","boots":1,"uptime_s":4}
```

Pull the USB cable and, within roughly 15 seconds, the broker publishes `{"state":"offline"}` on your behalf. Hold the BOOT button for 2 seconds to see the clean path: `{"state":"offline","reason":"clean"}`.

## Troubleshooting

- **Offline never appears:** the will only fires on an unclean drop. Unplug power instead of calling `disconnect()`.
- **Flapping online/offline:** two devices share the same `MQTT_CLIENT_ID`; the broker kicks the older one. Make each ID unique.
- **Late subscriber sees nothing:** check that your broker allows retained messages.
- **Slow offline detection:** lower `MQTT_KEEPALIVE_S` (detection takes about 1.5x keepalive).

## Security note

Never commit `config.h`; it is git-ignored. Public brokers are readable by anyone, so use a private broker with TLS and credentials for real deployments.

## License

MIT — see [LICENSE](LICENSE).

## Got stuck?

Drop by [fluidwire.com](https://fluidwire.com/) — happy to help.

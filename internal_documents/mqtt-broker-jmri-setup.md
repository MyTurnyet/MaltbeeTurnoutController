# MQTT Broker and JMRI Setup — Windows Surface 3

Companion to the ESP32 turnout controller project notes. Covers installing Mosquitto on the machine running JMRI, configuring JMRI's MQTT connection, and creating turnouts.

---

## 1. Why a broker is needed at all

**JMRI's MQTT connection is a client, not a broker.** JMRI does not include an embedded broker and does not expose an MQTT endpoint for devices to connect to. Pointing an ESP32 at JMRI's IP on port 1883 will find nothing listening.

MQTT is publish-subscribe: JMRI connects *out* to a broker and subscribes to topics. Your nodes connect *out* to the same broker. Neither ever connects to the other.

```
JMRI <--> MQTT broker <--> Node 1 / Node 2 / Node 3
```

**Consequences worth internalising:**

- JMRI has no idea how many nodes exist. It sees topics, not devices.
- Adding a node needs no JMRI change beyond adding turnouts — no new connection, no restart.
- **Nothing enforces turnout numbering.** Two nodes claiming turnout 105 will both act on it and neither will complain. Discipline is yours.
- Either side can restart without the other caring. This peer relationship is a real advantage over designs where layout hardware depends on JMRI being open.

**"Separate broker" does not mean separate hardware.** Mosquitto runs as a Windows Service on the machine already running JMRI. A few megabytes of RAM. Functionally more like installing a driver than adding a server.

---

## 2. The gotcha that catches everyone

Mosquitto 2.0 changed two defaults, and **both** break this use case.

1. Run without a config file, or without a configured listener, and it binds only to the loopback interface. Only local connections work. Your ESP32 is refused.
2. All listeners now require authentication, so `allow_anonymous` defaults to false. **Adding a listener alone still rejects your node.**

The working minimum is two lines:

```
listener 1883
allow_anonymous true
```

Anonymous access is fine on a layout LAN — same trust model as your DCC bus. Don't do it on a machine exposed to the internet.

Reference: https://mosquitto.org/documentation/migrating-to-2-0/

---

## 3. Install steps

### 3.1 Download and install

Get the Windows installer from https://mosquitto.org/download/

Run **as administrator**. Keep the **Service** component checked — that's what makes the broker start with Windows rather than needing a manual launch before every session.

Default location: `C:\Program Files\mosquitto`

### 3.2 Configure

Open `C:\Program Files\mosquitto\mosquitto.conf` in an editor **started as administrator** — you cannot save into Program Files otherwise.

Add:

```
listener 1883
allow_anonymous true
```

Save.

### 3.3 Restart the service

`services.msc` → **Mosquitto Broker** → Restart. Confirm startup type is **Automatic**.

If the service won't start, the config almost certainly has a typo. Run `mosquitto.exe -v` from an admin command prompt in the install directory to see the error directly — much clearer than the Event Viewer.

### 3.4 Open the firewall

Windows Firewall blocks inbound 1883 by default. Add an inbound rule for **TCP 1883**, scoped to the **private** network profile.

Without this the service runs happily and the ESP32 still can't reach it — a failure that looks identical to a misconfigured broker.

### 3.5 Verify from a different machine

Find the Surface's LAN IP with `ipconfig`. Then from **another machine on the same network**:

```
mosquitto_sub -h <surface-ip> -t 'test'
mosquitto_pub -h <surface-ip> -t 'test' -m 'hello'
```

**Testing from the Surface itself proves nothing** — loopback works even when the listener is misconfigured. This is the single most useful verification step.

---

## 4. Surface 3 specific concerns

### 4.1 Sleep will take down layout control

The real risk with a tablet. If the Surface sleeps mid-session you lose JMRI *and* Mosquitto simultaneously.

- Power plan: never sleep while plugged in
- Device Manager → network adapter → Power Management → **uncheck** "Allow the computer to turn off this device to save power". This one causes intermittent dropouts that look like broker faults.

### 4.2 The broker address must be stable

JMRI can use `localhost` since it's on the same machine. **Your ESP32 cannot** — it needs the Surface's LAN IP, stored in NVS.

If DHCP hands out a different address after a router reboot, every node stops working until reflashed. **Reserve a fixed DHCP lease for the Surface on your router.** Five minutes now.

### 4.3 Performance

If it's the 2015 Surface 3 (Atom, 2 GB RAM), that's modest for JMRI — but Mosquitto won't be what tips it over. The broker is negligible next to a Java application.

If JMRI already feels sluggish, moving both JMRI and the broker to a Raspberry Pi is well-trodden. **The nodes wouldn't care** — one IP address changes in NVS. Nothing in the design depends on where the broker lives.

---

## 5. JMRI configuration

### 5.1 Add the MQTT connection

Edit → Preferences → Connections → Add → **MQTT Connection**.

- IP address: `localhost` (broker is on this machine)
- Port: 1883
- Connection prefix: defaults to **M**

Restart JMRI.

Reference: https://www.jmri.org/help/en/html/hardware/mqtt/index.shtml

### 5.2 System names and topics

MQTT system names are: connection prefix + `T` for turnout + free-form suffix. The suffix generates the topic.

By default JMRI prepends `/trains/track/turnout/` to the suffix. So `MT101` publishes and subscribes to `/trains/track/turnout/101`.

The suffix need not be numeric — `MTyard-3` is valid — but numeric aligns with the node's block arithmetic.

### 5.3 Numbering scheme — node-prefixed

**Node N owns N01 through N16.**

| Node | Turnouts | System names | Topics |
|---|---|---|---|
| 1 | 101–116 | MT101–MT116 | /trains/track/turnout/101… |
| 2 | 201–216 | MT201–MT216 | /trains/track/turnout/201… |
| 3 | 301–316 | MT301–MT316 | /trains/track/turnout/301… |

Readable without arithmetic, growth doesn't force renumbering, and node ownership is `id / 100 == nodeId` with channel `id % 100`.

### 5.4 Add turnouts

Tools → Tables → Turnouts → **Add**.

- System name: `MT101`
- User name: something locating it, e.g. `Yard Lead (N1-01)`

**Put the node identity in the user name.** JMRI can't show which node owns a turnout — it doesn't know. User names appear on panels and in the Routes editor, so this costs nothing and pays off when diagnosing.

**The system name cannot be changed once entered.** Get the scheme right first.

### 5.5 Feedback mode

Set in the turnout's edit pane in the Turnout Table.

**This is the one setting to verify empirically rather than trust.** JMRI's ONESENSOR/TWOSENSOR modes mean feedback via separate JMRI *Sensor objects*. Our node publishes state on the turnout's own receive topic, which is closer to **MONITORING**.

Test: set a turnout to MONITORING, publish `THROWN` by hand with `mosquitto_pub`, and see whether JMRI's known state follows.

### 5.6 Send and receive topics

The connection preferences have separate fields for send (JMRI → layout) and receive (layout → JMRI). JMRI recommends setting them the same for most users, but notes that splitting them is intended for custom-programmed microcontrollers — i.e. exactly this project.

**Start with them the same.** Split later if independent tracing of command path vs state path would help debugging.

---

## 6. Test the JMRI side before any hardware exists

This is build-sequence step 2, and it's worth doing properly.

1. `mosquitto_sub -h localhost -t '/trains/#' -v` in a command window
2. Click a turnout's state button in the JMRI Turnout Table
3. Confirm the command appears on the broker
4. `mosquitto_pub -h localhost -t '/trains/track/turnout/101' -m 'THROWN'`
5. Confirm JMRI's known state updates

**Do this before the ESP32 exists.** It proves the JMRI half independently, so when the node later misbehaves you know which side to investigate.

It also answers three open design questions in one sitting:

- Which feedback mode actually consumes a state payload on the receive topic
- Whether a confirming payload is a no-op or fires a state-change event
- Whether JMRI accepts `UNKNOWN` as an inbound payload

---

## 7. Your existing DR-4018 decoders are unaffected

JMRI handles multiple connections simultaneously. DCC turnouts keep their system names on the DCC connection; MQTT turnouts live under `M`.

Turnouts are abstract above the connection layer — panels, routes, signal logic and CTC don't know or care which connection a turnout belongs to. A DR-4018 turnout and an MQTT turnout can sit in the same route.

**There is no "programming" in the DR-4018 sense.** No CVs, no programming track, no DecoderPro, no POM. Configuration splits into two surfaces that never touch:

| Surface | Holds | Set how |
|---|---|---|
| **Node** | pin assignments, orientation, timings, node id, WiFi, broker IP | Serial command at the bench, stored in NVS |
| **JMRI** | which turnout numbers exist, topics, feedback mode | Turnout Table |

The contract between them is just the topic string.

---

## 8. Troubleshooting

| Symptom | Likely cause |
|---|---|
| Service won't start | Config typo. Run `mosquitto.exe -v` to see it. |
| Works from the Surface, not from elsewhere | Missing `listener 1883`, or firewall rule absent |
| `listener` added but clients still refused | Missing `allow_anonymous true` — both lines are required |
| Worked yesterday, dead today | Surface slept, or DHCP changed its IP |
| Intermittent dropouts | WiFi adapter power saving |
| JMRI connects, node doesn't | Node using `localhost` instead of the LAN IP |
| Commands sent, nothing happens | Topic mismatch — compare `mosquitto_sub -v` output against what the node subscribes to |

---

## 9. Links

- Mosquitto downloads — https://mosquitto.org/download/
- Mosquitto 2.0 migration notes (the two-line gotcha) — https://mosquitto.org/documentation/migrating-to-2-0/
- mosquitto.conf reference — https://mosquitto.org/man/mosquitto-conf-5.html
- JMRI MQTT hardware support — https://www.jmri.org/help/en/html/hardware/mqtt/index.shtml
- JMRI Turnout Table help — https://webserver.jmri.org/help/en/package/jmri/jmrit/beantable/TurnoutTable.shtml
- JMRI turnout feedback concepts — https://webserver.jmri.org/help/en/html/doc/Technical/TurnoutFeedback.shtml

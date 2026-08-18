# First-Time Setup Guide (Wireless — No Computer Required)

This guide walks a customer through configuring a new Maltbee Turnout
Controller board using only a phone or laptop's WiFi — no USB cable to a
computer, no serial terminal, no software to install. It documents the
**wireless commissioning** flow described in
`docs/software-class-list.md` ("Wireless Commissioning & Field
Identification").

> A USB (or other) *power* connection is still required — the board has to be
> powered on to do anything. What this guide avoids is plugging the board
> into a **computer** to run setup commands over a serial connection.

## What You'll Need

- The board, powered on (via whatever power connection it ships with).
- A phone, tablet, or laptop with WiFi.
- Your home WiFi network name (SSID) and password. The ESP32 only joins
  **2.4 GHz** networks — if your home network is 5 GHz-only, use your
  router's 2.4 GHz band/SSID instead.
- The address and port of your MQTT broker (the JMRI computer's IP address
  and the broker port — ask whoever set up JMRI if you don't know this;
  the default MQTT port is `1883`).
- A node ID for this board: a number from **1 to 16**, unique among all the
  turnout controller boards on your layout.

## Step 1: Enter Setup Mode

1. Make sure the board is already powered on and running normally — don't
   hold BOOT while plugging it in or resetting it.
2. Press and hold the board's **BOOT** button for about 3 seconds, then
   release it.

Releasing BOOT after that hold reboots the board into setup mode: it skips
normal startup and starts its own WiFi network instead of joining yours.

## Step 2: Connect to the Board's WiFi Network

Look for a WiFi network named:

```
Tortoise-Setup-XXXX
```

where `XXXX` is 4 characters unique to that board (taken from its hardware
address) — this is how you tell multiple new boards apart if you're setting
several up at once. The network is open (no password needed).

Connect your phone or laptop to it.

## Step 3: Open the Setup Page

Most phones and laptops will automatically pop up a "sign in to network"
page within a few seconds of connecting — that's the board's setup form.

If it doesn't appear automatically, open a web browser and go to:

```
http://192.168.4.1
```

## Step 4: Fill Out the Form

Enter:

- **Node ID** — the 1–16 number you chose for this board.
- **WiFi SSID** / **WiFi Password** — your home network credentials.
- **Broker Host** / **Broker Port** — your MQTT broker's address (e.g.
  `192.168.1.50`) and port (usually `1883`).

Press **Save**.

- If everything is valid, the page will show `REBOOTING` and the board will
  restart, join your home WiFi, and connect to the broker.
- If something's wrong (e.g. the node ID isn't a number 1–16, or the broker
  port isn't a number), you'll see a message starting with `ERROR:` instead,
  and nothing is saved. Go back (Step 1) and try again with corrected
  values.

## Step 5: Confirm It's Running

Once rebooted, the board's status LED tells you what's going on:

| LED behavior | Meaning |
|---|---|
| Off | Normal — running quietly, nothing to report. |
| Blinks its node number (N short blinks, pause, repeat) | Someone short-pressed BOOT to ask "which board is this?" — see below. |
| Fast, steady blinking (no pauses) | **Duplicate node ID** — another board on the network already claims this ID. Re-enter setup mode (Step 1) and give this board a different ID. |

To confirm a board's identity once it's running normally, **briefly** press
BOOT (a quick tap, not a hold) — the LED will blink out its node number for
about 5 seconds, so you can match the physical board to its ID on the
layout.

## Changing Settings Later

You can re-run this whole process at any time — e.g. to move a board to a
new WiFi network, or fix a duplicate node ID — by repeating Step 1. Holding
BOOT for 3 seconds during normal operation, then releasing it, always
re-enters setup mode, even on a board that's already configured and
working.

## Troubleshooting

- **Don't see `Tortoise-Setup-XXXX` in your WiFi list:** the hold probably
  wasn't caught, or was released too early. Make sure the board is fully
  powered on and running first, then press and hold BOOT for a full 3
  seconds and release it — check again after it reboots.
- **The setup page didn't pop up automatically:** browse to
  `http://192.168.4.1` manually (Step 3).
- **Turnouts don't move even though the board is online:** turnout wiring
  (which GPIO pin drives which Tortoise, feedback pins, orientation, etc.)
  is **not** configured through this wireless form — it's set up ahead of
  time by the builder over a USB serial connection. If turnouts aren't
  responding on a board that otherwise joined WiFi/MQTT successfully, that
  wiring likely wasn't completed — contact whoever built the board rather
  than trying to fix it wirelessly.
- **Forgot the WiFi password you entered, or need to redo it:** there's no
  way to review saved settings from the wireless form — just re-enter setup
  mode (Step 1) and submit the form again with corrected values.

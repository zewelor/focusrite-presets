# Scarlett 4i4 front-panel controls

`scarlett-front-panel` is a small, one-shot Linux helper for controlling the
front-panel LEDs on a Focusrite Scarlett 4i4 4th Gen running firmware 2417 or
later. It works without a kernel rebuild, even when the installed `scarlett2`
driver does not expose the new ALSA controls yet.

The helper supports only two settings:

- active LED brightness: `high`, `medium`, or `low`;
- automatic LED sleep after 0 through 86400 seconds.

It cannot change arbitrary Scarlett configuration.

## Build

The build requires a C compiler, `make`, `pkg-config`, and the libusb 1.0
development files.

```sh
make
```

## Read the current settings

Always use the wrapper rather than invoking the binary directly:

```sh
./run-exclusive
```

Example output:

```text
Focusrite firmware: 2417
Protocol probe (master volume at 0x32): 0xffe6
Front-panel brightness: 2 (Low)
Front-panel sleep time: 600 seconds
```

Running without options is read-only.

## Set brightness

The supported brightness values are:

| Argument | Device value | Meaning |
| --- | ---: | --- |
| `high` | 0 | brightest |
| `medium` | 1 | intermediate |
| `low` | 2 | dimmest supported active state |

Examples:

```sh
./run-exclusive --brightness low
./run-exclusive --brightness medium
./run-exclusive --brightness high
```

There is no supported brightness value below `low`. To turn the LEDs off,
configure the idle sleep timeout instead.

## Set automatic sleep

The sleep timeout is specified in seconds:

```sh
./run-exclusive --sleep 300
```

Common values:

| Timeout | Argument |
| --- | ---: |
| never sleep | `0` |
| 1 second | `1` |
| 30 seconds | `30` |
| 5 minutes | `300` |
| 10 minutes | `600` |
| 1 hour | `3600` |
| 24 hours | `86400` |

Zero disables automatic sleep. One second is the shortest enabled timeout.
Front-panel adjustments and passing audio count as activity and wake or keep
the LEDs awake. After waking, the panel uses the configured brightness.

## Set both values at once

Options can be combined. This sets the dimmest active brightness and turns the
LEDs off after five minutes of inactivity:

```sh
./run-exclusive --brightness low --sleep 300
```

Every requested setting is read back from the device and verified before the
program exits successfully.

## Why `run-exclusive` is required

The helper communicates directly with USB device `1235:821a`. The kernel
driver and a userspace helper cannot safely consume the same protocol
acknowledgements simultaneously.

`run-exclusive` therefore:

1. obtains `sudo` authentication;
2. stops WirePlumber;
3. temporarily unloads `snd_usb_audio`;
4. runs the helper;
5. reloads `snd_usb_audio` and starts WirePlumber, including after an error.

All USB audio devices are interrupted briefly during this operation. Do not
run Focusrite Control, ALSA Scarlett GUI, or another configuration tool at the
same time.

Run `./scarlett-front-panel --help` to display the command syntax without
accessing the USB device.

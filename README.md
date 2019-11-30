# anniebelle

## Purpose

Anniebelle displays an image when the system bell rings. This may be used in addition to or as a replacement to the audible bell.

## Requirements

The project depends on gtk 3.

## Building

Consider passing `--prefix=$HOME` to `./configure` to install in your home directory

```
./configure
make
make install
```

## Usage

`anniebelle path-to-image`

## Configuration Ubuntu 18.04

Set anniebelle to autostart in [Startup Applications](https://help.ubuntu.com/stable/ubuntu-help/startup-applications.html.en)

Optionally disable audible bell by going to Settings / Sound / Sound Effects, and dragging the alert volume slider to muted.

## Other distros note

anniebelle must be started in such a way that it can connect to the x server

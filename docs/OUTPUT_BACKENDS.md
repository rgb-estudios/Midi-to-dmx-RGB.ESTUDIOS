# Output backends

## Common interface

Every backend implements:

- enumerate;
- configure;
- open;
- arm/disarm;
- submit latest 512-byte frame;
- report status/error;
- close safely.

The runtime submits immutable frames; backend-specific code cannot alter semantic values.

## Art-Net version 1

- ArtDMX output.
- UDP port 6454.
- Unicast and broadcast.
- Select network adapter.
- Configurable 15-bit port address shown to users as net/subnet/universe or simplified universe.
- Sequence number.
- 30–44 Hz selectable refresh; 40 Hz default.
- One universe.
- Optional ArtPoll discovery later; not required for first hardware test.

Reference: official Art-Net 4 specification at https://art-net.org.uk/art-net-specification/

## DMX USB Pro compatible

- Serial/device discovery.
- Implement documented ENTTEC packet protocol.
- Buffered interface preferred for performance.
- Exact clone compatibility is hardware-tested, never assumed from marketing text.

Reference: https://support.enttec.com/dmx/usbdmx-dmx-usb-pro-70304/dmx-usb-pro-api

## Open DMX / FTDI

- Optional backend.
- Requires compatible FTDI device and driver.
- Software must generate DMX timing; more sensitive to scheduling.
- Not accepted for show use until soak-tested on the target computer.

## Unsupported phrase

Never advertise “all generic USB DMX interfaces.” Support is named by protocol family and tested VID/PID/model.

## Backend selection

A project stores preferred backend configuration, but the receiving computer may override device/IP without changing authored looks or patch semantics.

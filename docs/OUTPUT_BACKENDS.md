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
- Numeric IPv4 unicast only in Alpha v1; DNS, broadcast and multicast are
  rejected by preflight.
- Operating-system route selection; explicit adapter selection is not yet in
  the product UI.
- Configurable 15-bit port address shown as simplified universe `0..32767`.
- Sequence number.
- Fixed 40 Hz refresh in Alpha v1.
- One universe.
- Optional ArtPoll discovery later; not required for first hardware test.

The graphical product persists `backend`, numeric target and universe, then
opens the socket disarmed. `ARM OUTPUT` only enables refresh after project,
runtime and backend gates pass. Disable, project reload, offline render, send
error and shutdown request a safe transition. UDP preflight proves address
syntax, ownership and local socket creation; it does not prove node reception.

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

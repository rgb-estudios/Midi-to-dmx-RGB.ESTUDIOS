# Real-time and safety requirements

## Startup

- Output disarmed.
- DMX universe initialized to safe homes.
- Dimmer/strobe/haze/macro/reset zero or explicit safe values.
- User must select `ARM OUTPUT`.

## Stop and close

Configurable policy:

1. send blackout/safe frame repeatedly for a defined interval;
2. disarm backend;
3. close device/socket.

Default: safe-frame burst, then close.

## Host transport

Host stop must not accidentally trigger reset or macro. Project may choose:

- hold current base look;
- fade to configured idle scene;
- blackout;
- continue free-running preview only while output is disarmed.

## Dangerous attributes

- Reset and lamp-control segments require explicit confirmation and timeout.
- Strobe has configurable global maximum and per-fixture profile range.
- Haze has global emergency-off and maximum duty/intensity.

## Watchdogs

- Render heartbeat.
- Output heartbeat.
- Backend error counter.
- Media decode underrun counter.
- Host callback queue-overflow counter.

Any unrecoverable output failure disarms output and displays a persistent error.

## Testing

No safety behaviour is considered show-tested until verified with the actual node/interface and fixtures.

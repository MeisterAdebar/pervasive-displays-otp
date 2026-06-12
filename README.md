# OTP Memory Analysis: Pervasive Displays ESL 5.81"


The OTP memory was successfully read using an ESP32 with bidirectional software SPI (command 0xB9). No hardware SPI is required — the SDIO line is toggled between OUTPUT and INPUT in firmware. Data is real and consistent — no floating-line noise.

---

## Hardware setup

| Signal | ESP32 pin | Notes |
|--------|-----------|-------|
| SDIO   | GPIO 23   | Bidirectional data line — switched between OUTPUT (command) and INPUT (data) via `pinMode()` |
| CLK    | GPIO 18   | Clock (~250 kHz, 2 µs high/low) |
| CS     | GPIO 15   | Chip select (active LOW) |
| DC     | GPIO 17   | Data / command select |
| RST    | GPIO 16   | Reset (active LOW) |
| BUSY   | GPIO  4   | Busy signal (input only — not polled during OTP read) |

No hardware SPI is used. The SDIO line direction is toggled in firmware.

---

## OTP read procedure

```
1. COG driver reset:
   RST HIGH (200 ms) → RST LOW (4 ms) → RST HIGH (20 ms)

2. Send command 0xB9:
   DC LOW, CS LOW
   SDIO as OUTPUT → shift out 0xB9 (MSB first)

3. Switch to data mode:
   DC HIGH
   SDIO as INPUT

4. Read and discard dummy byte (mandatory per datasheet)

5. Read 128 bytes (MSB first, sample on CLK rising edge)

6. CS HIGH, SDIO back to OUTPUT
```

---

## Raw dump (single measurement, 2026-06-11)

```
0x10 0x63 0x01 0x01 0x56 0x52 0x50 0x30 0x32 0x33 0x00 0x25 0x00 0x1F 0x50 0xC8    [Bytes 00-15]
0x09 0x00 0x00 0x50 0x00 0x00 0x1F 0x50 0x00 0x1F 0x03 0x00 0x00 0x00 0xFF 0xFF    [Bytes 16-31]
0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x84 0x50 0x00 0x00 0x01 0x1F 0x9F 0x81    [Bytes 32-47]
0x8A 0x0A 0x00 0x00 0x01 0x1F 0x9F 0x81 0x88 0x0A 0x02 0x00 0x01 0x7F 0xFF 0x83    [Bytes 48-63]
0x88 0x0A 0x0A 0xFF 0x00 0x7F 0xFF 0x81 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF    [Bytes 64-79]
0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF    [Bytes 80-95]
0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x53 0x31 0x32 0x35 0x38 0x31 0x48 0x53    [Bytes 96-111]
0x30 0x42 0x31 0x30 0x32 0x33 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF    [Bytes 112-127]
```

Dummy byte: `0xFF`

---

## Known fields

### Chip and panel identification

| Index | Value | Interpretation |
|-------|-------|----------------|
| 0x00  | `0x10` | Chip ID |
| 0x01  | `0x63` | COG type |
| 0x04–0x09 | `VRP023` (ASCII) | Likely Voltage Reference Profile identifier |
| 0x68–0x75 | `S12581HS0B1023` (ASCII) | Panel part number / lot code (bytes 104–117) |

> **Note on IDs:** The values `0x10` / `0x63` differ from the Pervasive Displays
> reference documentation (Chip ID `0x16`, COG Type `0x95`). This is not a read
> error — the OTP read works correctly. This appears to be a different COG
> revision, possibly OEM-specific. The panel ID `S12581` confirms a 5.81" panel.

### Waveform data (suspected)

Bytes 0x20–0x4F (32–79) contain structured non-0xFF values, likely waveform
tables (voltage/timing sequences for EPD refresh). Not yet decoded.

```
Bytes 32-47:  [0xFF×8] 0x84 0x50 0x00 0x00 0x01 0x1F 0x9F 0x81
Bytes 48-63:  0x8A 0x0A 0x00 0x00 0x01 0x1F 0x9F 0x81 0x88 0x0A 0x02 0x00 0x01 0x7F 0xFF 0x83
Bytes 64-79:  0x88 0x0A 0x0A 0xFF 0x00 0x7F 0xFF 0x81 [0xFF×8]
```

Recurring patterns (`0x81`, `0x83`, `0x1F 0x9F`, `0x7F 0xFF`) suggest
tabular waveform phase encoding.

### Unknown fields

| Range | Values | Status |
|-------|--------|--------|
| 0x02–0x03 | `0x01 0x01` | Unknown |
| 0x0A | `0x00` | Unknown |
| 0x0B | `0x25` | Possibly temperature offset or calibration value |
| 0x0C–0x0F | `0x00 0x1F 0x50 0xC8` | Unknown (may encode resolution or timing) |
| 0x10–0x1D | Structured values | Unknown |
| 0x1E–0x27 | `0xFF×10` | Empty / unused |

---

## Open questions / TODO

- [ ] Cross-reference Chip ID `0x10` / COG Type `0x63` with other Pervasive Displays datasheets
      (possibly a newer generation than EK079KH1240 / UC8159)
- [ ] Decode waveform bytes 32–79 — compare against known EPD waveform formats
- [ ] Clarify meaning of `VRP023` (Voltage Reference Profile? Waveform revision?)
- [ ] Look up panel ID `S12581HS0B1023` in manufacturer documentation
- [ ] Read a second unit — verify whether OTP data is identical across samples

---

## Sketch

→ [`src/otp_reader_581/otp_reader_581.ino`](../src/otp_reader/otp_reader.ino)

---

## References

- Pervasive Displays Knowledge Base — Update Procedure (OTP, COG init, frame data):
  https://docs.pervasivedisplays.com/knowledge/Hardware/epd-usage/Screens/Wide_Medium/Update_Procedure.html
- Pervasive Displays GitHub (official drivers, incl. EPD_Driver_GU_mid for 5.81" / 7.4"):
  https://github.com/PervasiveDisplays
- Pervasive Displays EPD Driving Circuit Rev.02 (PDF, schematic reference, 5.81" = Group G3):
  https://www.pervasivedisplays.com/wp-content/uploads/2025/10/EPD-driving-circuit-of-Pervasive-Displays-rev.02.pdf
- Own measurement with ESP32 DevKit v1, directly on the FPC connector of the ESL panel

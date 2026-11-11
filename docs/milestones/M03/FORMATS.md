# M03 disc formats

Produced by `tools/disc_formats.py`. Counts and verdicts are in
[format_coverage.json](format_coverage.json), which holds metadata only. All 341 disc
files were checked on the user's image with no failures, except formats marked below
as having no structural check.

| Format | Files | What was checked | Run-time owner |
|---|---:|---|---|
| `DTZ` | 1 | Whole zlib stream ends; inflates to `GATG` (`GTAG` byte-reversed) | Guest. Upstream can inflate natively; the guest relocates the image |
| `LVZ` | 3 | Whole zlib stream ends; inflates to `DLRW` (`WRLD` byte-reversed) | Guest, one level loaded at a time |
| `IMG` | 6 | Size and header class only | Guest streaming through sector reads. There is no generic archive reader |
| `DIR` | 1 | 147 records of 32 bytes, each within its IMG | Guest |
| `GXT` | 6 | `TABL`, 12-byte records, offsets within the file | Guest |
| `XTX` | 30 | `xet` magic | Guest, rasters through the GS path |
| `PSP` | 48 | Layout text header | Guest. PS2 frontend data despite the name |
| `PSS` | 4 | MPEG program stream start code | Native movie player |
| `VB` | 93 | Whole 16-byte ADPCM blocks | Native sound driver |
| `RAW` | 68 | None | Native sound driver |
| `SDT` | 68 | 7,721 records of 12 bytes, offsets within the paired RAW | Native sound driver |
| `IRX` | 8 | ELF magic | IOP HLE; the modules are not run as guest IOP code |
| `90` (executable) | 1 | ELF magic, SHA-1 by the extractor | Recompiled, identity checked |
| `SCM`, `BIN`, `ICO`, `CNF` | 4 | None | Guest, native sound lookup, card icon, boot metadata |

## Decisions

- **No archive is unpacked.** The port serves the original bytes at the original LBAs.
  The guest's own parsers read these formats, so no member-level loader is needed for
  boot. Member-level parsing belongs to the post-beta mod work (M11, M12).
- **Boot-critical gaps: none found in the file set.** Every file the boot path reads
  exists, and the live comparison in [VERIFICATION.md](VERIFICATION.md) saw every read
  match the disc. `IOPRP300.IMG` is never read by the current upstream paths.
- **Formats with no structural check** (`RAW`, `SCM`, `BIN`, `ICO`, `CNF`) still have
  whole-file SHA-256 in the private manifest. A check here would only restate what the
  guest parser or sound driver already does.
- `IMG` and `VB` interiors are not enumerated. Only the guest or sound driver knows
  their layout, and which members are used is for M06 and M07 to prove in play.

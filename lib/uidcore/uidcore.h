// uidcore — pure, hardware-independent helpers used by the firmware.
//
// Nothing here touches Serial, SPI or the MFRC522, so the same code compiles
// both for the Uno and for the host (`pio test -e native`).
#pragma once

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Hex parsing
// ---------------------------------------------------------------------------

enum HexParseStatus {
  HEX_OK,          // parsed at least one whole byte
  HEX_EMPTY,       // no hex digits at all
  HEX_ODD_DIGITS,  // a trailing nibble was left without its pair
};

// Parse bytes from text like "79 D6 83 63", "79:D6:83:63" or "79D68363".
// Any non-hex character is treated as a separator and skipped.
//
// Up to `maxOut` bytes are stored into `out`; `*count` always receives the
// total number of whole bytes found, even when that exceeds `maxOut`, so the
// caller can report "need 4, got 10".
HexParseStatus parseHexBytes(const char *s, uint8_t *out, uint8_t maxOut,
                             uint8_t *count);

// ---------------------------------------------------------------------------
// UID helpers
// ---------------------------------------------------------------------------

// Block Check Character of a 4-byte UID: XOR of all four bytes.
// On MIFARE Classic this is byte 4 of block 0.
uint8_t uidBcc(const uint8_t uid[4]);

// Replace bytes 0..3 of a 16-byte block-0 image with `uid` and recompute the
// BCC in byte 4. Bytes 5..15 (SAK, ATQA, manufacturer data) are left as-is.
void patchBlock0Uid(uint8_t block[16], const uint8_t uid[4]);

// Format `size` bytes as "79 D6 83 63": upper-case, zero-padded, single
// space between bytes, no trailing space. Always NUL-terminates `out`
// (if outLen > 0) and truncates at whole bytes if the buffer is too small.
// Returns the number of characters written, not counting the NUL.
size_t formatUid(const uint8_t *buf, uint8_t size, char *out, size_t outLen);

// ---------------------------------------------------------------------------
// Serial command line handling
// ---------------------------------------------------------------------------

enum Command {
  CMD_NONE,     // blank line
  CMD_READ,     // R
  CMD_SET_UID,  // U <hex>
  CMD_WRITE,    // W
  CMD_DIAG,     // D
  CMD_UNKNOWN,  // anything else
};

// Classify the first non-blank character of `line` (case-insensitive).
// `*args` is set to point right after the command character, so for
// "U 79 D6 83 63" it points at " 79 D6 83 63". For a blank line it points at
// the terminating NUL.
Command parseCommand(const char *line, const char **args);

// Accumulates serial bytes into a line. Mirrors the behaviour of a typical
// serial monitor: CR, LF or CRLF all terminate a line, empty lines are
// ignored, and characters past the capacity are dropped (the line is
// truncated, not split).
class LineReader {
 public:
  static const uint8_t kCapacity = 40;  // bytes, including the NUL

  LineReader() : len_(0) { buf_[0] = '\0'; }

  // Feed one incoming byte. Returns true when a complete, non-empty line is
  // ready; it is available through line() until the next feed() call.
  bool feed(char c);

  // The last completed line (NUL-terminated).
  const char *line() const { return buf_; }

 private:
  char buf_[kCapacity];
  uint8_t len_;
};

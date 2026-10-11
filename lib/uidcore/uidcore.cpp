#include "uidcore.h"

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

HexParseStatus parseHexBytes(const char *s, uint8_t *out, uint8_t maxOut,
                             uint8_t *count) {
  uint8_t n = 0;
  int hi = -1;  // pending high nibble, -1 = none

  for (const char *p = s; *p; p++) {
    int v = hexNibble(*p);
    if (v < 0) continue;  // separator: space, colon, dash, ...

    if (hi < 0) {
      hi = v;
    } else {
      if (n < maxOut) out[n] = (uint8_t)((hi << 4) | v);
      if (n < 255) n++;
      hi = -1;
    }
  }

  *count = n;
  if (hi >= 0) return HEX_ODD_DIGITS;
  if (n == 0) return HEX_EMPTY;
  return HEX_OK;
}

uint8_t uidBcc(const uint8_t uid[4]) {
  return uid[0] ^ uid[1] ^ uid[2] ^ uid[3];
}

void patchBlock0Uid(uint8_t block[16], const uint8_t uid[4]) {
  for (uint8_t i = 0; i < 4; i++) block[i] = uid[i];
  block[4] = uidBcc(uid);
}

size_t formatUid(const uint8_t *buf, uint8_t size, char *out, size_t outLen) {
  static const char kHex[] = "0123456789ABCDEF";
  if (outLen == 0) return 0;

  size_t pos = 0;
  for (uint8_t i = 0; i < size; i++) {
    // Space before every byte except the first, then two hex digits, then
    // room for the NUL. Stop at a byte boundary if it would not fit.
    size_t need = (i == 0 ? 2 : 3) + 1;
    if (outLen - pos < need) break;

    if (i > 0) out[pos++] = ' ';
    out[pos++] = kHex[buf[i] >> 4];
    out[pos++] = kHex[buf[i] & 0x0F];
  }
  out[pos] = '\0';
  return pos;
}

Command parseCommand(const char *line, const char **args) {
  const char *p = line;
  while (*p == ' ' || *p == '\t') p++;  // skip leading whitespace

  char cmd = *p;
  *args = (cmd != '\0') ? p + 1 : p;

  switch (cmd) {
    case '\0': return CMD_NONE;
    case 'r': case 'R': return CMD_READ;
    case 'u': case 'U': return CMD_SET_UID;
    case 'w': case 'W': return CMD_WRITE;
    case 'd': case 'D': return CMD_DIAG;
    default:  return CMD_UNKNOWN;
  }
}

bool LineReader::feed(char c) {
  if (c == '\n' || c == '\r') {
    if (len_ == 0) return false;  // empty line or the LF of a CRLF pair
    buf_[len_] = '\0';
    len_ = 0;
    return true;
  }
  if (len_ < kCapacity - 1) buf_[len_++] = c;
  return false;
}

// Unit tests for lib/uidcore — the hardware-independent part of the firmware.
//
// Run on the host:    pio test -e native
// Run on the board:   pio test -e uno      (Uno connected over USB)

#include <string.h>
#include <unity.h>

#include "uidcore.h"

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------
// parseHexBytes
// ---------------------------------------------------------------------------

static void test_parse_space_separated() {
  uint8_t out[8];
  uint8_t n = 0;
  TEST_ASSERT_EQUAL(HEX_OK, parseHexBytes(" 79 D6 83 63", out, sizeof(out), &n));
  TEST_ASSERT_EQUAL_UINT8(4, n);
  const uint8_t expected[] = {0x79, 0xD6, 0x83, 0x63};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 4);
}

static void test_parse_colon_dash_and_no_separators() {
  uint8_t out[8];
  uint8_t n = 0;
  const uint8_t expected[] = {0x79, 0xD6, 0x83, 0x63};

  TEST_ASSERT_EQUAL(HEX_OK, parseHexBytes("79:D6:83:63", out, sizeof(out), &n));
  TEST_ASSERT_EQUAL_UINT8(4, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 4);

  TEST_ASSERT_EQUAL(HEX_OK, parseHexBytes("79-D6-83-63", out, sizeof(out), &n));
  TEST_ASSERT_EQUAL_UINT8(4, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 4);

  TEST_ASSERT_EQUAL(HEX_OK, parseHexBytes("79D68363", out, sizeof(out), &n));
  TEST_ASSERT_EQUAL_UINT8(4, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 4);
}

static void test_parse_is_case_insensitive() {
  uint8_t out[8];
  uint8_t n = 0;
  TEST_ASSERT_EQUAL(HEX_OK, parseHexBytes("de ad Be eF", out, sizeof(out), &n));
  TEST_ASSERT_EQUAL_UINT8(4, n);
  const uint8_t expected[] = {0xDE, 0xAD, 0xBE, 0xEF};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 4);
}

static void test_parse_empty_input() {
  uint8_t out[8];
  uint8_t n = 99;
  TEST_ASSERT_EQUAL(HEX_EMPTY, parseHexBytes("", out, sizeof(out), &n));
  TEST_ASSERT_EQUAL_UINT8(0, n);

  TEST_ASSERT_EQUAL(HEX_EMPTY, parseHexBytes("  : - ", out, sizeof(out), &n));
  TEST_ASSERT_EQUAL_UINT8(0, n);
}

static void test_parse_odd_digit_count() {
  uint8_t out[8];
  uint8_t n = 0;
  TEST_ASSERT_EQUAL(HEX_ODD_DIGITS, parseHexBytes("79 D6 83 6", out, sizeof(out), &n));
  // The three whole bytes before the dangling nibble are still reported.
  TEST_ASSERT_EQUAL_UINT8(3, n);
}

static void test_parse_counts_beyond_buffer() {
  uint8_t out[8];
  memset(out, 0, sizeof(out));
  uint8_t n = 0;
  TEST_ASSERT_EQUAL(HEX_OK,
                    parseHexBytes("00 11 22 33 44 55 66 77 88 99", out, sizeof(out), &n));
  TEST_ASSERT_EQUAL_UINT8(10, n);  // total seen, so the caller can say "got 10"
  const uint8_t expected[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 8);  // only 8 stored, no overflow
}

// ---------------------------------------------------------------------------
// uidBcc / patchBlock0Uid
// ---------------------------------------------------------------------------

static void test_bcc_is_xor_of_uid_bytes() {
  const uint8_t uid[] = {0x79, 0xD6, 0x83, 0x63};
  TEST_ASSERT_EQUAL_HEX8(0x79 ^ 0xD6 ^ 0x83 ^ 0x63, uidBcc(uid));

  const uint8_t zeros[] = {0, 0, 0, 0};
  TEST_ASSERT_EQUAL_HEX8(0x00, uidBcc(zeros));

  const uint8_t same[] = {0xAA, 0xAA, 0xAA, 0xAA};
  TEST_ASSERT_EQUAL_HEX8(0x00, uidBcc(same));
}

static void test_patch_block0_keeps_manufacturer_bytes() {
  // A realistic block 0: old UID, its BCC, SAK, ATQA, manufacturer data.
  uint8_t block[16] = {0x11, 0x22, 0x33, 0x44, 0x44, 0x08, 0x04, 0x00,
                       0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69};
  const uint8_t uid[] = {0x79, 0xD6, 0x83, 0x63};

  patchBlock0Uid(block, uid);

  const uint8_t expected[16] = {0x79, 0xD6, 0x83, 0x63, 0x79 ^ 0xD6 ^ 0x83 ^ 0x63,
                                0x08, 0x04, 0x00, 0x62, 0x63, 0x64, 0x65,
                                0x66, 0x67, 0x68, 0x69};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, block, 16);
}

// ---------------------------------------------------------------------------
// formatUid
// ---------------------------------------------------------------------------

static void test_format_four_byte_uid() {
  const uint8_t uid[] = {0x79, 0xD6, 0x83, 0x63};
  char out[32];
  size_t len = formatUid(uid, 4, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("79 D6 83 63", out);
  TEST_ASSERT_EQUAL_size_t(11, len);
}

static void test_format_zero_pads_and_uppercases() {
  const uint8_t uid[] = {0x00, 0x0A, 0xFF};
  char out[32];
  formatUid(uid, 3, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("00 0A FF", out);
}

static void test_format_seven_byte_uid() {
  const uint8_t uid[] = {0x04, 0xA2, 0x3B, 0x1A, 0xC5, 0x5E, 0x80};
  char out[32];
  formatUid(uid, 7, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("04 A2 3B 1A C5 5E 80", out);
}

static void test_format_empty_uid() {
  char out[8] = "junk";
  TEST_ASSERT_EQUAL_size_t(0, formatUid(NULL, 0, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
}

static void test_format_truncates_at_byte_boundary() {
  const uint8_t uid[] = {0x79, 0xD6, 0x83, 0x63};
  char out[7];  // room for "79 D6" + NUL, not for the third byte
  size_t len = formatUid(uid, 4, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("79 D6", out);
  TEST_ASSERT_EQUAL_size_t(5, len);
}

// ---------------------------------------------------------------------------
// parseCommand
// ---------------------------------------------------------------------------

static void test_command_letters_both_cases() {
  const char *args;
  TEST_ASSERT_EQUAL(CMD_READ, parseCommand("R", &args));
  TEST_ASSERT_EQUAL(CMD_READ, parseCommand("r", &args));
  TEST_ASSERT_EQUAL(CMD_WRITE, parseCommand("W", &args));
  TEST_ASSERT_EQUAL(CMD_WRITE, parseCommand("w", &args));
  TEST_ASSERT_EQUAL(CMD_DIAG, parseCommand("D", &args));
  TEST_ASSERT_EQUAL(CMD_DIAG, parseCommand("d", &args));
  TEST_ASSERT_EQUAL(CMD_SET_UID, parseCommand("U", &args));
  TEST_ASSERT_EQUAL(CMD_SET_UID, parseCommand("u", &args));
}

static void test_command_skips_leading_whitespace() {
  const char *args;
  TEST_ASSERT_EQUAL(CMD_READ, parseCommand("   R", &args));
  TEST_ASSERT_EQUAL(CMD_READ, parseCommand("\t R", &args));
}

static void test_command_args_point_after_letter() {
  const char *args;
  TEST_ASSERT_EQUAL(CMD_SET_UID, parseCommand("U 79 D6 83 63", &args));
  TEST_ASSERT_EQUAL_STRING(" 79 D6 83 63", args);

  TEST_ASSERT_EQUAL(CMD_SET_UID, parseCommand("u79d68363", &args));
  TEST_ASSERT_EQUAL_STRING("79d68363", args);
}

static void test_command_blank_and_unknown() {
  const char *args;
  TEST_ASSERT_EQUAL(CMD_NONE, parseCommand("", &args));
  TEST_ASSERT_EQUAL_STRING("", args);
  TEST_ASSERT_EQUAL(CMD_NONE, parseCommand("   ", &args));

  TEST_ASSERT_EQUAL(CMD_UNKNOWN, parseCommand("X", &args));
  TEST_ASSERT_EQUAL(CMD_UNKNOWN, parseCommand("help", &args));
  TEST_ASSERT_EQUAL(CMD_UNKNOWN, parseCommand("?", &args));
}

// ---------------------------------------------------------------------------
// LineReader
// ---------------------------------------------------------------------------

// Feed a whole string; return how many complete lines came out.
static int feedAll(LineReader &r, const char *s) {
  int lines = 0;
  for (const char *p = s; *p; p++) {
    if (r.feed(*p)) lines++;
  }
  return lines;
}

static void test_line_completes_on_lf() {
  LineReader r;
  TEST_ASSERT_FALSE(r.feed('R'));
  TEST_ASSERT_TRUE(r.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("R", r.line());
}

static void test_line_crlf_yields_one_line() {
  LineReader r;
  TEST_ASSERT_EQUAL(1, feedAll(r, "W\r\n"));
  TEST_ASSERT_EQUAL_STRING("W", r.line());
}

static void test_line_cr_only_also_works() {
  LineReader r;
  TEST_ASSERT_EQUAL(1, feedAll(r, "D\r"));
  TEST_ASSERT_EQUAL_STRING("D", r.line());
}

static void test_line_empty_lines_are_ignored() {
  LineReader r;
  TEST_ASSERT_EQUAL(0, feedAll(r, "\n\r\n\r\r\n"));
  TEST_ASSERT_EQUAL(1, feedAll(r, "\n\nR\n\n"));
  TEST_ASSERT_EQUAL_STRING("R", r.line());
}

static void test_line_multiple_commands_in_a_row() {
  LineReader r;
  TEST_ASSERT_EQUAL(0, feedAll(r, "U 79 D6 83 63"));
  TEST_ASSERT_TRUE(r.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("U 79 D6 83 63", r.line());

  TEST_ASSERT_EQUAL(1, feedAll(r, "W\n"));
  TEST_ASSERT_EQUAL_STRING("W", r.line());
}

static void test_line_truncates_overlong_input() {
  LineReader r;
  // 60 'A's, which is more than the 39 usable bytes of the buffer.
  for (int i = 0; i < 60; i++) TEST_ASSERT_FALSE(r.feed('A'));
  TEST_ASSERT_TRUE(r.feed('\n'));

  TEST_ASSERT_EQUAL_size_t(LineReader::kCapacity - 1, strlen(r.line()));
  for (const char *p = r.line(); *p; p++) TEST_ASSERT_EQUAL_CHAR('A', *p);

  // The reader is still usable afterwards.
  TEST_ASSERT_EQUAL(1, feedAll(r, "R\n"));
  TEST_ASSERT_EQUAL_STRING("R", r.line());
}

// ---------------------------------------------------------------------------
// End-to-end over the pure layer: serial bytes -> command -> parsed UID
// ---------------------------------------------------------------------------

static void test_serial_bytes_to_uid() {
  LineReader r;
  TEST_ASSERT_EQUAL(1, feedAll(r, "  u 79:d6:83:63\r\n"));

  const char *args;
  TEST_ASSERT_EQUAL(CMD_SET_UID, parseCommand(r.line(), &args));

  uint8_t uid[8];
  uint8_t n = 0;
  TEST_ASSERT_EQUAL(HEX_OK, parseHexBytes(args, uid, sizeof(uid), &n));
  TEST_ASSERT_EQUAL_UINT8(4, n);

  char text[32];
  formatUid(uid, n, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("79 D6 83 63", text);
}

// ---------------------------------------------------------------------------

static int runAllTests() {
  UNITY_BEGIN();

  RUN_TEST(test_parse_space_separated);
  RUN_TEST(test_parse_colon_dash_and_no_separators);
  RUN_TEST(test_parse_is_case_insensitive);
  RUN_TEST(test_parse_empty_input);
  RUN_TEST(test_parse_odd_digit_count);
  RUN_TEST(test_parse_counts_beyond_buffer);

  RUN_TEST(test_bcc_is_xor_of_uid_bytes);
  RUN_TEST(test_patch_block0_keeps_manufacturer_bytes);

  RUN_TEST(test_format_four_byte_uid);
  RUN_TEST(test_format_zero_pads_and_uppercases);
  RUN_TEST(test_format_seven_byte_uid);
  RUN_TEST(test_format_empty_uid);
  RUN_TEST(test_format_truncates_at_byte_boundary);

  RUN_TEST(test_command_letters_both_cases);
  RUN_TEST(test_command_skips_leading_whitespace);
  RUN_TEST(test_command_args_point_after_letter);
  RUN_TEST(test_command_blank_and_unknown);

  RUN_TEST(test_line_completes_on_lf);
  RUN_TEST(test_line_crlf_yields_one_line);
  RUN_TEST(test_line_cr_only_also_works);
  RUN_TEST(test_line_empty_lines_are_ignored);
  RUN_TEST(test_line_multiple_commands_in_a_row);
  RUN_TEST(test_line_truncates_overlong_input);

  RUN_TEST(test_serial_bytes_to_uid);

  return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>

void setup() {
  delay(2000);  // give the serial monitor time to attach
  runAllTests();
}

void loop() {}
#else
int main() { return runAllTests(); }
#endif

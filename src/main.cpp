// rc522_utils
// ---------------------------------------------------------------------------
// Two-action helper for restoring your own fob:
//   1) read the UID from the old (broken) fob and remember it;
//   2) write that UID onto a blank magic card (gen1a/gen2).
//
// Controlled over the serial monitor at 115200. Commands: R (read), W (write).
//
// NOTE:
//  - RC522 works at 13.56 MHz -> MIFARE/NTAG only. Intercom fobs are often
//    125 kHz (EM-Marine) — this module can NOT read or write those.
//  - Rewriting the UID is only possible on a "magic" card whose block 0 is
//    open. A factory MIFARE card will not let you write block 0.
//  - The library's MIFARE_SetUid works with a 4-byte UID.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#include "uidcore.h"  // hex parsing, BCC, UID formatting, line/command parsing

#ifndef RC522_SS_PIN
#define RC522_SS_PIN 10
#endif
#ifndef RC522_RST_PIN
#define RC522_RST_PIN 9
#endif

MFRC522 mfrc522(RC522_SS_PIN, RC522_RST_PIN);

// UID of the source fob, kept in the microcontroller's memory
byte savedUid[10];
byte savedUidSize = 0;
bool haveUid = false;

enum Mode { MODE_IDLE, MODE_READ, MODE_WRITE, MODE_DIAG };
Mode mode = MODE_READ;

// Collects serial bytes until Enter; the finished line is then parsed.
LineReader lineReader;

void printUid(const byte *buf, byte size) {
  char text[3 * sizeof(savedUid)];  // "XX " per byte, last space becomes NUL
  formatUid(buf, size, text, sizeof(text));
  Serial.print(text);
}

void printMenu() {
  Serial.println();
  Serial.println(F("=== rc522_utils ==="));
  if (haveUid) {
    Serial.print(F("UID in memory: "));
    printUid(savedUid, savedUidSize);
    Serial.println();
  } else {
    Serial.println(F("No UID in memory."));
  }
  Serial.println(F("R + Enter       - read the UID from the old fob"));
  Serial.println(F("U <hex> + Enter - set the UID by hand, e.g. U 79 D6 83 63"));
  Serial.println(F("W + Enter       - write the stored UID onto a magic card"));
  Serial.println(F("D + Enter       - diagnose the card on the reader"));
  Serial.println();
}

// Parse 4 UID bytes from a text like "79 D6 83 63", "79:D6:83:63" or
// "79D68363". Separators are ignored; exactly 4 bytes are required.
void setUidFromText(const char *s) {
  byte tmp[8];
  byte n = 0;
  HexParseStatus st = parseHexBytes(s, tmp, sizeof(tmp), &n);

  if (st == HEX_ODD_DIGITS) {
    Serial.println(F("!! Odd number of hex digits. Use whole bytes."));
    return;
  }
  if (st == HEX_EMPTY) {
    Serial.println(F("!! Usage: U 79 D6 83 63  (4 hex bytes)"));
    return;
  }
  if (n != 4) {
    Serial.print(F("!! Need exactly 4 bytes, got "));
    Serial.println(n);
    return;
  }

  for (byte i = 0; i < 4; i++) savedUid[i] = tmp[i];
  savedUidSize = 4;
  haveUid = true;
  Serial.print(F("UID set by hand: "));
  printUid(savedUid, savedUidSize);
  Serial.println();
  printMenu();
}

// Dispatch one complete command line.
void processLine(const char *line) {
  const char *args;
  switch (parseCommand(line, &args)) {
    case CMD_READ:
      mode = MODE_READ;
      Serial.println(F(">> Read mode. Tap the old fob."));
      break;
    case CMD_SET_UID:
      mode = MODE_IDLE;  // stop card polling while we set the UID by hand
      setUidFromText(args);
      break;
    case CMD_WRITE:
      if (!haveUid) {
        Serial.println(F("!! No UID yet. Use R (read) or U (enter by hand) first."));
        return;
      }
      mode = MODE_WRITE;
      Serial.println(F(">> Write mode. Tap a blank magic card."));
      break;
    case CMD_DIAG:
      mode = MODE_DIAG;
      Serial.println(F(">> Diagnostics mode. Tap the card to test."));
      break;
    case CMD_NONE:
    case CMD_UNKNOWN:
      break;  // silently ignored, same as before
  }
}

// Collect serial bytes into a line; run the command on Enter.
void handleSerial() {
  while (Serial.available()) {
    if (lineReader.feed((char)Serial.read())) processLine(lineReader.line());
  }
}

void doRead() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  savedUidSize = mfrc522.uid.size;
  for (byte i = 0; i < savedUidSize; i++) savedUid[i] = mfrc522.uid.uidByte[i];
  haveUid = true;

  Serial.print(F("Read UID: "));
  printUid(savedUid, savedUidSize);
  Serial.print(F("  (type: "));
  Serial.print(mfrc522.PICC_GetTypeName(mfrc522.PICC_GetType(mfrc522.uid.sak)));
  Serial.println(F(")"));

  mfrc522.PICC_HaltA();

  mode = MODE_IDLE;
  printMenu();
}

// Re-select a card that is sitting on the reader after a HALT, using WUPA
// (0x52) instead of REQA (0x26) so a halted card answers again.
bool reselectCard() {
  byte atqa[2];
  byte atqaSize = sizeof(atqa);
  MFRC522::StatusCode s = mfrc522.PICC_WakeupA(atqa, &atqaSize);
  if (s != MFRC522::STATUS_OK && s != MFRC522::STATUS_COLLISION) return false;
  return mfrc522.PICC_Select(&mfrc522.uid, 0) == MFRC522::STATUS_OK;
}

// gen2 / CUID path: block 0 is directly writable after a normal auth.
// The card must be selected and active when this is called.
bool writeUidGen2() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;  // default key

  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, 0, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("   gen2 auth failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Read the current block 0 to keep its SAK/ATQA/manufacturer bytes.
  byte buf[18];
  byte size = sizeof(buf);
  status = mfrc522.MIFARE_Read(0, buf, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("   gen2 read block 0 failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Patch the UID and recompute the BCC check byte.
  patchBlock0Uid(buf, savedUid);

  status = mfrc522.MIFARE_Write(0, buf, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("   gen2 write block 0 failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

void doWrite() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  Serial.print(F("Target card, current UID: "));
  printUid(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();

  if (savedUidSize != 4) {
    Serial.print(F("!! Stored a "));
    Serial.print(savedUidSize);
    Serial.println(F("-byte UID, but only 4-byte UIDs are supported."));
    mode = MODE_IDLE;
    printMenu();
    return;
  }

  bool ok = false;

  // Attempt 1: gen2 / CUID. The card is freshly selected and active here.
  Serial.println(F(".. trying gen2 direct write"));
  ok = writeUidGen2();
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // Attempt 2: gen1a. Wake the halted card, then let the library's
  // MIFARE_SetUid open the UID backdoor and rewrite block 0.
  if (!ok) {
    Serial.println(F(".. trying gen1a backdoor"));
    if (reselectCard()) {
      ok = mfrc522.MIFARE_SetUid(savedUid, savedUidSize, true);
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  if (ok) {
    Serial.print(F("OK. UID written to the card: "));
    printUid(savedUid, savedUidSize);
    Serial.println();
  } else {
    Serial.println(F("!! Failed on both gen2 and gen1a."));
    Serial.println(F("   The card is likely a normal (non-magic) MIFARE,"));
    Serial.println(F("   or sector 0 uses a non-default key."));
  }

  mode = MODE_IDLE;
  printMenu();
}

// Non-destructive writability test: authenticate the block's sector with the
// default key, read the block, then write the very same bytes back. The data
// is unchanged; we only report whether the card ACKs the write.
bool blockWritable(byte blockAddr) {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) return false;

  byte buf[18];
  byte size = sizeof(buf);
  status = mfrc522.MIFARE_Read(blockAddr, buf, &size);
  if (status != MFRC522::STATUS_OK) return false;

  status = mfrc522.MIFARE_Write(blockAddr, buf, 16);  // same bytes back
  return status == MFRC522::STATUS_OK;
}

void doDiagnose() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  Serial.println();
  Serial.println(F("--- card diagnostics ---"));
  Serial.print(F("UID:  "));
  printUid(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
  Serial.print(F("Type: "));
  Serial.println(mfrc522.PICC_GetTypeName(mfrc522.PICC_GetType(mfrc522.uid.sak)));

  // 1. Is a normal data block (sector 0, block 1) writable with the default key?
  bool dataOk = blockWritable(1);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // 2. Does the card answer the gen1a UID backdoor? (non-destructive probe)
  bool gen1a = false;
  if (reselectCard()) gen1a = mfrc522.MIFARE_OpenUidBackdoor(false);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // 3. Is block 0 directly writable? (gen2 / CUID) - write identical bytes back.
  bool gen2 = false;
  if (reselectCard()) gen2 = blockWritable(0);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  Serial.print(F("default key, data block writable: "));
  Serial.println(dataOk ? F("YES") : F("no"));
  Serial.print(F("gen1a UID backdoor (0x40):        "));
  Serial.println(gen1a ? F("YES") : F("no"));
  Serial.print(F("block 0 directly writable (gen2): "));
  Serial.println(gen2 ? F("YES") : F("no"));

  Serial.print(F("=> "));
  if (gen1a) {
    Serial.println(F("magic gen1a: UID can be changed (gen1a path)."));
  } else if (gen2) {
    Serial.println(F("magic gen2/CUID: UID can be changed (gen2 path)."));
  } else if (dataOk) {
    Serial.println(F("normal MIFARE: data writable, UID is locked."));
  } else {
    Serial.println(F("no write access: wrong key or card not responding."));
  }

  mode = MODE_IDLE;
  printMenu();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  SPI.begin();
  mfrc522.PCD_Init();
  delay(50);

  Serial.println(F("RC522 ready."));
  printMenu();
  Serial.println(F(">> Read mode. Tap the old fob."));
  mode = MODE_READ;
}

void loop() {
  handleSerial();
  if (mode == MODE_READ) doRead();
  else if (mode == MODE_WRITE) doWrite();
  else if (mode == MODE_DIAG) doDiagnose();
  delay(50);
}

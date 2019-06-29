/*------------------------------------------------------------------------
  Demoonstrate different character sets on ESC POS printers
  Based on Adafruit Thermal printer library.
  ------------------------------------------------------------------------*/

#include "ESC_POS_Printer.h"
#include "USBPrinter.h"

class PrinterOper : public USBPrinterAsyncOper
{
  public:
    uint8_t OnInit(USBPrinter *pPrinter);
};

uint8_t PrinterOper::OnInit(USBPrinter *pPrinter)
{
  Serial.println(F("USB Printer OnInit"));
  Serial.print(F("Bidirectional="));
  Serial.println(pPrinter->isBidirectional());
  return 0;
}

USB myusb;
PrinterOper AsyncOper;
USBPrinter uprinter(&myusb, &AsyncOper);
ESC_POS_Printer printer(&uprinter);

// -----------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) delay(1);
  if (myusb.Init())
    Serial.println(F("USB host failed to initialize"));

  delay( 200 );
  Serial.println(F("USB Host init OK"));
  Serial.println(F("Character set demo"));
}

// Print charset map to printer
void dump() {
  uint8_t major, minor, c;

  printer.println(F("        01234567  89ABCDEF"));
  for(major=0; major<16; major++) {
    printer.print(F("     "));
    printer.print(major, HEX);
    printer.print(F("- "));
    for(minor=0; minor<16; minor++) {
      c = (major << 4) | minor;
      if(c < 32) c = ' '; // Skip control codes!
      printer.write(c);
      if(minor == 7) printer.print(F("  "));
    }
    printer.println();
  }
}

void print_character_set() {
  printer.underlineOn();
  printer.println(F("Character set demo\n"));
  printer.underlineOff();

  printer.println(F("Default"));
  printer.setCharset(0);
  printer.setCodePage(0);
  dump();

  printer.println(F("\nFrench char set with"));
  printer.println(F("Multilingual CP850 code page"));
  // Charset selection alters a few chars in ASCII 0x23-0x7E range.
  printer.setCharset(CHARSET_FRANCE);
  // Code page selects alt symbols for 'upper' ASCII 0x80-0xFF.
  printer.setCodePage(CODEPAGE_CP850);
  dump();

  printer.feed(2);
  printer.setDefault(); // Restore printer to defaults
}

void loop() {
  myusb.Task();

  // Make sure USB printer found and ready
  if (uprinter.isReady()) {
    printer.begin();
    Serial.println(F("Init ESC POS printer"));

    print_character_set();
    // Do this one time to avoid wasting paper
    while (1) delay(1);
  }
}

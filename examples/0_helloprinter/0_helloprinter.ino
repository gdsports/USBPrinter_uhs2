/*
 * printer driver with ESC POS library.
 * Print one line on printer.
 */

#include "USBPrinter.h"
#include "ESC_POS_Printer.h"

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

USB     myusb;
PrinterOper AsyncOper;
USBPrinter  uprinter(&myusb, &AsyncOper);
ESC_POS_Printer printer(&uprinter);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) delay(1);

  if (myusb.Init()) {
    Serial.println(F("USB host failed to initialize"));
    while (1) delay(1);
  }

  Serial.println(F("USB Host init OK"));
}

void loop() {
  myusb.Task();

  // Make sure USB printer found and ready
  if (uprinter.isReady()) {
    printer.begin();
    Serial.println(F("Init ESC POS printer"));

    printer.setSize('L');   // L for large
    printer.println(F("Hello printer"));
    // Do this one time to avoid wasting paper
    while (1) delay(1);
  }
}

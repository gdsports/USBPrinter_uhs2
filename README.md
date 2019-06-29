# USBPrinter driver for USB Host Shield 2.0 Library

This driver is designed to work for small USB thermal printers. Do not expect
this to work for laser or inkjet printers because they require complex software
support.

This has been tested with USB Host Shield Library 2.0 with one required patch
and the ESC POS Printer library on a Uno.

https://github.com/felis/USB_Host_Shield_2.0

Required patch

https://github.com/felis/USB_Host_Shield_2.0/pull/470

https://github.com/gdsports/ESC_POS_Printer

Sample program that prints "Hello Printer" in large letters.

```
#include "USBPrinter.h"			// https://github.com/gdsports/USBPrinter_uhs2
#include "ESC_POS_Printer.h"	// https://github.com/gdsports/ESC_POS_Printer

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
    printer.println(F("Hello Printer"));
    printer.feed(2);

    // Do this one time to avoid wasting paper
    while (1) delay(1);
  }
}
```

The ESC POS library send printer ESC/POS commands for different font sizes,
graphics mode, etc. It is based on the Adafruit Thermal printer library but
modified to work with USB thermal receipt printers. Note different thermal
printers from even if from the same manufacturers implement different subsets
of the ESC POS commands. You will have to find the ones that work on your
printer.

## Related

The following driver works on SAMD board with USB OTG ports such as Arduino Zero and MKR family.

https://github.com/gdsports/USBPrinter_uhls

The following driver works on Teensy 3.6 boards.

https://github.com/gdsports/USBPrinter_t36

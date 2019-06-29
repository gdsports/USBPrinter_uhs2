/*
   MIT License

   Copyright (c) 2019 gdsports625@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/
#include "USBPrinter.h"

const uint8_t USBPrinter::epDataInIndex = 1;
const uint8_t USBPrinter::epDataOutIndex = 2;

USBPrinter::USBPrinter(USB *p, USBPrinterAsyncOper *pasync) :
	pUsb(p),
	pAsync(pasync),
	bAddress(0),
	bIface(0),
	bNumEP(1),
	qNextPollTime(0),
	ready(false),
	bidirectional(false),
	readRemaining(0) {
		for(uint8_t i = 0; i < USBPRINTER_MAX_ENDPOINTS; i++) {
			epInfo[i].epAddr = 0;
			epInfo[i].maxPktSize = (i) ? 0 : 8;
			epInfo[i].bmSndToggle = 0;
			epInfo[i].bmRcvToggle = 0;
			epInfo[i].bmNakPower = (i == epDataInIndex) ? USB_NAK_NOWAIT : USB_NAK_NONAK;

		}
		if(pUsb)
			pUsb->RegisterDeviceClass(this);
	}

uint8_t USBPrinter::Init(uint8_t parent, uint8_t port, bool lowspeed) {

	const uint8_t constBufSize = sizeof (USB_DEVICE_DESCRIPTOR);

	uint8_t buf[constBufSize];
	USB_DEVICE_DESCRIPTOR * udd = reinterpret_cast<USB_DEVICE_DESCRIPTOR*>(buf);

	uint8_t rcode;
	UsbDevice *p = NULL;
	EpInfo *oldep_ptr = NULL;
	uint8_t num_of_conf; // number of configurations

	AddressPool &addrPool = pUsb->GetAddressPool();

	USBTRACE("USBPrinter Init\r\n");

	if(bAddress)
		return USB_ERROR_CLASS_INSTANCE_ALREADY_IN_USE;

	// Get pointer to pseudo device with address 0 assigned
	p = addrPool.GetUsbDevicePtr(0);

	if(!p)
		return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;

	if(!p->epinfo) {
		USBTRACE("epinfo\r\n");
		return USB_ERROR_EPINFO_IS_NULL;
	}

	// Save old pointer to EP_RECORD of address 0
	oldep_ptr = p->epinfo;

	// Temporary assign new pointer to epInfo to p->epinfo in order to avoid toggle inconsistence
	p->epinfo = epInfo;

	p->lowspeed = lowspeed;

	// Get device descriptor
	rcode = pUsb->getDevDescr(0, 0, constBufSize, (uint8_t*)buf);

	// Restore p->epinfo
	p->epinfo = oldep_ptr;

	if(rcode)
		goto FailGetDevDescr;

	// Allocate new address according to device class
	bAddress = addrPool.AllocAddress(parent, false, port);

	if(!bAddress)
		return USB_ERROR_OUT_OF_ADDRESS_SPACE_IN_POOL;

	// Extract Max Packet Size from the device descriptor
	epInfo[0].maxPktSize = udd->bMaxPacketSize0;

	// Assign new address to the device
	rcode = pUsb->setAddr(0, 0, bAddress);

	if(rcode) {
		p->lowspeed = false;
		addrPool.FreeAddress(bAddress);
		bAddress = 0;
		USBTRACE2("setAddr:", rcode);
		return rcode;
	}

	USBTRACE2("Addr:", bAddress);

	p->lowspeed = false;

	p = addrPool.GetUsbDevicePtr(bAddress);

	if(!p)
		return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;

	p->lowspeed = lowspeed;

	num_of_conf = udd->bNumConfigurations;

	// Assign epInfo to epinfo pointer
	rcode = pUsb->setEpInfoEntry(bAddress, 1, epInfo);

	if(rcode)
		goto FailSetDevTblEntry;

	USBTRACE2("NC:", num_of_conf);

	for(uint8_t i = 0; i < num_of_conf; i++) {
		ConfigDescParser< USB_CLASS_PRINTER,
			USBPRINTER_SUBCLASS_INTERFACE,
			USBPRINTER_PROTOCOL_BIDIRECTIONAL,
			CP_MASK_COMPARE_ALL > cdp_bidirectional(this);

		ConfigDescParser< USB_CLASS_PRINTER,
			USBPRINTER_SUBCLASS_INTERFACE,
			USBPRINTER_PROTOCOL_UNIDIRECTIONAL,
			CP_MASK_COMPARE_ALL > cdp_unidirectional(this);

		rcode = pUsb->getConfDescr(bAddress, 0, i, &cdp_bidirectional);

		if(rcode)
			goto FailGetConfDescr;

		if(bNumEP > 1) {
			bidirectional = true;
			break;
		}

		rcode = pUsb->getConfDescr(bAddress, 0, i, &cdp_unidirectional);

		if(rcode)
			goto FailGetConfDescr;

		if(bNumEP > 1) {
			bidirectional = false;
			break;
		}
	} // for

	USBTRACE2("bNumEP:", bNumEP);
	if (bidirectional) {
		if (bNumEP < USBPRINTER_MAX_ENDPOINTS)
			return USB_DEV_CONFIG_ERROR_DEVICE_NOT_SUPPORTED;
	}
	else {
		if (bNumEP < (USBPRINTER_MAX_ENDPOINTS-1))
			return USB_DEV_CONFIG_ERROR_DEVICE_NOT_SUPPORTED;
	}

	// Assign epInfo to epinfo pointer
	rcode = pUsb->setEpInfoEntry(bAddress, bNumEP, epInfo);

	USBTRACE2("Conf:", bConfNum);

	// Set Configuration Value
	rcode = pUsb->setConf(bAddress, 0, bConfNum);

	if(rcode)
		goto FailSetConfDescr;

	// Set up features status
	rcode = pAsync->OnInit(this);

	if(rcode)
		goto FailOnInit;

	USBTRACE("USBPrinter configured\r\n");

	ready = true;
	bidirectional = false;

	return 0;

FailGetDevDescr:
#ifdef DEBUG_USB_HOST
	NotifyFailGetDevDescr();
	goto Fail;
#endif

FailSetDevTblEntry:
#ifdef DEBUG_USB_HOST
	NotifyFailSetDevTblEntry();
	goto Fail;
#endif

FailGetConfDescr:
#ifdef DEBUG_USB_HOST
	NotifyFailGetConfDescr();
	goto Fail;
#endif

FailSetConfDescr:
#ifdef DEBUG_USB_HOST
	NotifyFailSetConfDescr();
	goto Fail;
#endif

FailOnInit:
#ifdef DEBUG_USB_HOST
	USBTRACE("OnInit:");
#endif

#ifdef DEBUG_USB_HOST
Fail:
	NotifyFail(rcode);
#endif
	Release();
	return rcode;
}

void USBPrinter::EndpointXtract(uint8_t conf, uint8_t iface, uint8_t alt __attribute__((unused)), uint8_t proto __attribute__((unused)), const USB_ENDPOINT_DESCRIPTOR *pep) {
	bConfNum = conf;
	bIface = iface;

	uint8_t index;

	if((pep->bmAttributes & bmUSB_TRANSFER_TYPE) == USB_TRANSFER_TYPE_BULK)
		index = ((pep->bEndpointAddress & 0x80) == 0x80) ? epDataInIndex : epDataOutIndex;
	else
		return;

	// Fill in the endpoint info structure
	epInfo[index].epAddr = (pep->bEndpointAddress & 0x0F);
	epInfo[index].maxPktSize = (uint8_t)pep->wMaxPacketSize;
	epInfo[index].bmSndToggle = 0;
	epInfo[index].bmRcvToggle = 0;

	bNumEP++;

	PrintEndpointDescriptor(pep);
}

uint8_t USBPrinter::Release() {
	ready = false;
	pUsb->GetAddressPool().FreeAddress(bAddress);

	bIface = 0;
	bNumEP = 1;

	bAddress = 0;
	qNextPollTime = 0;
	return 0;
}

uint8_t USBPrinter::Poll() {
	return 0;
}

uint8_t USBPrinter::RcvData(uint16_t *bytes_rcvd, uint8_t *dataptr) {
	uint8_t rv = pUsb->inTransfer(bAddress, epInfo[epDataInIndex].epAddr, bytes_rcvd, dataptr);
	if(rv && rv != hrNAK) {
		Release();
	}
	return rv;
}

uint8_t USBPrinter::SndData(uint16_t nbytes, uint8_t *dataptr) {
	uint8_t rv = pUsb->outTransfer(bAddress, epInfo[epDataOutIndex].epAddr, nbytes, dataptr);
	if(rv && rv != hrNAK) {
		Release();
	}
	return rv;
}

uint8_t USBPrinter::GetStatus() {
	uint8_t printer_status=0x10;
	if (!bidirectional) {
		uint8_t rv = pUsb->ctrlReq(bAddress, 0, USBPRINTER_GET, USBPRINTER_REQUEST_STATUS, 0x00, 0x00, bIface, 1, 1, &printer_status, NULL);
		if(rv && rv != hrNAK) {
			Release();
		}
	}
	return printer_status;
}

uint8_t USBPrinter::SoftReset() {
	uint8_t rv = pUsb->ctrlReq(bAddress, 0, USBPRINTER_PUT, USBPRINTER_REQUEST_SOFT_RESET, 0x00, 0x00, bIface, 0, 0, NULL, NULL);
	if(rv && rv != hrNAK) {
		Release();
	}
	return rv;
}

void USBPrinter::PrintEndpointDescriptor(const USB_ENDPOINT_DESCRIPTOR* ep_ptr) {
	Notify(PSTR("Endpoint descriptor:"), 0x80);
	Notify(PSTR("\r\nLength:\t\t"), 0x80);
	D_PrintHex<uint8_t > (ep_ptr->bLength, 0x80);
	Notify(PSTR("\r\nType:\t\t"), 0x80);
	D_PrintHex<uint8_t > (ep_ptr->bDescriptorType, 0x80);
	Notify(PSTR("\r\nAddress:\t"), 0x80);
	D_PrintHex<uint8_t > (ep_ptr->bEndpointAddress, 0x80);
	Notify(PSTR("\r\nAttributes:\t"), 0x80);
	D_PrintHex<uint8_t > (ep_ptr->bmAttributes, 0x80);
	Notify(PSTR("\r\nMaxPktSize:\t"), 0x80);
	D_PrintHex<uint16_t > (ep_ptr->wMaxPacketSize, 0x80);
	Notify(PSTR("\r\nPoll Intrv:\t"), 0x80);
	D_PrintHex<uint8_t > (ep_ptr->bInterval, 0x80);
	Notify(PSTR("\r\n"), 0x80);
}

int USBPrinter::available() {
	return readRemaining;
}

int USBPrinter::read() {
	int byte=-1;
	if (readRemaining == 0) {
		readRemaining = readBytes((char *)readBuffer, sizeof(readBuffer));
		readPointer = &readBuffer[0];
	}
	if (readRemaining > 0) {
		readRemaining--;
		byte = *readPointer++;
	}
	return byte;
}

int USBPrinter::peek() {
	int byte=-1;
	if (readRemaining == 0) {
		readRemaining = readBytes((char *)readBuffer, sizeof(readBuffer));
		readPointer = &readBuffer[0];
	}
	if (readRemaining > 0) {
		byte = *readPointer;
	}
	return byte;
}

size_t USBPrinter::readBytes(char *buffer, size_t length) {
	uint16_t rcvd = length;
	uint8_t rcode = RcvData(&rcvd, (uint8_t *)buffer);
	if (rcode && rcode != hrNAK) {
		ErrorMessage<uint8_t>(PSTR("Ret"), rcode);
		rcvd = 0;
	}
	return rcvd;
}

size_t USBPrinter::write(const uint8_t *buffer, size_t size) {
	uint8_t rcode = SndData(size, (uint8_t *)buffer);
	if (rcode) {
		ErrorMessage<uint8_t>(PSTR("SndData"), rcode);
		return 0;
	}
	return size;
}

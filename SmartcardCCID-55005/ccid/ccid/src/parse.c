/*
    parse.c: parse CCID structure
    Copyright (C) 2003-2004   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc., 51
	Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/*
 * $Id: parse.c 3408 2009-04-02 07:27:13Z rousseau $
 */

#include <stdio.h>
#include <string.h>
# ifdef S_SPLINT_S
# include <sys/types.h>
# endif
#include <usb.h>
#include <errno.h>

#include "defs.h"
#include "ccid.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define BLUE "\33[34m"
#define RED "\33[31m"
#define BRIGHT_RED "\33[01;31m"
#define GREEN "\33[32m"
#define MAGENTA "\33[35m"
#define NORMAL "\33[0m"

static int ccid_parse_interface_descriptor(usb_dev_handle *handle,
	struct usb_device *dev, int num);


/*****************************************************************************
 *
 *					main
 *
 ****************************************************************************/
int main(int argc, char *argv[])
{
	static struct usb_bus *busses = NULL;
	struct usb_bus *bus;
	struct usb_dev_handle *dev_handle;
	int nb = 0;
	char buffer[256];
	char class_ff = FALSE;

	if ((argc > 1) && (0 == strcmp(argv[1], "-p")))
		class_ff = TRUE;

	usb_init();
	(void)usb_find_busses();
	(void)usb_find_devices();

	busses = usb_get_busses();
	if (busses == NULL)
	{
		(void)printf("No USB buses found\n");
		return -1;
	}

	/* on any USB buses */
	for (bus = busses; bus; bus = bus->next)
	{
		struct usb_device *dev;

		/* any device on this bus */
		for (dev = bus->devices; dev; dev = dev->next)
		{
			struct usb_interface *usb_interface = NULL;
			int interface;
			int num = 0;

			dev_handle = usb_open(dev);
			if (NULL == dev_handle)
			{
				(void)fprintf(stderr, "Can't usb_open(%s/%s): %s\n",
					bus->dirname, dev->filename, strerror(errno));
				if (getuid())
				{
					(void)fprintf(stderr, BRIGHT_RED "Please, restart the command as root\n" NORMAL);
					return 1;
				}
				continue;
			}

			(void)fprintf(stderr, "Parsing USB bus/device: %s/%s\n",
				bus->dirname, dev->filename);

			(void)fprintf(stderr, " idVendor:  0x%04X", dev->descriptor.idVendor);
			if (usb_get_string_simple(dev_handle, dev->descriptor.iManufacturer,
				buffer, sizeof(buffer)) < 0)
			{
				(void)fprintf(stderr, "  Can't get iManufacturer string\n");
				if (getuid())
				{
					(void)fprintf(stderr, BRIGHT_RED "Please, restart the command as root\n" NORMAL);
					return 1;
				}
			}
			else
				(void)fprintf(stderr, "  iManufacturer: " BLUE "%s\n" NORMAL, buffer);

			(void)fprintf(stderr, " idProduct: 0x%04X", dev->descriptor.idProduct);
			if (usb_get_string_simple(dev_handle, dev->descriptor.iProduct,
				buffer, sizeof(buffer)) < 0)
				(void)fprintf(stderr, "  Can't get iProduct string\n");
			else
				(void)fprintf(stderr, "  iProduct: " BLUE "%s\n" NORMAL, buffer);

again:
			/* check if the device has bInterfaceClass == 11 */
			usb_interface = get_ccid_usb_interface(dev, &num);
			if (NULL == usb_interface)
			{
				(void)usb_close(dev_handle);
				/* only if we found no CCID interface */
				if (0 == num)
					(void)fprintf(stderr, RED "  NOT a CCID/ICCD device\n" NORMAL);
				continue;
			}
			if (!class_ff && (0xFF == usb_interface->altsetting->bInterfaceClass))
			{
				(void)fprintf(stderr, MAGENTA "  Found a possibly CCID/ICCD device (bInterfaceClass = 0xFF). Use -p\n" NORMAL);
				continue;
			}
			(void)fprintf(stderr, GREEN "  Found a CCID/ICCD device at interface %d\n" NORMAL, num);

			/* now we found a free reader and we try to use it */
			if (NULL == dev->config)
			{
				(void)usb_close(dev_handle);
				(void)fprintf(stderr, "No dev->config found for %s/%s\n",
					 bus->dirname, dev->filename);
				continue;
			}

			interface = usb_interface->altsetting->bInterfaceNumber;
#ifndef __APPLE__
			if (usb_claim_interface(dev_handle, interface) < 0)
			{
				(void)usb_close(dev_handle);
				(void)fprintf(stderr, "Can't claim interface %s/%s: %s\n",
						bus->dirname, dev->filename, strerror(errno));
				if (EBUSY == errno)
				{
					(void)fprintf(stderr,
						BRIGHT_RED " Please, stop pcscd and retry\n\n" NORMAL);
					return TRUE;
				}
				continue;
			}
#endif

			(void)ccid_parse_interface_descriptor(dev_handle, dev, num);

#ifndef __APPLE__
			(void)usb_release_interface(dev_handle, interface);
#endif
			/* check for another CCID interface on the same device */
			num++;
			goto again;

			(void)usb_close(dev_handle);
			nb++;
		}
	}

	if ((0 == nb) && (0 != geteuid()))
		(void)fprintf(stderr, "Can't find any CCID device.\nMaybe you must run parse as root?\n");
	return 0;
} /* main */


/*****************************************************************************
 *
 *					Parse a CCID USB Descriptor
 *
 ****************************************************************************/
static int ccid_parse_interface_descriptor(usb_dev_handle *handle,
	struct usb_device *dev, int num)
{
	struct usb_interface_descriptor *usb_interface;
	unsigned char *extra;
	char buffer[256*sizeof(int)];  /* maximum is 256 records */
	/* unsigned version of buffer[] used for multi-bytes conversions */
	unsigned char *ubuffer = (unsigned char *)buffer;

	/*
	 * Vendor/model name
	 */
	(void)printf(" idVendor: 0x%04X\n", dev->descriptor.idVendor);
	if (usb_get_string_simple(handle, dev->descriptor.iManufacturer,
		buffer, sizeof(buffer)) < 0)
	{
		(void)printf("  Can't get iManufacturer string\n");
		if (getuid())
		{
			(void)fprintf(stderr,
				BRIGHT_RED "Please, restart the command as root\n\n" NORMAL);
			return TRUE;
		}
	}
	else
		(void)printf("  iManufacturer: %s\n", buffer);

	(void)printf(" idProduct: 0x%04X\n", dev->descriptor.idProduct);
	if (usb_get_string_simple(handle, dev->descriptor.iProduct,
		buffer, sizeof(buffer)) < 0)
		(void)printf("  Can't get iProduct string\n");
	else
		(void)printf("  iProduct: %s\n", buffer);

	(void)printf(" bcdDevice: %X.%02X (firmware release?)\n",
		dev->descriptor.bcdDevice >> 8, dev->descriptor.bcdDevice & 0xFF);

	usb_interface = get_ccid_usb_interface(dev, &num)->altsetting;

	(void)printf(" bLength: %d\n", usb_interface->bLength);

	(void)printf(" bDescriptorType: %d\n", usb_interface->bDescriptorType);

	(void)printf(" bInterfaceNumber: %d\n", usb_interface->bInterfaceNumber);

	(void)printf(" bAlternateSetting: %d\n", usb_interface->bAlternateSetting);

	(void)printf(" bNumEndpoints: %d\n", usb_interface->bNumEndpoints);
	switch (usb_interface->bNumEndpoints)
	{
		case 0:
			(void)printf("  Control only\n");
			break;
		case 1:
			(void)printf("  Interrupt-IN\n");
			break;
		case 2:
			(void)printf("  bulk-IN and bulk-OUT\n");
			break;
		case 3:
			(void)printf("  bulk-IN, bulk-OUT and Interrupt-IN\n");
			break;
		default:
			(void)printf("  UNKNOWN value\n");
	}

	(void)printf(" bInterfaceClass: 0x%02X", usb_interface->bInterfaceClass);
	if (usb_interface->bInterfaceClass == 0x0b)
		(void)printf(" [Chip Card Interface Device Class (CCID)]\n");
	else
	{
		(void)printf("\n  NOT A CCID DEVICE\n");
		if (usb_interface->bInterfaceClass != 0xFF)
			return TRUE;
		else
			(void)printf("  Class is 0xFF (proprietary)\n");
	}

	(void)printf(" bInterfaceSubClass: %d\n", usb_interface->bInterfaceSubClass);
	if (usb_interface->bInterfaceSubClass)
		(void)printf("  UNSUPPORTED SubClass\n");

	(void)printf(" bInterfaceProtocol: %d\n", usb_interface->bInterfaceProtocol);
	switch (usb_interface->bInterfaceProtocol)
	{
		case 0:
			(void)printf("  bulk transfer, optional interrupt-IN (CCID)\n");
			break;
		case 1:
			(void)printf("  ICCD Version A, Control transfers, (no interrupt-IN)\n");
			break;
		case 2:
			(void)printf("  ICCD Version B, Control transfers, (optional interrupt-IN)\n");
			break;
		default:
			(void)printf("  UNSUPPORTED InterfaceProtocol\n");
	}

	if (usb_get_string_simple(handle, usb_interface->iInterface,
		buffer, sizeof(buffer)) < 0)
		(void)printf(" Can't get iInterface string\n");
	else
		(void)printf(" iInterface: %s\n", buffer);

	if (usb_interface->extralen < 54)
	{
		(void)printf("USB extra length is too short: %d\n", usb_interface->extralen);
		(void)printf("\n  NOT A CCID DEVICE\n");
		return TRUE;
	}

	/*
	 * CCID Class Descriptor
	 */
	(void)printf(" CCID Class Descriptor\n");
	extra = usb_interface->extra;

	(void)printf("  bLength: 0x%02X\n", extra[0]);
	if (extra[0] != 0x36)
	{
		(void)printf("   UNSUPPORTED bLength\n");
		return TRUE;
	}

	(void)printf("  bDescriptorType: 0x%02X\n", extra[1]);
	if (extra[1] != 0x21)
	{
		if (0xFF == extra[1])
			(void)printf("   PROPRIETARY bDescriptorType\n");
		else
		{
			(void)printf("   UNSUPPORTED bDescriptorType\n");
			return TRUE;
		}
	}

	(void)printf("  bcdCCID: %X.%02X\n", extra[3], extra[2]);
	(void)printf("  bMaxSlotIndex: 0x%02X\n", extra[4]);
	(void)printf("  bVoltageSupport: 0x%02X\n", extra[5]);
	if (extra[5] & 0x01)
		(void)printf("   5.0V\n");
	if (extra[5] & 0x02)
		(void)printf("   3.0V\n");
	if (extra[5] & 0x04)
		(void)printf("   1.8V\n");

	(void)printf("  dwProtocols: 0x%02X%02X 0x%02X%02X\n", extra[9], extra[8],
		extra[7], extra[6]);
	if (extra[6] & 0x01)
		(void)printf("   T=0\n");
	if (extra[6] & 0x02)
		(void)printf("   T=1\n");

	(void)printf("  dwDefaultClock: %.3f MHz\n", dw2i(extra, 10)/1000.0);
	(void)printf("  dwMaximumClock: %.3f MHz\n", dw2i(extra, 14)/1000.0);
	(void)printf("  bNumClockSupported: %d %s\n", extra[18],
		extra[18] ? "" : "(will use whatever is returned)");
	{
		int n;

		/* See CCID 3.7.2 page 25 */
		n = usb_control_msg(handle,
			0xA1, /* request type */
			0x02, /* GET CLOCK FREQUENCIES */
			0x00, /* value */
			usb_interface->bInterfaceNumber, /* interface */
			buffer,
			sizeof(buffer),
			2 * 1000);

		/* we got an error? */
		if (n <= 0)
		{
			(void)(void)printf("   IFD does not support GET CLOCK FREQUENCIES request: %s\n", strerror(errno));
			if (EBUSY == errno)
			{
				(void)fprintf(stderr,
					BRIGHT_RED "   Please, stop pcscd and retry\n\n" NORMAL);
				return TRUE;
			}
		}
		else
			if (n % 4) 	/* not a multiple of 4 */
				(void)printf("   wrong size for GET CLOCK FREQUENCIES: %d\n", n);
			else
			{
				int i;

				/* we do not get the expected number of data rates */
				if ((n != extra[18]*4) && extra[18])
				{
					(void)printf("   Got %d clock frequencies but was expecting %d\n",
						n/4, extra[18]);

					/* we got more data than expected */
					if (n > extra[18]*4)
						n = extra[18]*4;
				}

				for (i=0; i<n; i+=4)
					(void)printf("   Support %d kHz\n", dw2i(ubuffer, i));
			}
	}
	(void)printf("  dwDataRate: %d bps\n", dw2i(extra, 19));
	(void)printf("  dwMaxDataRate: %d bps\n", dw2i(extra, 23));
	(void)printf("  bNumDataRatesSupported: %d %s\n", extra[27],
		extra[27] ? "" : "(will use whatever is returned)");
	{
		int n;

		/* See CCID 3.7.3 page 25 */
		n = usb_control_msg(handle,
			0xA1, /* request type */
			0x03, /* GET DATA RATES */
			0x00, /* value */
			usb_interface->bInterfaceNumber, /* interface */
			buffer,
			sizeof(buffer),
			2 * 1000);

		/* we got an error? */
		if (n <= 0)
			(void)printf("   IFD does not support GET_DATA_RATES request: %s\n",
				strerror(errno));
		else
			if (n % 4) 	/* not a multiple of 4 */
				(void)printf("   wrong size for GET_DATA_RATES: %d\n", n);
			else
			{
				int i;

				/* we do not get the expected number of data rates */
				if ((n != extra[27]*4) && extra[27])
				{
					(void)printf("   Got %d data rates but was expecting %d\n", n/4,
						extra[27]);

					/* we got more data than expected */
					if (n > extra[27]*4)
						n = extra[27]*4;
				}

				for (i=0; i<n; i+=4)
					(void)printf("   Support %d bps\n", dw2i(ubuffer, i));
			}
	}
	(void)printf("  dwMaxIFSD: %d\n", dw2i(extra, 28));
	(void)printf("  dwSynchProtocols: 0x%08X\n", dw2i(extra, 32));
	if (extra[32] & 0x01)
			(void)printf("   2-wire protocol\n");
	if (extra[32] & 0x02)
			(void)printf("   3-wire protocol\n");
	if (extra[32] & 0x04)
			(void)printf("   I2C protocol\n");

	(void)printf("  dwMechanical: 0x%08X\n", dw2i(extra, 36));
	if (extra[36] == 0)
		(void)printf("   No special characteristics\n");
	if (extra[36] & 0x01)
		(void)printf("   Card accept mechanism\n");
	if (extra[36] & 0x02)
		(void)printf("   Card ejection mechanism\n");
	if (extra[36] & 0x04)
		(void)printf("   Card capture mechanism\n");
	if (extra[36] & 0x08)
		(void)printf("   Card lock/unlock mechanism\n");

	(void)printf("  dwFeatures: 0x%08X\n", dw2i(extra, 40));
	if (dw2i(extra, 40) == 0)
		(void)printf("   No special characteristics\n");
	if (extra[40] & 0x02)
		(void)printf("   ....02 Automatic parameter configuration based on ATR data\n");
	if (extra[40] & 0x04)
		(void)printf("   ....04 Automatic activation of ICC on inserting\n");
	if (extra[40] & 0x08)
		(void)printf("   ....08 Automatic ICC voltage selection\n");
	if (extra[40] & 0x10)
		(void)printf("   ....10 Automatic ICC clock frequency change according to parameters\n");
	if (extra[40] & 0x20)
		(void)printf("   ....20 Automatic baud rate change according to frequency and Fi, Di params\n");
	if (extra[40] & 0x40)
		(void)printf("   ....40 Automatic parameters negotiation made by the CCID\n");
	if (extra[40] & 0x80)
		(void)printf("   ....80 Automatic PPS made by the CCID\n");
	if (extra[41] & 0x01)
		(void)printf("   ..01.. CCID can set ICC in clock stop mode\n");
	if (extra[41] & 0x02)
		(void)printf("   ..02.. NAD value other than 00 accepted (T=1)\n");
	if (extra[41] & 0x04)
		(void)printf("   ..04.. Automatic IFSD exchange as first exchange (T=1)\n");
	if (extra[41] & 0x08)
		(void)printf("   ..08.. Unknown (ICCD?)\n");
	switch (extra[42] & 0x07)
	{
		case 0x00:
			(void)printf("   00.... Character level exchange\n");
			break;

		case 0x01:
			(void)printf("   01.... TPDU level exchange\n");
			break;

		case 0x02:
			(void)printf("   02.... Short APDU level exchange\n");
			break;

		case 0x04:
			(void)printf("   04.... Short and Extended APDU level exchange\n");
			break;
	}

	(void)printf("  dwMaxCCIDMessageLength: %d bytes\n", dw2i(extra, 44));
	(void)printf("  bClassGetResponse: 0x%02X\n", extra[48]);
	if (0xFF == extra[48])
		(void)printf("   echoes the APDU class\n");
	(void)printf("  bClassEnveloppe: 0x%02X\n", extra[49]);
	if (0xFF == extra[49])
		(void)printf("   echoes the APDU class\n");
	(void)printf("  wLcdLayout: 0x%04X\n", (extra[51] << 8)+extra[50]);
	if (extra[50])
		(void)printf("   %d lines\n", extra[50]);
	if (extra[51])
		(void)printf("   %d characters per line\n", extra[51]);
	(void)printf("  bPINSupport: 0x%02X\n", extra[52]);
	if (extra[52] & 0x01)
		(void)printf("   PIN Verification supported\n");
	if (extra[52] & 0x02)
		(void)printf("   PIN Modification supported\n");
	(void)printf("  bMaxCCIDBusySlots: %d\n", extra[53]);

	return FALSE;
} /* ccid_parse_interface_descriptor */


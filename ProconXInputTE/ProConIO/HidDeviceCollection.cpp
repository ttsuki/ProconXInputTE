#include "HidDeviceCollection.h"

namespace ProconXInputTE
{
	HidDeviceCollection::HidDeviceCollection(unsigned short vendorId, unsigned short productId)
		: devs_(hid_enumerate(vendorId, productId))
	{
	}

	HidDeviceCollection::~HidDeviceCollection()
	{
		hid_free_enumeration(devs_);
	}

	HidDeviceCollection HidDeviceCollection::EnumerateDevices(unsigned short vendorId, unsigned short productId)
	{
		return {vendorId, productId};
	}
}

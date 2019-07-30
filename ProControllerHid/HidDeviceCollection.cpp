#include "HidDeviceCollection.h"

#include <hidapi.h>

namespace ProControllerHid
{
	const hid_device_info& HidDeviceCollection::const_iterator::operator*() const
	{
		return *ptr_;
	}

	const hid_device_info* HidDeviceCollection::const_iterator::operator->() const
	{
		return ptr_;
	}

	HidDeviceCollection::const_iterator& HidDeviceCollection::const_iterator::operator++()
	{
		return ptr_ = ptr_->next, *this;
	}

	HidDeviceCollection::const_iterator HidDeviceCollection::const_iterator::operator++(int)
	{
		auto i = *this;
		++* this;
		return i;
	}

	bool HidDeviceCollection::const_iterator::operator==(const const_iterator& rhs) const
	{
		return ptr_ == rhs.ptr_;
	}

	bool HidDeviceCollection::const_iterator::operator!=(const const_iterator& rhs) const
	{
		return !(*this == rhs);
	}

	HidDeviceCollection::HidDeviceCollection(unsigned short vendorId, unsigned short productId)
		: devs_(hid_enumerate(vendorId, productId))
	{
	}

	HidDeviceCollection::~HidDeviceCollection()
	{
		hid_free_enumeration(devs_);
	}

	HidDeviceCollection::HidDeviceCollection(HidDeviceCollection&& other) noexcept
		: devs_(other.devs_)
	{
		other.devs_ = nullptr;
	}

	HidDeviceCollection& HidDeviceCollection::operator=(HidDeviceCollection&& other) noexcept
	{
		if (this == &other) { return *this; }
		devs_ = other.devs_;
		other.devs_ = nullptr;
		return *this;
	}

	HidDeviceCollection::const_iterator HidDeviceCollection::begin() const
	{
		return const_iterator{devs_};
	}

	HidDeviceCollection::const_iterator HidDeviceCollection::end() const
	{
		return const_iterator{nullptr};
	}

	HidDeviceCollection HidDeviceCollection::EnumerateDevices(unsigned short vendorId, unsigned short productId)
	{
		return HidDeviceCollection{ vendorId, productId };
	}
}

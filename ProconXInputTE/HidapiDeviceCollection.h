#pragma once

#include <hidapi.h>
#include "IUncopyable.h"

namespace ProconXInputTE
{
	class HidapiDeviceCollection : IUncopyable
	{
		struct const_iterator
		{
			const hid_device_info* ptr_{};
			const hid_device_info& operator *() const { return *ptr_; }
			const hid_device_info* operator ->() const { return ptr_; }
			const_iterator& operator ++() { return ptr_ = ptr_->next, *this; }
			const_iterator operator ++(int) { auto i = *this; ++* this; return i; }
			bool operator ==(const const_iterator& rhs) const { return ptr_ == rhs.ptr_; }
			bool operator !=(const const_iterator& rhs) const { return !(*this == rhs); }
		};
		hid_device_info* const devs_{};
	public:
		HidapiDeviceCollection(unsigned short vendorId, unsigned short productId)
			: devs_{ hid_enumerate(vendorId, productId) }
		{
		}

		~HidapiDeviceCollection() { hid_free_enumeration(devs_); }
		const_iterator begin() const { return const_iterator{ devs_ }; }
		const_iterator end() const { return const_iterator{ nullptr }; }

		static HidapiDeviceCollection EnumerateDevices(unsigned short vendorId, unsigned short productId)
		{
			return { vendorId, productId };
		}
	};
}

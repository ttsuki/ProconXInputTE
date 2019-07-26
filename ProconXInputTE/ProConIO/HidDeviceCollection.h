#pragma once

#include <hidapi.h>

namespace ProconXInputTE
{
	class HidDeviceCollection
	{
		hid_device_info* const devs_{};
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

		HidDeviceCollection(unsigned short vendorId, unsigned short productId);
	public:
		~HidDeviceCollection();
		HidDeviceCollection(const HidDeviceCollection& other) = delete;
		HidDeviceCollection& operator=(const HidDeviceCollection& other) = delete;

		const_iterator begin() const { return const_iterator{ devs_ }; }
		const_iterator end() const { return const_iterator{ nullptr }; }

		static HidDeviceCollection EnumerateDevices(unsigned short vendorId, unsigned short productId);
	};
}

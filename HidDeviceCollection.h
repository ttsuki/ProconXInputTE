#pragma once

struct hid_device_info;

namespace ProControllerHid
{
	class HidDeviceCollection
	{
		struct const_iterator
		{
			const hid_device_info* ptr_{};
			const hid_device_info& operator *() const;
			const hid_device_info* operator ->() const;
			const_iterator& operator ++();
			const_iterator operator ++(int);
			bool operator ==(const const_iterator& rhs) const;
			bool operator !=(const const_iterator& rhs) const;
		};

		HidDeviceCollection(unsigned short vendorId, unsigned short productId);
		hid_device_info* devs_{};

	public:
		~HidDeviceCollection();
		HidDeviceCollection(const HidDeviceCollection& other) = delete;
		HidDeviceCollection(HidDeviceCollection&& other) noexcept;
		HidDeviceCollection& operator=(const HidDeviceCollection& other) = delete;
		HidDeviceCollection& operator=(HidDeviceCollection&& other) noexcept;
		const_iterator begin() const;
		const_iterator end() const;

		static HidDeviceCollection EnumerateDevices(unsigned short vendorId, unsigned short productId);
	};
}

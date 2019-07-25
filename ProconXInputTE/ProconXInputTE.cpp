
#include <iostream>

#include "HidapiDeviceCollection.h"

namespace ProconXInputTE
{
	constexpr unsigned short kNintendoVID{ 0x057E };
	constexpr unsigned short kProControllerPID{ 0x2009 };

	void Run()
	{
		for (auto device : HidapiDeviceCollection::EnumerateDevices(kNintendoVID, kProControllerPID))
		{
			std::cout << "Device found:" << std::endl;
			std::cout << "  Path: " << device.path << std::endl;
			std::wcout << L"  Manufacture: " << device.manufacturer_string << std::endl;
			std::wcout << L"  Product: " << device.product_string << std::endl;
		}
	}
}

int main()
{
	ProconXInputTE::Run();
}

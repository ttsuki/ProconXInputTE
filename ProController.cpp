#include "ProController.h"

#include <algorithm>
#include <thread>
#include <chrono>
#include <shared_mutex>
#include <map>

#include "HidDevice.h"
#include "SysDep.h"

namespace ProControllerHid
{
	using Buffer = HidIo::Buffer;

	static uint64_t tick()
	{
		using namespace std::chrono;
		return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}

	static void DumpPacket(const char *msg, const Buffer &data, int start = 0, int count = -1, uint64_t clock = tick())
	{
#ifdef LOG_PACKET
		static std::mutex m;
		std::lock_guard<decltype(m)> lock(m);
		if (count == -1) { count = data.size() - start; }
		printf("\x1b[2K%lld: %9s", clock, msg);
		for (int i = start; i < start + count; i++) { printf(" %02X", data[i]); }
		printf("\n");
#endif
	}

	class ProControllerImpl final : public ProController
	{
		using PacketProc = std::function<void(const Buffer &)>;
		using Clock = std::chrono::steady_clock;

		HidIo::HidDeviceThreaded device_{};
		bool imuSensorsEnabled_{};

		uint8_t nextPacketNumber_{};
		Buffer rumbleStatus_{};
		uint8_t playerLedStatus_{};

		std::thread controllerUpdaterThread_{};
		std::atomic_flag controllerUpdaterThreadRunning_{ATOMIC_FLAG_INIT};

		InputStatusCallback statusCallback_{};
		bool statusCallbackEnabled_{};
		std::mutex statusCallbackCalling_{};

		class PendingCommandMap
		{
			std::shared_mutex mutex_{};
			std::condition_variable_any signal_{};
			std::map<uint8_t, std::pair<Clock::time_point, PacketProc>> pending_{};
		public:
			void Register(uint8_t command, PacketProc onReply = nullptr, Clock::duration timeout = std::chrono::milliseconds(60));
			void Signal(uint8_t command, const Buffer &replyPacket);
			bool Pending(uint8_t command);
			bool Wait(uint8_t command);
		} usbCommandQueue_{}, subCommandQueue_{};


		struct SpiCalibrationParameters
		{
			struct SensorCalibration
			{
				struct Axis
				{
					int16_t Origin;
					uint16_t Sensitivity;
					int16_t HorizontalOffset;
				} X, Y, Z;
			} Accelerometer, Gyroscope;

			struct StickCalibration
			{
				struct Range
				{
					int16_t MinValue, CenterValue, MaxValue;
				} X, Y;

				int16_t DeadZone, RangeRatio;
			} LeftStick, RightStick;
		} calibrationParameters_{};

	public:
		ProControllerImpl(const char *pathToDevice, int index, InputStatusCallback statusCallback);
		~ProControllerImpl() override;

		void StartStatusCallback() override;
		void StopStatusCallback() override;
		CorrectedInputStatus CorrectInput(const InputStatus &raw) override;

		void SetRumbleBasic(
			uint8_t leftLowAmp, uint8_t rightLowAmp, uint8_t leftHighAmp, uint8_t rightHighAmp,
			uint8_t leftLowFreq, uint8_t rightLowFreq, uint8_t leftHighFreq, uint8_t rightHighFreq) override;

		void SetPlayerLed(uint8_t playerLed) override;

	private:
		SpiCalibrationParameters LoadCalibrationParametersFromSpiMemory();
		void SendUsbCommand(uint8_t usbCommand, const Buffer &data, bool waitAck);
		void SendSubCommand(uint8_t subCommand, const Buffer &data, bool waitAck, const PacketProc &callback = nullptr);
		Buffer ReadSpiMemory(uint16_t address, uint8_t length);
		void SendRumble();

		void OnPacket(const Buffer &data);
		void OnStatus(const Buffer &data);
	};

	void ProControllerImpl::PendingCommandMap::Register(uint8_t command, PacketProc onReply, Clock::duration timeout)
	{
		std::lock_guard<decltype(mutex_)> lock(mutex_);
		pending_[command] = {Clock::now() + timeout, std::move(onReply)};
	}

	void ProControllerImpl::PendingCommandMap::Signal(uint8_t command, const Buffer &replyPacket)
	{
		std::lock_guard<decltype(mutex_)> lock(mutex_);
		auto it = pending_.find(command);
		if (it != pending_.end())
		{
			const PacketProc callback = std::move(it->second.second);
			pending_.erase(command);
			if (callback) { callback(replyPacket); }
		}
		signal_.notify_all();
	}

	bool ProControllerImpl::PendingCommandMap::Pending(uint8_t command)
	{
		std::shared_lock<decltype(mutex_)> lock(mutex_);
		auto it = pending_.find(command);
		return it != pending_.end() && it->second.first < Clock::now();
	}

	bool ProControllerImpl::PendingCommandMap::Wait(uint8_t command)
	{
		std::shared_lock<decltype(mutex_)> lock(mutex_);
		auto it = pending_.find(command);
		if (it == pending_.end())
		{
			return true;
		}

		return signal_.wait_until(lock, it->second.first,
				[this, command] { return pending_.count(command) == 0; })
			|| pending_.count(command) == 0;
	}

	ProControllerImpl::ProControllerImpl(const char *pathToDevice, int index, InputStatusCallback statusCallback)
	{
		imuSensorsEnabled_ = true;

		SetRumbleBasic(0, 0, 0, 0, 0x80, 0x80, 0x80, 0x80);
		SetPlayerLed((1 << index) - 1);

		bool openResult = device_.OpenDevice(pathToDevice, [this](const Buffer &data) { OnPacket(data); });
		if (!openResult)
		{
			return;
		}

		SendUsbCommand(0x02, {}, true); // Handshake
		SendUsbCommand(0x03, {}, true); // Set baudrate to 3Mbps
		SendUsbCommand(0x02, {}, true); // Handshake
		SendUsbCommand(0x04, {}, false); // HID only-mode(No use Bluetooth)

		calibrationParameters_ = LoadCalibrationParametersFromSpiMemory();

		SendSubCommand(0x03, {0x30}, true); // Set input report mode
		SendSubCommand(0x40, {static_cast<uint8_t>(imuSensorsEnabled_ ? 0x01 : 0x00)}, true); // enable/disable imuData
		SendSubCommand(0x48, {0x01}, true); // enable Rumble
		SendSubCommand(0x38, {0x2F, 0x10, 0x11, 0x33, 0x33}, true); // Set HOME Light animation
		SendSubCommand(0x30, {playerLedStatus_}, true); // Set Player LED Status

		statusCallback_ = std::move(statusCallback);

		controllerUpdaterThreadRunning_.test_and_set();
		controllerUpdaterThread_ = std::thread([this, threadName = std::string(pathToDevice) + "-UpdaterThread"]
			{
				SysDep::SetThreadName(threadName.c_str());
				SysDep::SetThreadPriorityToRealtime();
				auto clock = Clock::now();

				decltype(playerLedStatus_) playerLedStatus = playerLedStatus_;
				decltype(playerLedStatus_) playerLedStatusSending = playerLedStatus_;
				auto updatePlayerLedStatus = [&]
				{
					if (subCommandQueue_.Pending(0x30)) { return false; }
					if (subCommandQueue_.Wait(0x30)) { playerLedStatus = playerLedStatusSending; }
					if (playerLedStatusSending != playerLedStatus_ || playerLedStatus != playerLedStatus_)
					{
						playerLedStatusSending = playerLedStatus_;
						SendSubCommand(0x30, {playerLedStatusSending}, false);
						return true;
					}
					return false;
				};

				while (controllerUpdaterThreadRunning_.test_and_set())
				{
					if (updatePlayerLedStatus())
					{
					}
					else
					{
						SendRumble();
					}
					clock += std::chrono::nanoseconds(16666667);
					std::this_thread::sleep_until(clock);
				}
			}
		);
	}

	ProControllerImpl::~ProControllerImpl()
	{
		if (controllerUpdaterThread_.joinable())
		{
			controllerUpdaterThreadRunning_.clear();
			controllerUpdaterThread_.join();
		}
		SendSubCommand(0x38, {0x00}, true); // Set HOME Light animation
		SendSubCommand(0x30, {0x00}, true); // Set Player LED
		SendUsbCommand(0x05, {}, false); // Allows the Joy-Con or Pro Controller to time out and talk Bluetooth again.
		//ExecSubCommand(0x06, { 0x00 }, false); // Set HCI state (sleep mode)
		device_.CloseDevice();
	}

	void ProControllerImpl::StartStatusCallback()
	{
		std::lock_guard<decltype(statusCallbackCalling_)> lock(statusCallbackCalling_);
		statusCallbackEnabled_ = true;
	}

	void ProControllerImpl::StopStatusCallback()
	{
		std::lock_guard<decltype(statusCallbackCalling_)> lock(statusCallbackCalling_);
		statusCallbackEnabled_ = false;
	}

	CorrectedInputStatus ProControllerImpl::CorrectInput(const InputStatus &raw)
	{
		const auto correctStick = [](SpiCalibrationParameters::StickCalibration a, SpiCalibrationParameters::StickCalibration::Range r, int16_t value)
		{
			const int upperLower = r.CenterValue + a.DeadZone;
			const int lowerUpper = r.CenterValue - a.DeadZone;
			float f{};
			if (value >= upperLower) { f = static_cast<float>(value - upperLower) / static_cast<float>(r.MaxValue - upperLower); }
			else if (value <= lowerUpper) { f = -static_cast<float>(value - lowerUpper) / static_cast<float>(r.MinValue - lowerUpper); }
			else f = 0.0f;
			return std::min(std::max(f, -1.0f), 1.0f);
		};

		CorrectedInputStatus result{};
		result.clock = raw.clock;
		result.LeftStick.X = correctStick(calibrationParameters_.LeftStick, calibrationParameters_.LeftStick.X, raw.LeftStick.AxisX);
		result.LeftStick.Y = correctStick(calibrationParameters_.LeftStick, calibrationParameters_.LeftStick.Y, raw.LeftStick.AxisY);
		result.RightStick.X = correctStick(calibrationParameters_.RightStick, calibrationParameters_.RightStick.X, raw.RightStick.AxisX);
		result.RightStick.Y = correctStick(calibrationParameters_.RightStick, calibrationParameters_.RightStick.Y, raw.RightStick.AxisY);
		result.Buttons = raw.Buttons;

		result.HasSensorStatus = raw.HasSensorStatus;
		if (raw.HasSensorStatus)
		{
			const auto correctAccelerometer = [](SpiCalibrationParameters::SensorCalibration::Axis a, int16_t value)
			{
				//if (abs(value) < 205) { return 0.0f; }
				return static_cast<float>(value * 4.0 / (a.Sensitivity - a.Origin));
			};

			const auto correctGyroscope = [](SpiCalibrationParameters::SensorCalibration::Axis a, int16_t value)
			{
				//if (abs(value - a.Origin) < 75) { return 0.0f; }
				return static_cast<float>((value - a.Origin) * 0.0027777778 * 936.0 / (a.Sensitivity - a.Origin));
			};

			for (int i = 0; i < 3; i++)
			{
				result.Sensors[i].Accelerometer.X = correctAccelerometer(calibrationParameters_.Accelerometer.X, raw.Sensors[i].Accelerometer.X);
				result.Sensors[i].Accelerometer.Y = correctAccelerometer(calibrationParameters_.Accelerometer.Y, raw.Sensors[i].Accelerometer.Y);
				result.Sensors[i].Accelerometer.Z = correctAccelerometer(calibrationParameters_.Accelerometer.Z, raw.Sensors[i].Accelerometer.Z);
				result.Sensors[i].Gyroscope.X = correctGyroscope(calibrationParameters_.Gyroscope.X, raw.Sensors[i].Gyroscope.X);
				result.Sensors[i].Gyroscope.Y = correctGyroscope(calibrationParameters_.Gyroscope.Y, raw.Sensors[i].Gyroscope.Y);
				result.Sensors[i].Gyroscope.Z = correctGyroscope(calibrationParameters_.Gyroscope.Z, raw.Sensors[i].Gyroscope.Z);
			}
		}
		return result;
	}

	void ProControllerImpl::SetRumbleBasic(
		uint8_t leftLowAmp, uint8_t rightLowAmp, uint8_t leftHighAmp, uint8_t rightHighAmp,
		uint8_t leftLowFreq, uint8_t rightLowFreq, uint8_t leftHighFreq, uint8_t rightHighFreq)
	{
		uint32_t l = 0x40000000u | leftHighFreq >> 1 << 2 | leftHighAmp >> 1 << 9 | leftLowFreq >> 1 << 16 | leftLowAmp >> 1 << 23;
		uint32_t r = 0x40000000u | rightHighFreq >> 1 << 2 | rightHighAmp >> 1 << 9 | rightLowFreq >> 1 << 16 | rightLowAmp >> 1 << 23;

		rumbleStatus_[0] = l, rumbleStatus_[1] = l >> 8, rumbleStatus_[2] = l >> 16, rumbleStatus_[3] = l >> 24;
		rumbleStatus_[4] = r, rumbleStatus_[5] = r >> 8, rumbleStatus_[6] = r >> 16, rumbleStatus_[7] = r >> 24;
	}

	void ProControllerImpl::SetPlayerLed(uint8_t playerLed)
	{
		playerLedStatus_ = playerLed;
	}


	ProControllerImpl::SpiCalibrationParameters ProControllerImpl::LoadCalibrationParametersFromSpiMemory()
	{
		SpiCalibrationParameters result{};

		// SensorCalibration
		{
			Buffer factory = ReadSpiMemory(0x6020, 24);
			Buffer user = ReadSpiMemory(0x8026, 2 + 24);
			const uint16_t *mem = reinterpret_cast<uint16_t*>(&factory[0]);
			if (user[0] == 0xB2 && user[1] == 0xA1)
			{
				mem = reinterpret_cast<uint16_t*>(&user[2]);
			}

			result.Accelerometer.X.Origin = mem[0];
			result.Accelerometer.Y.Origin = mem[1];
			result.Accelerometer.Z.Origin = mem[2];
			result.Accelerometer.X.Sensitivity = mem[3];
			result.Accelerometer.Y.Sensitivity = mem[4];
			result.Accelerometer.Z.Sensitivity = mem[5];
			result.Gyroscope.X.Origin = mem[6];
			result.Gyroscope.Y.Origin = mem[7];
			result.Gyroscope.Z.Origin = mem[8];
			result.Gyroscope.X.Sensitivity = mem[9];
			result.Gyroscope.Y.Sensitivity = mem[10];
			result.Gyroscope.Z.Sensitivity = mem[11];
		}

		{
			// SensorParameter
			Buffer factory = ReadSpiMemory(0x6080, 6);
			const uint16_t *mem = reinterpret_cast<uint16_t*>(&factory[0]);
			result.Accelerometer.X.HorizontalOffset = mem[0];
			result.Accelerometer.Y.HorizontalOffset = mem[1];
			result.Accelerometer.Z.HorizontalOffset = mem[2];
		}

		// Stick1
		const auto ReadStick = [this](
			uint16_t factoryCalibrationAddress,
			uint16_t userCalibrationAddress,
			uint16_t paramsAddress, int stickIndex)
		-> SpiCalibrationParameters::StickCalibration
		{
			SpiCalibrationParameters::StickCalibration result{};

			Buffer factory = ReadSpiMemory(factoryCalibrationAddress, 9);
			Buffer user = ReadSpiMemory(userCalibrationAddress, 2 + 9);
			Buffer params = ReadSpiMemory(paramsAddress, 18);

			auto parse = [](const uint8_t bytes[3]) -> std::pair<uint16_t, uint16_t>
			{
				return {bytes[0] | (bytes[1] << 8 & 0xF00), bytes[1] >> 4 | bytes[2] << 4};
			};

			{
				const uint8_t *mem = &factory[0];
				if (user[0] == 0xB2 && user[1] == 0xA1)
				{
					mem = &user[2];
				}
				auto aboveCenter = parse(&mem[stickIndex == 0 ? 0 : 6]);
				auto center = parse(&mem[stickIndex == 0 ? 3 : 0]);
				auto belowCenter = parse(&mem[stickIndex == 0 ? 6 : 3]);
				result.X.MinValue = center.first - belowCenter.first;
				result.X.CenterValue = center.first;
				result.X.MaxValue = center.first + aboveCenter.first;
				result.Y.MinValue = center.second - belowCenter.second;
				result.Y.CenterValue = center.second;
				result.Y.MaxValue = center.second + aboveCenter.second;
			}

			{
				const uint8_t *mem = &params[0];
				auto stickParams = parse(&mem[3]);
				result.DeadZone = stickParams.first;
				result.RangeRatio = stickParams.second;
			}

			return result;
		};

		result.LeftStick = ReadStick(0x603D, 0x8010, 0x6086, 0);
		result.RightStick = ReadStick(0x6046, 0x801D, 0x6098, 1);

		return result;
	}

	void ProControllerImpl::SendUsbCommand(uint8_t usbCommand, const Buffer &data, bool waitAck)
	{
		Buffer buf = {
			0x80,
		};
		buf += {usbCommand};
		buf += data;
		DumpPacket("UsbCmd>", buf);

		usbCommandQueue_.Register(usbCommand, nullptr);
		device_.SendPacket(buf);
		if (waitAck)
		{
			while (!usbCommandQueue_.Wait(usbCommand))
			{
				usbCommandQueue_.Register(usbCommand, nullptr);
				device_.SendPacket(buf);
			}
		}
	}

	void ProControllerImpl::SendSubCommand(uint8_t subCommand, const Buffer &data, bool waitAck, const PacketProc &callback)
	{
		Buffer buf = {
			0x01, // SubCommand
			static_cast<uint8_t>(nextPacketNumber_++ & 0xf),
			rumbleStatus_[0], rumbleStatus_[1], rumbleStatus_[2], rumbleStatus_[3],
			rumbleStatus_[4], rumbleStatus_[5], rumbleStatus_[6], rumbleStatus_[7],
		};
		buf += {subCommand};
		buf += data;
		DumpPacket("SubCmd>", buf, 10);

		subCommandQueue_.Register(subCommand, callback);
		device_.SendPacket(buf);

		if (waitAck)
		{
			while (!subCommandQueue_.Wait(subCommand))
			{
				subCommandQueue_.Register(subCommand, callback);
				device_.SendPacket(buf);
			}
		}
	}

	Buffer ProControllerImpl::ReadSpiMemory(uint16_t address, uint8_t length)
	{
		Buffer result;
		union
		{
			uint32_t addr32{};
			uint8_t addr8[4];
		};
		addr32 = address;
		SendSubCommand(0x10, {addr8[0], addr8[1], addr8[2], addr8[3], length}, true,
			[&](const Buffer &reply)
			{
				const uint32_t replyAddress = reply[15] | reply[16] << 8 | reply[17] << 16 | reply[18] << 24;
				const uint8_t replyLength = reply[19];
				if (replyAddress == address && length == replyLength)
				{
					result.resize(length);
					memcpy(result.data(), reply.data() + 20, length);
				}
			});

		return result;
	}

	void ProControllerImpl::SendRumble()
	{
		Buffer buf = {
			0x10,
			static_cast<uint8_t>(nextPacketNumber_++ & 0xf),
			rumbleStatus_[0], rumbleStatus_[1], rumbleStatus_[2], rumbleStatus_[3],
			rumbleStatus_[4], rumbleStatus_[5], rumbleStatus_[6], rumbleStatus_[7],
		};
		device_.SendPacket(buf);
	}

	void ProControllerImpl::OnPacket(const Buffer &data)
	{
		if (data.size())
		{
			switch (data[0])
			{
			case 0x30: // Input status full
			case 0x31: // Input status full
				OnStatus(data);
				break;

			case 0x21: // Reply to sub command.
				subCommandQueue_.Signal(data[14], data);
				OnStatus(data);
				break;

			case 0x81: // Reply to usb command.
				usbCommandQueue_.Signal(data[1], data);
				break;

			default:
				DumpPacket("Packet<", data, 0, 16);
				break;
			}
		}
	}

	void ProControllerImpl::OnStatus(const Buffer &data)
	{
		union
		{
			StickStatus status;
			uint32_t raw;
		} lStick{}, rStick{};

		union
		{
			ButtonStatus status;
			uint32_t raw;
		} buttons{};

		buttons.raw = data[3] | data[4] << 8 | data[5] << 16;
		lStick.raw = data[6] | data[7] << 8 | data[8] << 16;
		rStick.raw = data[9] | data[10] << 8 | data[11] << 16;

		InputStatus status = {};
		status.clock = tick();
		status.LeftStick = lStick.status;
		status.RightStick = rStick.status;
		status.Buttons = buttons.status;

		status.HasSensorStatus = imuSensorsEnabled_ && data[0] == 0x30;
		if (status.HasSensorStatus)
		{
			memcpy(status.Sensors, &data[13], 12 * 3);
		}

		{
			std::lock_guard<decltype(statusCallbackCalling_)> lock(statusCallbackCalling_);
			if (statusCallbackEnabled_ && statusCallback_)
			{
				statusCallback_(status);
			}
		}
	}

	std::unique_ptr<ProController> ProController::Connect(const char *pathToDevice, int index, InputStatusCallback statusCallback)
	{
		return std::make_unique<ProControllerImpl>(pathToDevice, index, std::move(statusCallback));
	}

	std::vector<std::string> ProController::EnumerateProControllerDevicePaths()
	{
		std::vector<std::string> result;
		constexpr unsigned short kNintendoVID{0x057E};
		constexpr unsigned short kProControllerPID{0x2009};
		for (auto &&d : HidIo::EnumerateConnectedDevices(kNintendoVID, kProControllerPID))
		{
			result.push_back(d.devicePath);
		}
		return result;
	}
}

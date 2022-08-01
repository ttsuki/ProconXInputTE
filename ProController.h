#pragma once
#include <cstdint>
#include <chrono>
#include <memory>
#include <functional>
#include <vector>
#include <string>

namespace ProControllerHid
{
	using Clock = std::chrono::high_resolution_clock;
	using Timestamp = Clock::time_point;

	struct StickStatus
	{
		unsigned AxisX : 12;
		unsigned AxisY : 12;
		unsigned _padding : 8;
	};

	struct ButtonStatus
	{
		unsigned YButton : 1;
		unsigned XButton : 1;
		unsigned BButton : 1;
		unsigned AButton : 1;
		unsigned RSRButton : 1;
		unsigned RSLButton : 1;
		unsigned RButton : 1;
		unsigned RZButton : 1;

		unsigned MinusButton : 1;
		unsigned PlusButton : 1;
		unsigned RStick : 1;
		unsigned LStick : 1;
		unsigned HomeButton : 1;
		unsigned ShareButton : 1;
		unsigned Unknown3 : 1;
		unsigned ChargingGrip : 1;

		unsigned DownButton : 1;
		unsigned UpButton : 1;
		unsigned RightButton : 1;
		unsigned LeftButton : 1;
		unsigned LSRButton : 1;
		unsigned LSLButton : 1;
		unsigned LButton : 1;
		unsigned LZButton : 1;

		unsigned _padding : 8;
	};

	struct SensorStatus
	{
		struct Vector3i
		{
			int16_t X, Y, Z;
		};

		Vector3i Accelerometer;
		Vector3i Gyroscope;
	};

	struct RawInputStatus
	{
		Timestamp Timestamp;
		StickStatus LeftStick;
		StickStatus RightStick;
		ButtonStatus Buttons;
		bool HasSensorStatus;
		SensorStatus Sensors[3];
	};

	struct Vector2f
	{
		float X, Y;
	};

	struct Vector3f
	{
		float X, Y, Z;
	};

	struct ImuSensorStatus
	{
		Vector3f Accelerometer;
		Vector3f Gyroscope;
	};

	struct InputStatus
	{
		Timestamp Timestamp;
		Vector2f LeftStick;
		Vector2f RightStick;
		ButtonStatus Buttons;
		bool HasSensorStatus;
		ImuSensorStatus Sensors[3];
	};

	class ProController
	{
	public:
		ProController() = default;
		ProController(const ProController& other) = delete;
		ProController(ProController&& other) noexcept = delete;
		ProController& operator=(const ProController& other) = delete;
		ProController& operator=(ProController&& other) noexcept = delete;
		virtual ~ProController() = default;

	public:
		static inline constexpr uint16_t DeviceVendorID{0x057E};
		static inline constexpr uint16_t DeviceProductID{0x2009};
		static std::vector<std::string> EnumerateProControllerDevicePaths();

		static std::unique_ptr<ProController> Connect(
			const char* device_path,
			bool enable_imu_sensor = false,
			std::function<void(const char*)> write_log_callback = nullptr,
			bool dump_packet_log = false);

		virtual void SetInputStatusCallback(std::function<void(const InputStatus& status)> callback) = 0;

		virtual void SetRawInputStatusCallback(std::function<void(const RawInputStatus& status)> callback) = 0;

		virtual void SetPlayerLed(uint8_t player_led_bits) = 0;

		struct BasicRumble
		{
			struct
			{
				struct
				{
					uint8_t Freq, Amp;
				} High, Low;
			} Left{{0x80, 0x00}, {0x80, 0x00}}
			, Right{{0x80, 0x00}, {0x80, 0x00}};
		};

		virtual void SetRumble(BasicRumble rumble) = 0;

		void SetRumbleBasic(
			uint8_t left_low_amp, uint8_t right_low_amp,
			uint8_t left_high_amp = 0x00, uint8_t right_high_amp = 0x00,
			uint8_t left_low_freq = 0x80, uint8_t right_low_freq = 0x80,
			uint8_t left_high_freq = 0x80, uint8_t right_high_freq = 0x80)
		{
			return SetRumble(BasicRumble{
				{{left_high_freq, left_high_amp}, {left_low_freq, left_low_amp}},
				{{right_high_freq, right_high_amp}, {right_low_freq, right_low_amp}}
			});
		}
	};

	// Status dump helper
	std::string DumpInputStatusAsString(const RawInputStatus& input);
	std::string InputStatusAsString(const RawInputStatus& input);
	std::string InputStatusAsString(const InputStatus& input);
	std::string ImuSensorStatusAsString(const RawInputStatus& input);
	std::string ImuSensorStatusAsString(const InputStatus& input);
}

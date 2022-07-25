#include "ProController.h"

#include <chrono>

#include <array>
#include <queue>
#include <map>
#include <algorithm>
#include <optional>

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

#include "hidio.h"

namespace utl
{
	template <class T, class U, std::enable_if_t<sizeof(T) == sizeof(U) && std::is_trivial_v<T> && std::is_trivial_v<U>>* = nullptr>
	static T bit_cast(U u)
	{
		T t{};
		std::memcpy(&t, &u, sizeof(t));
		return t;
	}

	template <class T> using lazy = std::optional<T>;

	template <class T, size_t Size>
	class fixed_buffer_string final
	{
		using base_container = std::array<T, Size>;

	public:
		using value_type = typename base_container::value_type;
		using size_type = typename base_container::size_type;
		using difference_type = typename base_container::difference_type;
		using iterator = typename base_container::iterator;
		using reference = typename base_container::reference;
		using pointer = typename base_container::pointer;
		using const_iterator = typename base_container::const_iterator;
		using const_reference = typename base_container::const_reference;
		using const_pointer = typename base_container::const_pointer;

	private:
		base_container container_{};
		size_type size_{};

	public:
		constexpr fixed_buffer_string() = default;

		constexpr fixed_buffer_string(const value_type* data, size_t length)
		{
			size_type sz = size_ = std::min(length, capacity());
			for (size_t i = 0; i < sz; ++i) container_[i] = data[i]; // std::copy_n
		}

		constexpr fixed_buffer_string(std::initializer_list<value_type> list)
		{
			size_type sz = size_ = std::min(list.size(), capacity());
			for (size_t i = 0; i < sz; ++i) container_[i] = *(list.begin() + i); // std::copy_n
		}

		[[nodiscard]] constexpr pointer data() noexcept { return container_.data(); }
		[[nodiscard]] constexpr const_pointer data() const noexcept { return container_.data(); }
		[[nodiscard]] constexpr size_type size() const noexcept { return size_; }
		[[nodiscard]] constexpr size_type capacity() const noexcept { return container_.size(); }
		[[nodiscard]] constexpr reference operator[](size_type i) noexcept { return container_.operator[](i); }
		[[nodiscard]] constexpr const_reference operator[](size_type i) const noexcept { return container_.operator[](i); }

		[[nodiscard]] constexpr iterator begin() noexcept { return container_.begin(); }
		[[nodiscard]] constexpr iterator end() noexcept { return container_.begin() + static_cast<difference_type>(size_); }
		[[nodiscard]] constexpr const_iterator begin() const noexcept { return container_.begin(); }
		[[nodiscard]] constexpr const_iterator end() const noexcept { return container_.begin() + static_cast<difference_type>(size_); }

		constexpr void clear() noexcept { size_ = 0; }
		constexpr void resize(size_t sz) noexcept { size_ = sz; }

		template <size_t Size2>
		constexpr fixed_buffer_string& operator +=(const fixed_buffer_string<T, Size2>& rhs) noexcept
		{
			size_type sz = std::min(capacity() - size(), rhs.size());
			for (size_t i = 0; i < sz; ++i) container_[size_ + i] = rhs[i]; // std::copy_n
			size_ += sz;
			return *this;
		}

		constexpr fixed_buffer_string& operator +=(std::initializer_list<value_type> rhs) noexcept
		{
			size_type sz = std::min(capacity() - size(), rhs.size());
			for (size_t i = 0; i < sz; ++i) container_[size_ + i] = std::data(rhs)[i]; // std::copy_n
			size_ += sz;
			return *this;
		}
	};

}


namespace ProControllerHid
{
	using Packet = utl::fixed_buffer_string<uint8_t, 256>;

	class PacketSenderThread
	{
		std::shared_ptr<hidio::device> device_;

		std::mutex write_mutex_{};
		std::queue<std::optional<Packet>> write_queue_{};
		std::condition_variable write_cv_{};
		std::thread writer_thread_{};

	public:
		explicit PacketSenderThread(std::shared_ptr<hidio::device> device)
			: device_(std::move(device))
		{
			writer_thread_ = std::thread([this]
			{
				hidio::set_thread_priority_to_realtime();

				while (true)
				{
					auto packet = [this]
					{
						std::unique_lock lock(write_mutex_);
						write_cv_.wait(lock, [this] { return !write_queue_.empty(); });
						auto p = write_queue_.front();
						write_queue_.pop();
						return p;
					}();
					if (!packet) break;

					(void)device_->write(packet->data(), packet->size());
				}
			});
		}

		~PacketSenderThread()
		{
			if (writer_thread_.joinable())
			{
				{
					std::unique_lock lock(write_mutex_);
					write_queue_.emplace(std::nullopt);
					write_cv_.notify_one();
				}

				writer_thread_.join();
			}
		}

		void write(Packet packet)
		{
			std::unique_lock lock(write_mutex_);
			write_queue_.emplace(std::in_place, packet);
			write_cv_.notify_one();
		}

		PacketSenderThread(const PacketSenderThread& other) = delete;
		PacketSenderThread(PacketSenderThread&& other) noexcept = delete;
		PacketSenderThread& operator=(const PacketSenderThread& other) = delete;
		PacketSenderThread& operator=(PacketSenderThread&& other) noexcept = delete;
	};

	class PacketReceiverThread
	{
		std::shared_ptr<hidio::device> device_;
		std::function<void(const Packet& packet)> reader_callback_{};

		std::thread reader_thread_{};
		std::atomic_flag reader_running_{};

	public:
		explicit PacketReceiverThread(std::shared_ptr<hidio::device> device, std::function<void(const Packet& packet)> reader_callback)
			: device_(std::move(device))
			, reader_callback_(std::move(reader_callback))
		{
			reader_running_.test_and_set();
			reader_thread_ = std::thread([this]
			{
					hidio::set_thread_priority_to_realtime();

				Packet buf{};
				while (true)
				{
					int size = device_->read(buf.data(), buf.capacity(), 100);
					if (!reader_running_.test_and_set()) break;
					if (size > 0)
					{
						buf.resize(size);
						reader_callback_(buf);
					}
				}
			});
		}

		~PacketReceiverThread()
		{
			if (reader_thread_.joinable())
			{
				reader_running_.clear();
				reader_thread_.join();
			}
		}

		PacketReceiverThread(const PacketReceiverThread& other) = delete;
		PacketReceiverThread(PacketReceiverThread&& other) noexcept = delete;
		PacketReceiverThread& operator=(const PacketReceiverThread& other) = delete;
		PacketReceiverThread& operator=(PacketReceiverThread&& other) noexcept = delete;
	};

	class WaitingResponseMap
	{
		using command_t = uint8_t;
		using callback_t = std::function<void(const Packet&)>;

		struct entry_t
		{
			Timestamp timeout;
			callback_t callback;
		};

		std::shared_mutex mutex_{};
		std::condition_variable_any cv_{};
		std::map<command_t, entry_t> waiting_map_{};

	public:
		void register_callback(
			command_t command,
			callback_t callback = nullptr,
			Clock::duration timeout = std::chrono::milliseconds(1000))
		{
			std::lock_guard lock(mutex_);
			waiting_map_[command] = {Clock::now() + timeout, std::move(callback)};
		}

		void notify(command_t command, const Packet& reply)
		{
			std::lock_guard lock(mutex_);

			if (auto it = waiting_map_.find(command); it != waiting_map_.end())
			{
				const callback_t callback = std::move(it->second.callback);
				waiting_map_.erase(command);
				if (callback) { callback(reply); }
			}

			cv_.notify_all();
		}

		bool is_waiting(command_t command)
		{
			std::shared_lock lock(mutex_);

			if (auto it = waiting_map_.find(command);
				it != waiting_map_.end() && Clock::now() <= it->second.timeout)
				return true;

			return false;
		}

		bool wait(command_t command)
		{
			std::shared_lock lock(mutex_);

			auto it = waiting_map_.find(command);
			if (it == waiting_map_.end()) return true;

			return cv_.wait_until(lock, it->second.timeout, [this, command]
			{
				return !is_waiting(command);
			});
		}
	};

	struct InputCorrector
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
				uint16_t MinValue, CenterValue, MaxValue;
			} X, Y;

			uint16_t DeadZone, RangeRatio;
		} LeftStick, RightStick;

		[[nodiscard]] static InputCorrector LoadFromSpiMemory(std::function<Packet(uint16_t address, uint8_t length)> ReadSpiMemory)
		{
			InputCorrector result{};

			// SensorCalibration
			{
				Packet factory = ReadSpiMemory(0x6020, 24);
				Packet user = ReadSpiMemory(0x8026, 2 + 24);
				const uint16_t* source = reinterpret_cast<uint16_t*>(&factory[0]);
				if (user[0] == 0xB2 && user[1] == 0xA1)
				{
					source = reinterpret_cast<uint16_t*>(&user[2]);
				}

				result.Accelerometer.X.Origin = source[0];
				result.Accelerometer.Y.Origin = source[1];
				result.Accelerometer.Z.Origin = source[2];
				result.Accelerometer.X.Sensitivity = source[3];
				result.Accelerometer.Y.Sensitivity = source[4];
				result.Accelerometer.Z.Sensitivity = source[5];
				result.Gyroscope.X.Origin = source[6];
				result.Gyroscope.Y.Origin = source[7];
				result.Gyroscope.Z.Origin = source[8];
				result.Gyroscope.X.Sensitivity = source[9];
				result.Gyroscope.Y.Sensitivity = source[10];
				result.Gyroscope.Z.Sensitivity = source[11];
			}

			{
				// SensorParameter
				Packet factory = ReadSpiMemory(0x6080, 6);
				const uint16_t* source = reinterpret_cast<uint16_t*>(&factory[0]);
				result.Accelerometer.X.HorizontalOffset = source[0];
				result.Accelerometer.Y.HorizontalOffset = source[1];
				result.Accelerometer.Z.HorizontalOffset = source[2];
			}

			// Stick1
			const auto ReadStick = [&](
				uint16_t factoryCalibrationAddress,
				uint16_t userCalibrationAddress,
				uint16_t paramsAddress, int stickIndex)
			-> StickCalibration
			{
				StickCalibration result{};

				Packet factory = ReadSpiMemory(factoryCalibrationAddress, 9);
				Packet user = ReadSpiMemory(userCalibrationAddress, 2 + 9);
				Packet params = ReadSpiMemory(paramsAddress, 18);

				// 3byte(24bit) to 2x 12bit
				auto parse = [](const uint8_t bytes[3]) -> std::pair<uint16_t, uint16_t>
				{
					return {
						static_cast<uint16_t>((bytes[0] >> 0 & 0x0FF) | (bytes[1] << 8 & 0xF00)),
						static_cast<uint16_t>((bytes[1] >> 4 & 0x00F) | (bytes[2] << 4 & 0xFF0))
					};
				};

				{
					const uint8_t* source = &factory[0];
					if (user[0] == 0xB2 && user[1] == 0xA1)
					{
						source = &user[2];
					}
					auto [x_positive, y_positive] = parse(&source[stickIndex == 0 ? 0 : 6]);
					auto [x_center, y_center] = parse(&source[stickIndex == 0 ? 3 : 0]);
					auto [x_negative, y_negative] = parse(&source[stickIndex == 0 ? 6 : 3]);
					result.X.MinValue = x_center - x_negative;
					result.X.CenterValue = x_center;
					result.X.MaxValue = x_center + x_positive;
					result.Y.MinValue = y_center - y_negative;
					result.Y.CenterValue = y_center;
					result.Y.MaxValue = y_center + y_positive;
				}

				{
					const uint8_t* source = &params[0];
					std::tie(result.DeadZone, result.RangeRatio) = parse(&source[3]);
				}

				return result;
			};

			result.LeftStick = ReadStick(0x603D, 0x8010, 0x6086, 0);
			result.RightStick = ReadStick(0x6046, 0x801D, 0x6098, 1);

			return result;
		}

		[[nodiscard]] InputStatus CorrectInput(const RawInputStatus& raw) const noexcept
		{
			constexpr auto CorrectStick = [](
				const StickCalibration& cal,
				const StickStatus& value) -> Vector2f
			{
				int x = static_cast<int>(value.AxisX) - cal.X.CenterValue;
				int y = static_cast<int>(value.AxisY) - cal.Y.CenterValue;
				if (abs(x) <= cal.DeadZone && abs(y) <= cal.DeadZone)
				{
					return Vector2f{};
				}

				const auto normalize = [](int value, const StickCalibration::Range& r)
				{
					const float f = value >= 0
						                ? static_cast<float>(value) / static_cast<float>(r.MaxValue - r.CenterValue)
						                : static_cast<float>(-value) / static_cast<float>(r.MinValue - r.CenterValue);
					return std::min(std::max(f, -1.0f), 1.0f);
				};
				Vector2f result{};
				result.X = normalize(x, cal.X);
				result.Y = normalize(y, cal.Y);
				return result;
			};

			InputStatus result{};
			result.Timestamp = raw.Timestamp;
			result.LeftStick = CorrectStick(this->LeftStick, raw.LeftStick);
			result.RightStick = CorrectStick(this->RightStick, raw.RightStick);
			result.Buttons = raw.Buttons;

			result.HasSensorStatus = raw.HasSensorStatus;
			if (raw.HasSensorStatus)
			{
				constexpr auto CorrectAccelerometer = [](SensorCalibration::Axis a, int16_t value)
				{
					//if (abs(value) < 205) { return 0.0f; }
					return static_cast<float>(value * 4.0 / (a.Sensitivity - a.Origin));
				};

				constexpr auto CorrectGyroscope = [](SensorCalibration::Axis a, int16_t value)
				{
					//if (abs(value - a.Origin) < 75) { return 0.0f; }
					return static_cast<float>((value - a.Origin) * 0.0027777778 * 936.0 / (a.Sensitivity - a.Origin));
				};

				for (int i = 0; i < 3; i++)
				{
					result.Sensors[i].Accelerometer.X = CorrectAccelerometer(this->Accelerometer.X, raw.Sensors[i].Accelerometer.X);
					result.Sensors[i].Accelerometer.Y = CorrectAccelerometer(this->Accelerometer.Y, raw.Sensors[i].Accelerometer.Y);
					result.Sensors[i].Accelerometer.Z = CorrectAccelerometer(this->Accelerometer.Z, raw.Sensors[i].Accelerometer.Z);
					result.Sensors[i].Gyroscope.X = CorrectGyroscope(this->Gyroscope.X, raw.Sensors[i].Gyroscope.X);
					result.Sensors[i].Gyroscope.Y = CorrectGyroscope(this->Gyroscope.Y, raw.Sensors[i].Gyroscope.Y);
					result.Sensors[i].Gyroscope.Z = CorrectGyroscope(this->Gyroscope.Z, raw.Sensors[i].Gyroscope.Z);
				}
			}
			return result;
		}
	};


	class ProControllerImpl final : public ProController
	{
		std::string devicePath_{};
		bool imu_sensor_enabled_{};

		utl::lazy<PacketSenderThread> sender_{};
		utl::lazy<PacketReceiverThread> receiver_{};

		InputCorrector calibration_parameters_{};

		Packet rumble_data_{};
		uint8_t player_led_data_{};

		std::thread update_thread_{};
		std::atomic_flag update_thread_running_{};

		std::recursive_mutex status_callback_mutex_{};
		std::function<void(const InputStatus& status)> input_status_callback_{};
		std::function<void(const RawInputStatus& status)> raw_input_status_callback_{};

	private: // logger
		std::function<void(const char*)> write_log_callback_{};
		bool enable_packet_dump_{};
		std::mutex write_log_mutex_{};

		void write_log(const char* text)
		{
			if (write_log_callback_)
			{
				std::lock_guard lock(write_log_mutex_);
				write_log_callback_(text);
			}
		}

		void dump_packet(const char* msg, const Packet& data, size_t start = 0, size_t count = 0)
		{
			if (enable_packet_dump_)
			{
				utl::fixed_buffer_string<char, 256> buf(msg, std::min(strlen(msg), size_t{16}));
				if (count == 0) { count = data.size() - start; }
				count = std::min(count, size_t{64});
				for (size_t i = start; i < start + count; i++)
				{
					buf += {
						' ',
						"0123456789ABCDEF"[data[i] >> 4 & 0x0F],
						"0123456789ABCDEF"[data[i] >> 0 & 0x0F],
					};
				}

				write_log(buf.data());
			}
		}

	public:
		ProControllerImpl(
			const char* device_path,
			bool imu_sensor_enabled,
			std::function<void(const char*)> logger = nullptr,
			bool enable_packet_dump = false)
			: devicePath_(device_path)
			, imu_sensor_enabled_(imu_sensor_enabled)
			, write_log_callback_(std::move(logger))
			, enable_packet_dump_(enable_packet_dump)
		{
			SetRumbleBasic(0, 0, 0, 0, 0x80, 0x80, 0x80, 0x80);

			write_log((std::string("Opening device...: ") + device_path).c_str());
			auto device_for_write = hidio::device::open(device_path);
			auto device_for_read = hidio::device::open(device_path);
			if (!device_for_write || !device_for_read)
			{
				write_log((std::string("Failed to open device: ") + device_path).c_str());
				throw std::runtime_error(std::string("Failed to open device: ") + device_path);
			}

			sender_.emplace(std::move(device_for_write));
			receiver_.emplace(std::move(device_for_read), [this](const Packet& packet) { ProcessReceivedPacket(packet); });

			write_log("Handshaking...");
			SendUsbCommand(0x02, {}, true);  // Handshake
			SendUsbCommand(0x03, {}, true);  // Set baudrate to 3Mbps
			SendUsbCommand(0x02, {}, true);  // Handshake
			SendUsbCommand(0x04, {}, false); // HID only-mode (turn off Bluetooth)

			write_log("Reading calibration parameters...");
			calibration_parameters_ = InputCorrector::LoadFromSpiMemory(
				[this](uint16_t address, uint8_t length) { return this->ReadSpiMemory(address, length); });

			write_log("Setting up controller features...");
			SendSubCommand(0x03, {0x30}, true);                                                    // Set input report mode
			SendSubCommand(0x40, {static_cast<uint8_t>(imu_sensor_enabled_ ? 0x01 : 0x00)}, true); // enable/disable imuData
			SendSubCommand(0x48, {0x01}, true);                                                    // enable Rumble
			SendSubCommand(0x38, {0x2F, 0x10, 0x11, 0x33, 0x33}, true);                            // Set HOME Light animation
			SendSubCommand(0x30, {player_led_data_}, true);                                        // Set Player LED Status

			write_log("Starting control thread...");

			update_thread_running_.test_and_set();
			update_thread_ = std::thread([this]
				{
					hidio::set_thread_priority_to_realtime();

					auto clock = Clock::now();

					auto updatePlayerLedStatus = [&, sending_ = player_led_data_]() mutable
					{
						if (sub_cmd_queue_.is_waiting(0x30)) { return false; } // sending...
						if (sending_ != player_led_data_)
						{
							sending_ = player_led_data_;
							SendSubCommand(0x30, {sending_}, true);
							return true;
						}
						return false;
					};

					while (update_thread_running_.test_and_set())
					{
						if (updatePlayerLedStatus()) {}
						else
						{
							SendRumbleCommand();
						}
						clock += std::chrono::nanoseconds(16666667);
						std::this_thread::sleep_until(clock);
					}
				}
			);

			write_log("Ready.");
		}

		~ProControllerImpl() override
		{
			write_log("Stopping control thread...");
			if (update_thread_.joinable())
			{
				update_thread_running_.clear();
				update_thread_.join();
			}

			write_log("Cleaning up controller features...");
			SendSubCommand(0x38, {0x00}, true); // Set HOME Light animation
			SendSubCommand(0x30, {0x00}, true); // Set Player LED
			SendUsbCommand(0x05, {}, false);    // Allows the Joy-Con or Pro Controller to time out and talk Bluetooth again.
			//ExecSubCommand(0x06, { 0x00 }, false); // Set HCI state (sleep mode)

			write_log("Closing device...");
			sender_.reset();
			receiver_.reset();

			write_log("Closed.");
		}

		void SetInputStatusCallback(std::function<void(const InputStatus& status)> callback) override
		{
			std::lock_guard lock(status_callback_mutex_);
			input_status_callback_ = callback;
		}

		void SetRawInputStatusCallback(std::function<void(const RawInputStatus& status)> callback) override
		{
			std::lock_guard lock(status_callback_mutex_);
			raw_input_status_callback_ = callback;
		}

		void SetRumbleBasic(
			uint8_t left_low_amp, uint8_t right_low_amp,
			uint8_t left_high_amp = 0x00, uint8_t right_high_amp = 0x00,
			uint8_t left_low_freq = 0x80, uint8_t right_low_freq = 0x80,
			uint8_t left_high_freq = 0x80, uint8_t right_high_freq = 0x80) override
		{
			uint32_t l = 0x40000000u | left_high_freq >> 1 << 2 | left_high_amp >> 1 << 9 | left_low_freq >> 1 << 16 | left_low_amp >> 1 << 23;
			uint32_t r = 0x40000000u | right_high_freq >> 1 << 2 | right_high_amp >> 1 << 9 | right_low_freq >> 1 << 16 | right_low_amp >> 1 << 23;

			rumble_data_.resize(8);
			rumble_data_[0] = static_cast<uint8_t>(l >> 0);
			rumble_data_[1] = static_cast<uint8_t>(l >> 8);
			rumble_data_[2] = static_cast<uint8_t>(l >> 16);
			rumble_data_[3] = static_cast<uint8_t>(l >> 24);
			rumble_data_[4] = static_cast<uint8_t>(r >> 0);
			rumble_data_[5] = static_cast<uint8_t>(r >> 8);
			rumble_data_[6] = static_cast<uint8_t>(r >> 16);
			rumble_data_[7] = static_cast<uint8_t>(r >> 24);
		}

		void SetPlayerLed(uint8_t player_led_bits) override
		{
			player_led_data_ = player_led_bits & 0x0F;
		}

	private:
		uint8_t send_packet_count_{};
		WaitingResponseMap usb_cmd_queue_{};
		WaitingResponseMap sub_cmd_queue_{};

		void SendUsbCommand(uint8_t usb_command, const Packet& data, bool wait_ack)
		{
			Packet packet = {
				0x80,
			};
			packet += {usb_command};
			packet += data;

			do
			{
				dump_packet("UsbCmd>", packet);
				usb_cmd_queue_.register_callback(usb_command, nullptr);
				sender_->write(packet);
			}
			while (wait_ack && !usb_cmd_queue_.wait(usb_command));
		}

		void SendSubCommand(uint8_t sub_command, const Packet& data, bool wait_ack, const std::function<void(const Packet&)>& callback = nullptr)
		{
			Packet packet = {0x01}; // SubCommand;
			packet += {static_cast<uint8_t>(send_packet_count_++ & 0xf)};
			packet += rumble_data_;
			packet += {sub_command};
			packet += data;

			do
			{
				dump_packet("SubCmd>", packet, 10);
				sub_cmd_queue_.register_callback(sub_command, callback);
				sender_->write(packet);
			}
			while (wait_ack && !sub_cmd_queue_.wait(sub_command));
		}

		void SendRumbleCommand()
		{
			Packet packet = {0x10}; // Rumble update;
			packet += {static_cast<uint8_t>(send_packet_count_++ & 0xf)};
			packet += rumble_data_;
			sender_->write(packet);
		}

		void ProcessReceivedPacket(const Packet& packet)
		{
			if (packet.size())
			{
				switch (packet[0])
				{
				case 0x30: // Input status full
				case 0x31: // Input status full
					RaiseStatusPacketCallback(ParseStatusPacket(packet, imu_sensor_enabled_));
					break;

				case 0x21: // Reply to sub command.
					dump_packet(" Reply<", packet, 14, 1);
					sub_cmd_queue_.notify(packet[14], packet);
					RaiseStatusPacketCallback(ParseStatusPacket(packet, imu_sensor_enabled_));
					break;

				case 0x81: // Reply to usb command.
					dump_packet(" Reply<", packet, 1, 1);
					usb_cmd_queue_.notify(packet[1], packet);
					break;

				default: // Unknown packet
					dump_packet("Unknown packet received<", packet, 0, 16);
					break;
				}
			}
		}

		void RaiseStatusPacketCallback(const RawInputStatus& raw)
		{
			std::lock_guard lock(status_callback_mutex_);
			if (raw_input_status_callback_) raw_input_status_callback_(raw);
			if (input_status_callback_) input_status_callback_(calibration_parameters_.CorrectInput(raw));
		}

		static RawInputStatus ParseStatusPacket(const Packet& data, bool with_imu)
		{
			RawInputStatus status = {};
			status.Timestamp = Clock::now();
			status.LeftStick = utl::bit_cast<StickStatus>(data[6] | data[7] << 8 | data[8] << 16);
			status.RightStick = utl::bit_cast<StickStatus>(data[9] | data[10] << 8 | data[11] << 16);
			status.Buttons = utl::bit_cast<ButtonStatus>(data[3] | data[4] << 8 | data[5] << 16);

			if (with_imu && data[0] == 0x30)
			{
				static_assert(sizeof(RawInputStatus::Sensors) == 36);
				status.HasSensorStatus = true;
				status.Sensors[0] = reinterpret_cast<const SensorStatus*>(&data[13])[0];
				status.Sensors[1] = reinterpret_cast<const SensorStatus*>(&data[13])[1];
				status.Sensors[2] = reinterpret_cast<const SensorStatus*>(&data[13])[2];
			}
			return status;
		}

		Packet ReadSpiMemory(uint16_t address, uint8_t length)
		{
			Packet result;
			SendSubCommand(
				0x10,
				{
					static_cast<uint8_t>(static_cast<uint32_t>(address) >> 0),
					static_cast<uint8_t>(static_cast<uint32_t>(address) >> 8),
					static_cast<uint8_t>(static_cast<uint32_t>(address) >> 16),
					static_cast<uint8_t>(static_cast<uint32_t>(address) >> 24),
					length
				},
				true,
				[&](const Packet& reply)
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
	};

	std::vector<std::string> ProController::EnumerateProControllerDevicePaths()
	{
		std::vector<std::string> result;
		for (auto&& d : hidio::enumerate_devices(DeviceVendorID, DeviceProductID))
		{
			result.push_back(d.device_path);
		}
		return result;
	}

	std::unique_ptr<ProController> ProController::Connect(
		const char* device_path,
		bool enable_imu_sensor,
		std::function<void(const char*)> write_log_callback,
		bool dump_packet_log)
	{
		try
		{
			return std::make_unique<ProControllerImpl>(
				device_path,
				enable_imu_sensor,
				std::move(write_log_callback),
				dump_packet_log);
		}
		catch (...)
		{
			return nullptr;
		}
	}


	std::string DumpInputStatusAsString(const RawInputStatus& input)
	{
		char str[64];

		snprintf(
			str, sizeof(str),
			"%06X,%06X,%06X,%012llX,%012llX",
			utl::bit_cast<uint32_t>(input.LeftStick),
			utl::bit_cast<uint32_t>(input.RightStick),
			utl::bit_cast<uint32_t>(input.Buttons),
			utl::bit_cast<uint64_t>(std::array<int16_t, 4>{input.Sensors[0].Accelerometer.X, input.Sensors[0].Accelerometer.Y, input.Sensors[0].Accelerometer.Z}),
			utl::bit_cast<uint64_t>(std::array<int16_t, 4>{input.Sensors[0].Gyroscope.X, input.Sensors[0].Accelerometer.Y, input.Sensors[0].Accelerometer.Z})
		);
		return str;
	}

	std::string InputStatusAsString(const RawInputStatus& input)
	{
		char str[256];
		snprintf(
			str, sizeof(str),
			"L(%4u,%4u),R(%4u,%4u)"
			",Buttons:%s%s%s%s%s%s%s%s"
			"%s%s%s%s%s%s"
			"%s%s%s%s",

			static_cast<unsigned int>(input.LeftStick.AxisX),
			static_cast<unsigned int>(input.LeftStick.AxisY),
			static_cast<unsigned int>(input.RightStick.AxisX),
			static_cast<unsigned int>(input.RightStick.AxisY),

			input.Buttons.UpButton ? "U" : "",
			input.Buttons.DownButton ? "D" : "",
			input.Buttons.LeftButton ? "L" : "",
			input.Buttons.RightButton ? "R" : "",
			input.Buttons.AButton ? "A" : "",
			input.Buttons.BButton ? "B" : "",
			input.Buttons.XButton ? "X" : "",
			input.Buttons.YButton ? "Y" : "",

			input.Buttons.LButton ? "L" : "",
			input.Buttons.RButton ? "R" : "",
			input.Buttons.LZButton ? "Lz" : "",
			input.Buttons.RZButton ? "Rz" : "",
			input.Buttons.LStick ? "Ls" : "",
			input.Buttons.RStick ? "Rs" : "",

			input.Buttons.PlusButton ? "+" : "",
			input.Buttons.MinusButton ? "-" : "",
			input.Buttons.HomeButton ? "H" : "",
			input.Buttons.ShareButton ? "S" : ""
		);
		return str;
	}


	std::string InputStatusAsString(const InputStatus& input)
	{
		char str[256];
		snprintf(
			str, sizeof(str),
			"L(%+1.3f,%+1.3f),R(%+1.3f,%+1.3f)"
			",Buttons:%s%s%s%s%s%s%s%s"
			"%s%s%s%s%s%s"
			"%s%s%s%s",
			static_cast<double>(input.LeftStick.X),
			static_cast<double>(input.LeftStick.Y),
			static_cast<double>(input.RightStick.X),
			static_cast<double>(input.RightStick.Y),

			input.Buttons.UpButton ? "U" : "",
			input.Buttons.DownButton ? "D" : "",
			input.Buttons.LeftButton ? "L" : "",
			input.Buttons.RightButton ? "R" : "",
			input.Buttons.AButton ? "A" : "",
			input.Buttons.BButton ? "B" : "",
			input.Buttons.XButton ? "X" : "",
			input.Buttons.YButton ? "Y" : "",

			input.Buttons.LButton ? "L" : "",
			input.Buttons.RButton ? "R" : "",
			input.Buttons.LZButton ? "Lz" : "",
			input.Buttons.RZButton ? "Rz" : "",
			input.Buttons.LStick ? "Ls" : "",
			input.Buttons.RStick ? "Rs" : "",

			input.Buttons.PlusButton ? "+" : "",
			input.Buttons.MinusButton ? "-" : "",
			input.Buttons.HomeButton ? "H" : "",
			input.Buttons.ShareButton ? "S" : "");
		return str;
	}

	std::string ImuSensorStatusAsString(const RawInputStatus& input)
	{
		char str[256];
		const auto& sensor = input.Sensors[0];
		snprintf(
			str, sizeof(str),
			"Imu: Acl(%4d,%4d,%4d)/Gyr(%4d,%4d,%4d)",
			sensor.Accelerometer.X,
			sensor.Accelerometer.Y,
			sensor.Accelerometer.Z,
			sensor.Gyroscope.X,
			sensor.Gyroscope.Y,
			sensor.Gyroscope.Z);
		return str;
	}

	std::string ImuSensorStatusAsString(const InputStatus& input)
	{
		char str[256];
		const auto& sensor = input.Sensors[0];
		snprintf(
			str, sizeof(str),
			"Imu: Acl(%+.4f,%+.4f,%+.4f)/Gyr(%+.4f,%+.4f,%+.4f)",
			static_cast<double>(sensor.Accelerometer.X),
			static_cast<double>(sensor.Accelerometer.Y),
			static_cast<double>(sensor.Accelerometer.Z),
			static_cast<double>(sensor.Gyroscope.X),
			static_cast<double>(sensor.Gyroscope.Y),
			static_cast<double>(sensor.Gyroscope.Z));
		return str;
	}
}

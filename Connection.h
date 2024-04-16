#pragma once

#include <vector>
#include "JvCryption.h"

namespace pcpp
{
	class Packet;
}

class App;
class Connection
{
public:
	inline bool IsLogOpen() const {
		return _logHandle != nullptr;
	}

	Connection(
		App* app,
		bool isLogin,
		const std::string& clientIPv4,
		const uint16_t clientPort,
		const std::string& serverIPv4,
		const uint16_t serverPort,
		const time_t connectionTimestamp,
		const std::string& logFilename);
	void process_raw_packet(const pcpp::Packet& parsedPacket);

protected:
	void append_packet_data(const uint8_t* data, int length);

public:
	~Connection();

	static std::shared_ptr<Connection> find_or_create(
		App* app,
		const std::string& srcIPv4,
		uint16_t srcPort,
		const std::string& dstIPv4,
		uint16_t dstPort,
		time_t timestamp);
	static void append_packet_data_to_file(FILE* fp, const uint8_t* data, int length);

protected:
	App* _app;
	const time_t _connectionTimestamp;
	FILE* _logHandle;
	bool _isLogin;

	struct ConnectionData
	{
		struct Packet
		{
			Packet(const uint8_t* buffer, int length)
				: Buffer(buffer), Length(length)
			{
			}

			template <typename T>
			bool read(T& ret, size_t& offset)
			{
				if (offset + sizeof(T) > Length)
					return false;

				ret = *(T *)&Buffer[offset];
				offset += sizeof(T);
				return true;
			}

			const uint8_t* Buffer;
			int Length;
		};

		const std::string IPv4;
		const uint16_t Port;
		std::vector<uint8_t> Buffer;
		CJvCryption Cryption;
		bool IsJvCryptEnabled = false;
		bool IsServerToClient;
		uint16_t ExpectedHeader = 0;
		uint16_t ExpectedFooter = 0;
		bool ReceivedFirstPacket = false;
		std::string LastTimestamp;

		ConnectionData(bool is_client_to_server, const std::string& ipv4, uint16_t port)
			: IsServerToClient(is_client_to_server), IPv4(ipv4), Port(port)
		{
		}

		void append_to_buffer(
			Connection* connection,
			const char* timestampString,
			const uint8_t* payload,
			int payload_size);

		void decompress_packet(
			Connection* connection,
			Packet* pkt,
			size_t& offset);

		void s2c_version_check(
			Connection* connection,
			Packet* pkt,
			size_t& offset);

		void s2c_continous_packet(
			Connection* connection,
			Packet* pkt,
			size_t& offset);

		void enable_stock_crypto(uint64_t public_key);

	protected:
		enum class PacketType
		{
			Normal,
			Compressed_LZF,
			Compressed_PKWARE,
			Region
		};

		static const char* packet_type_for_log(PacketType packet_type);

		bool decrypt_packet(
			Connection* connection,
			uint8_t* data,
			uint16_t& len);

		void process_packet(
			Connection* connection,
			const uint8_t* data,
			int length,
			PacketType packet_type = PacketType::Normal);

		template <typename T>
		bool buffer_read(T& ret, size_t& offset)
		{
			if (offset + sizeof(T) > Buffer.size())
				return false;

			ret = *(T *)&Buffer[offset];
			offset += sizeof(T);
			return true;
		}
	};

	ConnectionData _server;
	ConnectionData _client;
};

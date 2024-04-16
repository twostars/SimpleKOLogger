#include "pch.h"
#include "Connection.h"
#include "App.h"
#include "packets.h"

#include "Compression_LZF.h"
#include "Compression_PKWARE.h"

#include <unordered_map>

#include <pcapplusplus/Packet.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/TcpLayer.h>

std::shared_ptr<Connection> Connection::find_or_create(
	App* app,
	const std::string& srcIPv4,
	uint16_t srcPort,
	const std::string& dstIPv4,
	uint16_t dstPort,
	time_t timestamp)
{
	static std::unordered_map<std::string, std::shared_ptr<Connection>> connection_map;

	// This isn't really the most efficient, but it's the easiest to debug.
	// Our dataset shouldn't be huge enough for it to be an issue.
	std::string conn1 = srcIPv4 + ":" + std::to_string(srcPort);
	std::string conn2 = dstIPv4 + ":" + std::to_string(dstPort);

	std::string key1 = conn1 + " " + conn2;

	auto itr = connection_map.find(key1);
	if (itr != connection_map.end())
		return itr->second;

	std::string key2 = conn2 + " " + conn1;

	itr = connection_map.find(key2);
	if (itr != connection_map.end())
		return itr->second;

	std::string logFilename = app->GetLogFileName()
		+ "_"
		+ std::to_string(timestamp)
		+ "_"
		+ std::to_string(dstPort)
		+ app->GetLogFileExt();

	auto connection = std::make_shared<Connection>(
		app,
		dstPort == app->GetDstPortLogin(), // isLogin
		srcIPv4,
		srcPort,
		dstIPv4,
		dstPort,
		timestamp,
		logFilename);
	connection_map.insert(std::make_pair(key1, connection));
	if (!connection->IsLogOpen())
		std::cerr << "Failed to open log file: " << logFilename << std::endl;

	return connection;
}

Connection::Connection(
	App* app,
	bool isLogin,
	const std::string& clientIPv4,
	const uint16_t clientPort,
	const std::string& serverIPv4,
	const uint16_t serverPort,
	const time_t connectionTimestamp,
	const std::string& logFilename)
	: _app(app),
	_isLogin(isLogin),
	_client(true, clientIPv4, clientPort),
	_server(false, serverIPv4, serverPort),
	_connectionTimestamp(connectionTimestamp)
{
	fopen_s(&_logHandle, logFilename.c_str(), "wb");
	if (_logHandle != nullptr)
		std::cout << "Opened log file " << logFilename << " for writing" << std::endl;
}

void Connection::process_raw_packet(const pcpp::Packet& parsedPacket)
{
	// Log failed to open, nothing we can do here.
	if (_logHandle == nullptr)
		return;

	auto tcpLayer = parsedPacket.getLayerOfType<pcpp::TcpLayer>();
	if (tcpLayer == nullptr)
		return;

	auto ipv4Layer = parsedPacket.getLayerOfType<pcpp::IPv4Layer>();
	if (ipv4Layer == nullptr)
		return;

	// Ignore any packets without data; these will be the initial setup.
	int payloadSize = (int) tcpLayer->getLayerPayloadSize();
	if (payloadSize == 0)
		return;

	auto rawPacket = parsedPacket.getRawPacket();

	char timestampString[128] = {};
	time_t local_tv_sec = rawPacket->getPacketTimeStamp().tv_sec;
	uint64_t local_tv_nsec = rawPacket->getPacketTimeStamp().tv_nsec;
	tm local_timestamp;
	localtime_s(&local_timestamp, &local_tv_sec);
	strftime(timestampString, sizeof(timestampString), "%H:%M:%S", &local_timestamp);
	strcat_s(timestampString, ".");
	strcat_s(timestampString, std::to_string(local_tv_nsec).c_str());

	// Client => server packets
	if (tcpLayer->getSrcPort() == _client.Port
		&& ipv4Layer->getSrcIPv4Address() == _client.IPv4)
	{
		std::cout
			<< timestampString
			<< " [CLIENT => SERVER] "
			<< _client.IPv4 << ":" << _client.Port
			<< " => "
			<< _server.IPv4 << ":" << _server.Port
			<< " (" << payloadSize << " bytes)"
			<< std::endl;

		// Pretend like we're the server receiving this packet
		_server.append_to_buffer(
			this,
			timestampString,
			tcpLayer->getLayerPayload(),
			payloadSize);
	}
	// Server => client packets
	else
	{
		std::cout
			<< timestampString
			<< " [SERVER => CLIENT] "
			<< _server.IPv4 << ":" << _server.Port
			<< " => "
			<< _client.IPv4 << ":" << _client.Port
			<< " (" << payloadSize << " bytes)"
			<< std::endl;

		// Pretend like we're the client receiving this packet
		_client.append_to_buffer(
			this,
			timestampString,
			tcpLayer->getLayerPayload(),
			payloadSize);
	}
}

void Connection::ConnectionData::append_to_buffer(
	Connection* connection,
	const char* timestampString,
	const uint8_t* payload,
	int payload_size)
{
	LastTimestamp = timestampString;

	// Append new packet data
	size_t oldPos = Buffer.size();
	Buffer.resize(Buffer.size() + payload_size);
	memcpy(&Buffer[oldPos], payload, payload_size);

	size_t offset = 0;

	// Attempt to extract packet(s) in the buffer

	uint16_t len = 0;

	// If we haven't received our first packet, use it to identify the header info for future packet parsing.
	if (!ReceivedFirstPacket)
	{
		// Expected: 2 byte header, 2 byte length, 1 byte opcode (at least), 2 byte footer

		// NOTE: We'll just keep this in network order
		if (!buffer_read(ExpectedHeader, offset))
			return;

		// NOTE: This is always little endian (server order).
		if (!buffer_read(len, offset))
			return;

		// Don't have a full packet yet...
		if ((offset + len) > Buffer.size())
			return;

		size_t packet_offset = offset;

		offset += len;
		if (!buffer_read(ExpectedFooter, offset))
			return;

		if (decrypt_packet(
			connection,
			&Buffer[packet_offset],
			len))
		{
			process_packet(
				connection,
				&Buffer[packet_offset],
				len);
		}
	}

	for (;;)
	{
		uint16_t header, footer;
		if (!buffer_read(header, offset))
			break;

		if (header != ExpectedHeader)
		{
			std::cerr
				<< "Unexpected header " << std::hex << header
				<< " (expected: " << ExpectedHeader << ")"
				<< std::dec
				<< " from " << (IsServerToClient ? "server" : "client")
				<< " packet"
				<< std::endl;

			// Header is wrong, we advanced by 2 to read this.
			// We'll backtrack 1 so next read will attempt to read it from the 2nd byte onwards, in case there's a stray byte.
			--offset;
			break;
		}

		// NOTE: This is always little endian (server order).
		if (!buffer_read(len, offset))
			break;

		// Don't have a full packet yet...
		if ((offset + len) > Buffer.size())
			break;

		size_t packet_offset = offset;

		offset += len;
		if (!buffer_read(footer, offset))
			break;

		if (footer != ExpectedFooter)
		{
			std::cerr
				<< "Unexpected footer " << std::hex << footer
				<< " (expected: " << ExpectedFooter << ")"
				<< std::dec
				<< " from " << (IsServerToClient ? "server" : "client")
				<< " packet"
				<< std::endl;
			break;
		}

		if (!decrypt_packet(
			connection,
			&Buffer[packet_offset],
			len))
			break;

		process_packet(
			connection,
			&Buffer[packet_offset],
			len);
	}

	// Anything we've read until this point should be removed from the buffer.
	if (offset > 0)
	{
		auto itr = Buffer.begin();
		std::advance(itr, offset);
		Buffer.erase(Buffer.begin(), itr);
	}
}

const char* Connection::ConnectionData::packet_type_for_log(PacketType packet_type)
{
	switch (packet_type)
	{
	case PacketType::Compressed_LZF:
		return " [Compressed - LZF]";

	case PacketType::Compressed_PKWARE:
		return " [Compressed - PKWARE]";

	case PacketType::Region:
		return " [Region]";

	default:
		return "";
	}
}

bool Connection::ConnectionData::decrypt_packet(
	Connection* connection,
	uint8_t* data,
	uint16_t& length)
{
	if (!IsJvCryptEnabled)
		return true;

	// Server => client
	if (IsServerToClient)
	{
		if (length < 6)
		{
			std::cerr << "Failed to decrypt packet from server - invalid length" << std::endl;
			return false;
		}

		Cryption.JvDecryptionFast(length, data, data);

		if (connection->_app->UseOldEncryptionHeaders())
		{
			if (data[0] != 0xfc)
			{
				std::cerr << "Failed to decrypt packet from server - invalid header" << std::endl;
				return false;
			}

			length -= 4;
			memmove(data, &data[4], length);
		}
		else
		{
			if (data[0] != 0xfc
				|| data[1] != 0x1e)
			{
				std::cerr << "Failed to decrypt packet from server - invalid header" << std::endl;
				return false;
			}

			length -= 5;
			memmove(data, &data[5], length);
		}
	}
	// Client => server
	else
	{
		if (length < 4)
		{
			std::cerr << "Failed to decrypt packet from client - invalid length" << std::endl;
			return false;
		}

		Cryption.JvDecryptionFast(length, data, data);

		if (connection->_app->UseOldEncryptionHeaders())
		{
			length -= sizeof(uint32_t);
			memmove(data, &data[4], length);
		}
		else
		{
			if (length < 8)
			{
				std::cerr << "Failed to decrypt packet from client - invalid length" << std::endl;
				return false;
			}

			length -= sizeof(uint32_t) + sizeof(uint32_t);
			memmove(data, &data[4], length);
		}
	}

	return true;
}

void Connection::ConnectionData::process_packet(
	Connection* connection,
	const uint8_t* data,
	int length,
	PacketType packet_type)
{
	if (!ReceivedFirstPacket)
	{
		fprintf(
			connection->_logHandle,
			"%s %s - %d byte(s)%s [ExpectedHeader=%X ExpectedFooter=%X]\n",
			LastTimestamp.c_str(),
			IsServerToClient ? "SERVER => CLIENT" : "CLIENT => SERVER",
			length,
			packet_type_for_log(packet_type),
			FAST_NTOHS(ExpectedHeader),
			FAST_NTOHS(ExpectedFooter));
		ReceivedFirstPacket = true;
	}
	else
	{
		fprintf(
			connection->_logHandle,
			"%s %s - %d byte(s)%s\n",
			LastTimestamp.c_str(),
			IsServerToClient ? "SERVER => CLIENT" : "CLIENT => SERVER",
			length,
			packet_type_for_log(packet_type));
	}

	if (length <= 0)
		return;

	const char* opcodeName = connection->_isLogin
		? login_opcode_to_name(data[0])
		: game_opcode_to_name(data[0]);
	if (*opcodeName != '\0')
	{
		fprintf(
			connection->_logHandle,
			"%s\n",
			opcodeName);
	}

	connection->append_packet_data(data, length);

	Packet pkt(data, length);
	size_t offset = 0;
	uint8_t opcode = 0;
	ASSERT(pkt.read(opcode, offset));

	// Server => client game connections
	if (IsServerToClient
		&& !connection->_isLogin)
	{
		switch (opcode)
		{
		case WIZ_COMPRESS_PACKET:
			decompress_packet(connection, &pkt, offset);
			break;

		case WIZ_VERSION_CHECK:
			s2c_version_check(connection, &pkt, offset);
			break;

		case WIZ_CONTINOUS_PACKET:
			s2c_continous_packet(connection, &pkt, offset);
			break;
		}
	}
}

void Connection::ConnectionData::decompress_packet(
	Connection* connection,
	Packet* pkt,
	size_t& offset)
{
	uint16_t compressed_length = 0, original_length = 0;
	uint32_t checksum = 0;

	// NOTE: This will only work for older clients.
	ASSERT(pkt->read(compressed_length, offset));
	ASSERT(pkt->read(original_length, offset));
	ASSERT(pkt->read(checksum, offset));

	PacketType packet_type = PacketType::Compressed_LZF;

	uint8_t* decompressed_data = nullptr;
	
	if (connection->_app->AlwaysUsePKWareCompression())
	{
		decompressed_data = Compression::PKWARE::Decompress(&pkt->Buffer[offset], compressed_length, original_length);
		if (decompressed_data == nullptr)
			return;

		packet_type = PacketType::Compressed_PKWARE;
	}
	else
	{
		decompressed_data = Compression::LZF::Decompress(&pkt->Buffer[offset], compressed_length, original_length);
		if (decompressed_data == nullptr)
		{
			decompressed_data = Compression::PKWARE::Decompress(&pkt->Buffer[offset], compressed_length, original_length);
			if (decompressed_data == nullptr)
				return;

			packet_type = PacketType::Compressed_PKWARE;
		}
	}
	
	process_packet(
		connection,
		decompressed_data,
		original_length,
		packet_type);

	delete[] decompressed_data;
}

void Connection::ConnectionData::s2c_version_check(
	Connection* connection,
	Packet* pkt,
	size_t& offset)
{
	uint16_t version = 0;
	uint64_t public_key = 0;

	ASSERT(pkt->read(version, offset));
	ASSERT(pkt->read(public_key, offset));

	// Client should enable these packets now
	enable_stock_crypto(public_key);

	// Server should enable encryption for all packets following this
	connection->_server.enable_stock_crypto(public_key);
}

void Connection::ConnectionData::s2c_continous_packet(
	Connection* connection,
	Packet* pkt,
	size_t& offset)
{
	uint16_t whole_size = 0;
	ASSERT(pkt->read(whole_size, offset));

	size_t offset2 = offset, offset_prev = 0;
	const size_t offset_end = offset + whole_size;
	while (offset2 < offset_end)
	{
		offset_prev = offset2;

		uint16_t packet_size = 0;
		ASSERT(pkt->read(packet_size, offset2));

		if (packet_size <= 0
			|| packet_size >= whole_size)
		{
			std::cerr << "Invalid WIZ_CONTINOUS_PACKET received from server" << std::endl;
			break;
		}

		process_packet(
			connection,
			&pkt->Buffer[offset2],
			packet_size,
			PacketType::Region);

		offset2 = offset_prev + packet_size + sizeof(uint16_t);
	}
}

void Connection::ConnectionData::enable_stock_crypto(uint64_t public_key)
{
	Cryption.SetPublicKey(public_key);
	Cryption.Init();
	IsJvCryptEnabled = true;
}

void Connection::append_packet_data(const uint8_t* data, int length)
{
	append_packet_data_to_file(_logHandle, data, length);
}

void Connection::append_packet_data_to_file(FILE* fp, const uint8_t* data, int length)
{
	fprintf(fp, "\n|------------------------------------------------|----------------|\n");
	fprintf(fp, "|00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F |0123456789ABCDEF|\n");
	fprintf(fp, "|------------------------------------------------|----------------|\n");

	int countpos = 0, line = 1;
	if (length > 0)
	{
		fprintf(fp, "|");
		for (int count = 0; count < length; count++)
		{
			if (countpos == 16)
			{
				countpos = 0;
				fprintf(fp, "|");

				for (int a = count - 16; a < count; a++)
				{
					if ((data[a] < 32) || (data[a] > 126))
						fprintf(fp, ".");
					else
						fprintf(fp, "%c", data[a]);
				}

				fprintf(fp, "|\n");
				line++;
				fprintf(fp, "|");
			}

			fprintf(fp, "%02X ", data[count]);

			if (count + 1 == length && length <= 16)
			{
				for (int b = countpos + 1; b < 16;	b++)
					fprintf(fp, "   ");

				fprintf(fp, "|");

				for (int a = 0; a < length; a++)
				{
					if ((data[a] < 32) || (data[a] > 126))
						fprintf(fp, ".");
					else
						fprintf(fp, "%c", data[a]);
				}

				for (int c = count; c < 15; c++)
					fprintf(fp, " ");

				fprintf(fp, "|\n");
			}

			if (count + 1 == length && length > 16)
			{
				for (uint32_t b = countpos + 1; b < 16; b++)
					fprintf(fp, "   ");

				fprintf(fp, "|");

				uint32_t print = 0;

				for (int a = line * 16 - 16; a < length; a++)
				{
					if ((data[a] < 32) || (data[a] > 126))
						fprintf(fp, ".");
					else
						fprintf(fp, "%c", data[a]);

					print++;
				}

				for (int c = print; c < 16; c++)
					fprintf(fp, " ");

				fprintf(fp, "|\n");
			}

			countpos++;
		}
	}
	fprintf(fp, "-------------------------------------------------------------------\n\n");
}

Connection::~Connection()
{
	if (_logHandle != nullptr)
	{
		fclose(_logHandle);
		_logHandle = nullptr;
	}
}

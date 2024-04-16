#pragma once

#include <pcapplusplus/Device.h>

class App
{
public:
	static constexpr uint16_t DEFAULT_PORT_LOGIN	= 15100;
	static constexpr uint16_t DEFAULT_PORT_GAME		= 15001;

	inline const std::string& GetLogFileName() const {
		return _logFileName;
	}

	inline const std::string& GetLogFileExt() const {
		return _logFileExt;
	}

	inline uint16_t GetDstPortLogin() const {
		return _dstPortLogin;
	}

	inline uint16_t GetDstPortGame() const {
		return _dstPortGame;
	}

	inline bool UseOldEncryptionHeaders() const {
		return _useOldEncryption;
	}

	inline bool AlwaysUsePKWareCompression() const {
		return _alwaysUsePKWareCompression;
	}

	App();
	int run(int argc, const char* argv[]);
	int parse_command_line(int argc, const char* argv[]);
	int load_pcapng();
	int capture();
	int analyse();
	void print_usage();

protected:
	std::string _pcapngFile;
	std::string _logFileName;
	std::string _logFileExt;
	std::string _interface;
	std::string _interfaceIP;
	std::string _dstIP;
	uint16_t _dstPortLogin = DEFAULT_PORT_LOGIN;
	uint16_t _dstPortGame = DEFAULT_PORT_GAME;
	bool _useOldEncryption = false;
	bool _alwaysUsePKWareCompression = false;

	pcpp::RawPacketVector _capturedPackets;
};

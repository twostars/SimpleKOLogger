#include "pch.h"
#include "App.h"
#include "Connection.h"

#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/Packet.h>
#include <pcapplusplus/PcapFileDevice.h>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/TcpLayer.h>

static HANDLE _consoleEventHandle = nullptr;
static BOOL WINAPI _ConsoleHandler(DWORD dwCtrlType);

App::App()
{
	// add npcap dir to Dll directory
	wchar_t szPath[_MAX_PATH + 1] = {};
	GetSystemDirectoryW(szPath, _countof(szPath));

	std::wstring szNpcapDir = szPath;
	szNpcapDir += L"\\Npcap";
	SetDllDirectoryW(szNpcapDir.c_str());
}

int App::run(int argc, const char* argv[])
{
	int r = parse_command_line(argc, argv);
	if (r != 0)
		return r;

	if (!_pcapngFile.empty())
		r = load_pcapng();
	else
		r = capture();

	if (r != 0)
		return r;

	r = analyse();
	if (r != 0)
		return r;

	return 0;
}

int App::parse_command_line(int argc, const char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "Missing arguments" << std::endl;
		return 1;
	}

	_pcapngFile.clear();
	_interface.clear();
	_dstIP.clear();
	_dstPortLogin = DEFAULT_PORT_LOGIN;
	_dstPortGame = DEFAULT_PORT_GAME;
	_logFileName = "packet_log.txt";
	_useOldEncryption = false;
	_alwaysUsePKWareCompression = false;

	for (int i = 1; i < argc;)
	{
		std::string arg = argv[i];

		if (arg == "-i")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing argument to " << arg << " [interface IP or name]" << std::endl;
				return 2;
			}

			_interface = argv[i + 1];
			i += 2;
			continue;
		}

		if (arg == "-o")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing argument to " << arg << " [log filename]" << std::endl;
				return 2;
			}

			_logFileName = argv[i + 1];
			i += 2;
			continue;
		}

		if (arg == "-d"
			|| arg == "--dst-ip")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing argument to " << arg << " [destination IP]" << std::endl;
				return 2;
			}

			_dstIP = argv[i + 1];
			i += 2;
			continue;
		}

		if (arg == "-g"
			|| arg == "--game-port")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing argument to " << arg << " [game port]" << std::endl;
				return 2;
			}

			_dstPortGame = (uint16_t) atoi(argv[i + 1]);
			i += 2;
			continue;
		}

		if (arg == "-l"
			|| arg == "--login-port")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing argument to " << arg << " [login port]" << std::endl;
				return 2;
			}

			_dstPortLogin = (uint16_t) atoi(argv[i + 1]);
			i += 2;
			continue;
		}

		if (arg == "-old"
			|| arg == "--old-encryption")
		{
			_useOldEncryption = true;
			++i;
			continue;
		}

		if (arg == "-pkware"
			|| arg == "--old-compression")
		{
			_alwaysUsePKWareCompression = true;
			++i;
			continue;
		}

		if (arg == "-pcapng")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing argument to " << arg << " [pcapng filename]" << std::endl;
				return 2;
			}

			_pcapngFile = argv[i + 1];
			i += 2;
			continue;
		}

		++i;
	}

	size_t extPos = _logFileName.rfind('.');
	if (extPos != std::string::npos)
	{
		_logFileExt = _logFileName.substr(extPos);
		_logFileName = _logFileName.substr(0, extPos);
	}
	else
	{
		_logFileExt = ".txt";
	}

	std::cout
		<< "Using old encryption headers: "
		<< (_useOldEncryption ? "TRUE" : "FALSE")
		<< std::endl;

	std::cout
		<< "Always use old (PKWARE) compression: "
		<< (_alwaysUsePKWareCompression ? "TRUE" : "FALSE")
		<< std::endl << std::endl;

	return 0;
}

int App::load_pcapng()
{
	pcpp::PcapNgFileReaderDevice reader(_pcapngFile);
	if (!reader.open())
	{
		std::cerr << "Error opening the PCAPNG capture file: " << _pcapngFile << std::endl;
		return 3;
	}

	reader.getNextPackets(_capturedPackets);
	reader.close();

	return 0;
}

int App::capture()
{
	bool isLoopbackCapture = false;
	if (_interface == "127.0.0.1"
		|| _interface == "localhost"
		|| _interface == "::1")
	{
		isLoopbackCapture = true;
		_interface = "\\Device\\NPF_Loopback";
	}

	auto dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIpOrName(_interface);
	if (dev == nullptr)
	{
		std::cerr << "Cannot find interface for address or name " << _interface << std::endl;
		return 3;
	}

	if (isLoopbackCapture)
		_interfaceIP = "127.0.0.1";
	else
		_interfaceIP = dev->getIPv4Address().toString();

	std::cout
		<< "Interface info:" << std::endl
		<< "   Interface IP:          " << _interfaceIP << std::endl
		<< "   Interface name:        " << dev->getName() << std::endl
		<< "   Interface description: " << dev->getDesc() << std::endl
		<< "   MAC address:           " << dev->getMacAddress() << std::endl
		<< "   Default gateway:       " << dev->getDefaultGateway() << std::endl
		<< "   Interface MTU:         " << dev->getMtu() << std::endl;

	if (!dev->getDnsServers().empty())
		std::cout << "   DNS server:            " << dev->getDnsServers().at(0) << std::endl;

	std::cout << std::endl;

	if (!dev->open())
	{
		std::cerr << "Cannot open device" << std::endl;
		return 4;
	}

	std::cout << "Starting capture..." << std::endl;
	std::cout << "Press CTRL+C to stop capturing." << std::endl;

	SetConsoleCtrlHandler(_ConsoleHandler, TRUE);

	_consoleEventHandle = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	if (_consoleEventHandle == nullptr)
	{
		std::cerr << "Failed to create listen event for waiting, stopping capture..." << std::endl;
		dev->close();
		return 5;
	}

	// Build filters

	// port == _dstPortGame || port == _dstPortLogin
	pcpp::PortFilter portFilterGame(_dstPortGame, pcpp::SRC_OR_DST);
	pcpp::PortFilter portFilterLogin(_dstPortLogin, pcpp::SRC_OR_DST);
	pcpp::OrFilter portFilter;
	portFilter.addFilter(&portFilterGame);
	portFilter.addFilter(&portFilterLogin);

	// proto == tcp
	pcpp::ProtoFilter protocolFilter(pcpp::TCP);

	// Merge filters
	// (port == _dstPortGame || port == _dstPortLogin) && (proto == tcp)
	pcpp::AndFilter andFilter;
	andFilter.addFilter(&portFilter);
	andFilter.addFilter(&protocolFilter);

	if (!_dstIP.empty())
	{
		pcpp::IPFilter ipFilter(_dstIP, pcpp::SRC_OR_DST);
		andFilter.addFilter(&ipFilter);
	}

	dev->setFilter(andFilter);
	dev->startCapture(_capturedPackets);

	WaitForSingleObject(_consoleEventHandle, INFINITE);

	std::cout << "Stopping capture..." << std::endl;
	dev->stopCapture();

	CloseHandle(_consoleEventHandle);

	std::string pcapngFilename = "capture_" + std::to_string(time(nullptr)) + ".pcapng";
	std::cout << "Writing " << _capturedPackets.size() << " captured packets to " << pcapngFilename << std::endl;

	pcpp::PcapNgFileWriterDevice pcapNgWriter(pcapngFilename);

	if (!pcapNgWriter.open())
	{
		std::cerr << "Cannot open " << pcapngFilename << " for writing" << std::endl;
		return 0; // normally we'd error, but this is more of a warning than an error so well let it slide
	}

	pcapNgWriter.writePackets(_capturedPackets);
	pcapNgWriter.close();

	return 0;
}

int App::analyse()
{
	std::cout << "Analysing " << _capturedPackets.size() << " captured packets...\n";

	size_t i = 0;
	for (const auto rawPacket : _capturedPackets)
	{
		std::cout << "Parsing raw packet " << (++i) << " / " << _capturedPackets.size() << std::endl;

		pcpp::Packet parsedPacket(rawPacket);

		auto tcpLayer = parsedPacket.getLayerOfType<pcpp::TcpLayer>();
		if (tcpLayer == nullptr)
			continue;

		auto ipv4Layer = parsedPacket.getLayerOfType<pcpp::IPv4Layer>();
		if (ipv4Layer == nullptr)
			continue;

		auto pktDstIP = ipv4Layer->getDstIPv4Address();
		auto pktSrcIP = ipv4Layer->getSrcIPv4Address();

		time_t local_tv_sec = rawPacket->getPacketTimeStamp().tv_sec;

		// Ensure we create the connection on the first packet.
		// This should be the client performing the initial SYN request, so we know which side is the client.
		auto connection = Connection::find_or_create(
			this,
			pktSrcIP.toString(),
			tcpLayer->getSrcPort(),
			pktDstIP.toString(),
			tcpLayer->getDstPort(),
			local_tv_sec);

		connection->process_raw_packet(parsedPacket);
	}

	return 0;
}

void App::print_usage()
{
	std::cerr << "Usage:   SimpleKOLogger -i [capture interface IP or name]" << std::endl;
	std::cerr << "OR:      SimpleKOLogger -pcapng [pcapng capture log filename]" << std::endl;

	std::cerr << "Optional arguments:" << std::endl;
	std::cerr << "-o [log filename, defaults to packet_log.txt]" << std::endl;
	std::cerr << "-d or --dst-ip [destination/server IP address]" << std::endl;
	std::cerr << "-g or --game-port [game port, defaults to 15001]" << std::endl;
	std::cerr << "-l or --login-port [login port, defaults to 15100]" << std::endl;
	std::cerr << "-old or --old-encryption [if supplied, old encryption headers are used]" << std::endl;
	std::cerr << "-pkware or --old-compression [if supplied, forces PKWARE to always be used, rather than as fallback]" << std::endl;

	system("pause");
}

static BOOL WINAPI _ConsoleHandler(DWORD dwCtrlType)
{
	SetEvent(_consoleEventHandle);
	Sleep(10'000); // Win7 onwards allows 10 seconds before it'll forcibly terminate
	return TRUE;
}

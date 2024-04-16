# SimpleKOLogger

Tool designed to externally capture and log packets for older Knight OnLine clients (roughly 1.298 and older).

This is primarily for research purposes. It is not intended to be used for developing hacks, and as such is not designed for custom protocols or protections used by servers.

It is designed to be used with official clients only.

## Requirements

[Npcap](https://npcap.com/) (the successor to WinPcap) must be installed to allow us to capture packets.

We also only support 64-bit builds, because of our precompiled build of [PcapPlusPlus](https://github.com/seladb/PcapPlusPlus).

## Usage

SimpleKOLogger must start capturing before a connection is established so that it is able to intercept the public encryption key and decrypt all future packets.

So before you run the game client, you will want to start it capturing.

It can either be run in live capture mode:
```
SimpleKOLogger -i [capture interface IP or name]
```

Or it can read its packets from an existing packet capture file:
```
SimpleKOLogger -pcapng [pcapng capture log filename]
```

Other optional arguments:
 - -o \[log filename\] - defaults to packet\_log.txt. Logs will be generated in files following the following naming scheme: \(log\_filename\)\_\(timestamp\)\_\(port\).\(extension\)
 - -d or --dst-ip \[destination/server IP address\] - used for further filtering the capture
 - -g or --game-port \[game port\] - defaults to 15001
 - -l or --login-port \[login port\] - defaults to 15100
 - -old or --old-encryption - if supplied, old encryption headers from 1.089-era clients are used. Otherwise, the newer encryption headers found in versions at least 1.298+ will be used.
 - -pkware or --old-compression - if supplied, forces PKWARE decompression to always be used. The default behaviour is to try LZF (used by versions at least 1.298+) and then fallback to PKWARE.

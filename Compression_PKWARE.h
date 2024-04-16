#pragma once

namespace Compression
{
	namespace PKWARE
	{
		uint8_t* Decompress(const uint8_t* in_data, uint32_t in_len, uint32_t original_len);
	}
}

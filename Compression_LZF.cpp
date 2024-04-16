#include "pch.h"
#include "Compression_LZF.h"
#include <lzf/lzf.h>

namespace Compression
{
	namespace LZF
	{
		uint8_t* Decompress(const uint8_t* in_data, uint32_t in_len, uint32_t original_len)
		{
			uint8_t* out_data = new uint8_t[original_len];
			uint32_t out_len = lzf_decompress(in_data, in_len, out_data, original_len);
			if (out_len != original_len)
			{
				delete[] out_data;
				return nullptr;
			}

			return out_data;
		}
	}
}

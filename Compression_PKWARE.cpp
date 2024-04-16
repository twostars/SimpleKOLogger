#include "pch.h"
#include "Compression_PKWARE.h"
#include <pkware/pklib.h>

namespace Compression
{
	namespace PKWARE
	{
		enum class BufferError
		{
			None = 0,
			OutputTooSmall,
			OutputTooSmallForBlock
		};

		struct UserContext
		{
			const uint8_t* Input = nullptr;
			uint32_t InputSize = 0;
			uint32_t InputOffset = 0;

			uint8_t* Output = nullptr;
			uint32_t OutputSize = 0;
			uint32_t OutputOffset = 0;

			TCmpStruct* State = nullptr;

			bool Compressing = false;		// true when compressing, false when decompressing
			BufferError Error = BufferError::None;

			UserContext()
			{
				State = new TCmpStruct();
			}

			~UserContext()
			{
				delete State;
			}
		};

		static const uint32_t DefaultCompressionType = CMP_BINARY;
		static const uint32_t DefaultDictionarySize = 4096;

		uint32_t ReadCallback(char* buffer, uint32_t* bytesRemaining_, void* param)
		{
			UserContext* ctx = (UserContext*) param;
			if (ctx == nullptr)
				return 0;

			uint32_t bytesRemaining = *bytesRemaining_; // we don't want to touch the original
			uint32_t bytesRead = 0;

			if (ctx->InputOffset < ctx->InputSize)
			{
				uint32_t bytesLeft = ctx->InputSize - ctx->InputOffset;
				if (bytesLeft < bytesRemaining)
					bytesRemaining = bytesLeft;

				memcpy(buffer, &ctx->Input[ctx->InputOffset], bytesRemaining);
				ctx->InputOffset += bytesRemaining;
				bytesRead = bytesRemaining;
			}

			return bytesRead;
		}

		void WriteCallback(char* buffer, uint32_t* size, void* param)
		{
			UserContext* ctx = (UserContext*) param;
			if (ctx == nullptr)
				return;

			if (ctx->OutputOffset >= ctx->OutputSize)
			{
				ctx->Error = BufferError::OutputTooSmall;
				return;
			}

			if ((ctx->OutputSize - ctx->OutputOffset) < *size)
			{
				ctx->Error = BufferError::OutputTooSmallForBlock;
				return;
			}

			memcpy(&ctx->Output[ctx->OutputOffset], buffer, *size);
			ctx->OutputOffset += *size;
		}

		uint8_t* Decompress(const uint8_t* in_data, uint32_t in_len, uint32_t original_len)
		{
			if (original_len == 0)
				return nullptr;

			uint8_t* out_data = new uint8_t[original_len];
			UserContext ctx{};

			ctx.Input = in_data;
			ctx.InputSize = in_len;
			ctx.Output = out_data;
			ctx.OutputSize = original_len;

			ctx.Compressing = false;

			int r = explode(ReadCallback, WriteCallback, (char*) ctx.State, &ctx);
			if (r != 0
				|| ctx.Error != BufferError::None)
			{
				delete[] out_data;
				return nullptr;
			}

			return out_data;
		}
	}
}

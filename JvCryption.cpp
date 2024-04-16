#include "pch.h"
#include "JvCryption.h"

uint64_t CJvCryption::s_private_key = 0x1234567890123456;

void CJvCryption::Init()
{
	m_tkey = m_public_key ^ s_private_key;
}

void CJvCryption::JvDecryptionFast(size_t len, const uint8_t* datain, uint8_t* dataout)
{
	uint8_t* pkey, lkey, rsk;
	int rkey = 2157;

	pkey = (uint8_t*) &m_tkey;
	lkey = (len * 157) & 0xff;

	for (size_t i = 0; i < len; i++)
	{
		rsk = (rkey >> 8) & 0xff;
		dataout[i] = ((datain[i] ^ rsk) ^ pkey[(i % 8)]) ^ lkey;
		rkey *= 2171;
	}
}

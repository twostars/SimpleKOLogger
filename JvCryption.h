#pragma once

class CJvCryption
{
private:
	uint64_t m_public_key;
	uint64_t m_tkey;

public:
	static uint64_t s_private_key;

	CJvCryption() : m_public_key(0)
	{
	}

	uint64_t GetPublicKey() const {
		return m_public_key;
	}

	void SetPublicKey(uint64_t key) {
		m_public_key = key;
	}

	void Init();
	void JvDecryptionFast(size_t len, const uint8_t* datain, uint8_t* dataout);
};

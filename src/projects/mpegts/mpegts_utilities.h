//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

// If performance of ov::ByteStream improves later, modify it to use ov::ByteStream in MPEG-TS Parsers
class MpegTsParseData
{
public:
	MpegTsParseData(const ov::Data *data)
		: MpegTsParseData(data->GetDataAs<const uint8_t>(), data->GetLength())
	{
	}

	MpegTsParseData(const uint8_t *data, size_t length)
		: _data(data),
		  _current(data),
		  _length(length),
		  _remained(length)
	{
	}

	inline void SetLength(size_t length)
	{
		_remained = length;
	}

	// Note: ReadXXBytes() API obtains the bits without considering _bit_offset
	inline uint8_t ReadByte()
	{
		if (_remained < 1ULL)
		{
			return 0U;
		}

		_remained--;
		return *(_current++);
	}

	inline uint16_t Read2Bytes()
	{
		if (_remained < 2ULL)
		{
			return 0U;
		}

		uint16_t value = ((_current[0] << 8) | _current[1]);

		_remained -= 2;
		_current += 2;

		return value;
	}

	inline uint24_t Read3Bytes()
	{
		if (_remained < 3ULL)
		{
			return 0U;
		}

		uint24_t value = ((_current[0] << 16) | (_current[1] << 8) | _current[2]);

		_remained -= 3;
		_current += 3;

		return value;
	}

	inline uint32_t Read4Bytes()
	{
		if (_remained < 4ULL)
		{
			return 0U;
		}

		uint32_t value = ((_current[0] << 24) | (_current[1] << 16) | (_current[2] << 8) | _current[3]);

		_remained -= 4;
		_current += 4;

		return value;
	}

	inline ov::String ReadString(size_t length)
	{
		length = std::min(length, _remained);

		ov::String str(reinterpret_cast<const char *>(_current), length);

		_remained -= length;
		_current += length;

		return std::move(str);
	}

	inline bool Skip(size_t length)
	{
		if (_remained < length)
		{
			return false;
		}

		_remained -= length;
		_current += length;

		return true;
	}

	inline bool SkipAll()
	{
		return Skip(_remained);
	}

	inline const uint8_t *GetCurrent() const
	{
		return _current;
	}

	// Obtain sequentially from the leftmost bit (MSB)
	//
	// For example,
	//
	// 10 (dec): 00001010 (bin)
	//           12345678 <= Read order
	inline uint8_t Read1()
	{
		if (_bit_offset == -1)
		{
			NextBits();
		}

		auto bit = OV_GET_BIT(_bits, _bit_offset);

		_bit_offset--;

		return bit;
	}

	inline uint8_t Peek8() const
	{
		if (_remained == 0ULL)
		{
			// There is no data
			return 0U;
		}

		return *_current;
	}

	inline uint8_t Read(int bits)
	{
		uint8_t value;

		if (bits > 8)
		{
			// Invalid bits
			OV_ASSERT2(bits <= 8);
			return 0U;
		}

		if (_bit_offset == -1)
		{
			NextBits();
		}

		if (((_bit_offset + 1) - bits) < 0)
		{
			if (_remained == 0ULL)
			{
				// There is no data
				return 0U;
			}

			// _bit_offset: 1
			// bits: 3
			//
			//  76543210  76543210
			// [aaaaaaaa][bbbbbbbb]
			//        ~   ~
			//        |   +- _bits_offset + bits
			//        +- _bit_offset

			// Obtain (_bit_offset + 1) bits from 'a' group
			int remained_bit = bits - (_bit_offset + 1);

			value = OV_GET_BITS(uint8_t, _bits, 0, (_bit_offset + 1)) << remained_bit;

			NextBits();

			// Obtain (bits - (_bit_offset + 1)) bits from 'b' group
			_bit_offset -= remained_bit;
			value |= OV_GET_BITS(uint8_t, _bits, _bit_offset + 1, remained_bit);
		}
		else
		{
			_bit_offset -= bits;
			value = OV_GET_BITS(uint8_t, _bits, (_bit_offset + 1), bits);
		}

		return value;
	}

	inline uint16_t ReadBits16(int bits)
	{
		OV_ASSERT2(bits <= 16);

		return (bits <= 8) ? Read(bits) : ((Read(bits - 8) << 8) | Read(8));
	}

	inline bool ReadBoolBit()
	{
		return static_cast<bool>(Read1());
	}

	inline size_t GetRemained() const
	{
		return _remained;
	}

	inline bool IsRemained(size_t bytes) const
	{
		return _remained >= bytes;
	}

	inline size_t GetRemainedBits() const
	{
		return (_remained * 8) + (_bit_offset + 1);
	}

	ov::String Dump() const
	{
		return std::move(ov::Dump(_current, _remained));
	}

protected:
	inline void NextBits()
	{
		if (_remained > 0ULL)
		{
			_bits = ReadByte();
			_bit_offset = 7;
		}
	}

	const uint8_t *_data = nullptr;
	const uint8_t *_current = nullptr;
	int _bit_offset = -1;
	uint8_t _bits = 0U;
	size_t _length = 0ULL;
	size_t _remained = 0ULL;
};

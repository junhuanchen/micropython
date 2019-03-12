#include "BaseSeven.h"
#include <string.h>

// 编码数据转换结构
union BSVar
{
	uint16_t bit16;
	struct TwoChar
	{
		uint8_t right, left;
	}bit08;
};

// 同步修改索引进行块编码。（由宏改）
static void BsEncode(uint8_t *Buf[], uint8_t *Src[])
{
	union BSVar var = { 0 };
	for (uint8_t shift = 1; 8 != shift; (*Buf)++, (*Src)++, shift++)
	{
		var.bit08.right = **Src, var.bit16 <<= 8 - shift;
		**Buf = var.bit08.left | 0x80, var.bit16 <<= shift;
	}
	**Buf = var.bit08.left | 0x80;
}

uint8_t BaseSevenEncode(uint8_t Buf[], uint32_t BufLen, uint8_t Src[], uint32_t SrcLen)
{
	// 判断编码缓冲区是否可容纳数据源
	if ((BufLen / 8) * 7 >= SrcLen)
	{
		while (SrcLen >= 7)
		{
			BsEncode(&Buf, &Src);
			// 准备下一个编码块
			Buf++, SrcLen -= 7;
		}
		// 处理不足一个单位的部分
		if (SrcLen)
		{
			uint8_t TmpBuf[7] = { '\0' }, *TmpSrc = TmpBuf;
			memcpy(TmpBuf, Src, SrcLen);
			BsEncode(&Buf, &TmpSrc);
		}
		return 1;
	}
	return 0;
}

uint8_t BaseSevenDecode(uint8_t Buf[], uint32_t BufLen, uint8_t Src[], uint32_t SrcLen)
{
	if ((BufLen / 8) * 7 <= SrcLen)
	{
		// 数据位移长度值
		for (union BSVar var; BufLen; BufLen -= 8)
		{
			// 以8字节为解码单位长度
			var.bit08.left = (*Buf++);
			for (uint8_t shift = 1; 8 != shift; Buf++, Src++, shift++)
			{
				if (!(*Buf & 0x80)) return 0; // 异常编码值
				var.bit08.right = *Buf << 1, var.bit16 <<= shift;
				*Src = var.bit08.left, var.bit16 <<= 7 - shift;
			}
		}
		return 1;
	}
	return 0;
}

// 单元测试
#ifdef UNIT_TEST

#include <assert.h>
#include <string.h>

#define TestStr "0123456789ab"

static RsaType main()
{
	char Buf[16], Src[14];
	assert(1 == BaseSevenEncode(Buf, sizeof(Buf), TestStr, sizeof(TestStr)));
	assert(1 == BaseSevenDecode(Buf, sizeof(Buf), Src, sizeof(Src)));
	assert(0 == memcmp(Src, TestStr, sizeof(TestStr)));

	memset(Buf, 0, sizeof(Buf)), memset(Src, 0, sizeof(Src));
	assert(1 == BaseSevenEncode(Buf, sizeof(Buf), TestStr, sizeof(TestStr)));
	Buf[1] = 0x7F;
	assert(0 == BaseSevenDecode(Buf, sizeof(Buf), Src, sizeof(Src)));
	assert(0 != memcmp(Src, TestStr, sizeof(TestStr)));
	return 0;
}
#endif

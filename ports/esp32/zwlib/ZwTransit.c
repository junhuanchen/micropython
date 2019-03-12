#include "ZwTransit.h"

void ZwTranInit(void)
{
	CrcInit();
}

#include "Rsa.h"
#define		CIPHER_MAX		0xFF // 密文原始数据长度

static void ZwTranCrypt(uint8_t String[], uint8_t Crypt)
{
    for (; *String; String++)
    {
        if (CIPHER_MAX != *String)
        {
            *String = RsaCrypt(*String, Crypt, CIPHER_MAX);
        }
    }
}

uint8_t ZwTranCheck(uint8_t Buf[], uint8_t Len)
{
	return CrcFast(Buf, Len);
}

static void ZwTranSetCheck(uint8_t Buf[], uint8_t Len)
{
	Buf[Len] = ZwTranCheck(Buf, Len);
}

// 初始化编码包并赋值静态数据。
void ZwEncodeInit(ZwEncode * Self, uint8_t Crypt, uint8_t EntID, uint8_t DevID[], uint8_t DevIP, ExternGetTime Time)
{
	// 初始化为零
	memset(Self, '\0', sizeof(*Self));
	// 编码区静态数据
	memcpy(Self->Zip.EntID, &EntID, sizeof(Self->Zip.EntID));
	memcpy(Self->Zip.DevID, DevID, sizeof(Self->Zip.DevID));
	memcpy(Self->Zip.DevIP, &DevIP, sizeof(Self->Zip.DevIP));
	// 函数以及相关数据接口
	Self->Time = Time;
	Self->Crypt = Crypt;
}

static void ZwEncodeCore(ZwEncode * Self, ZwEncodeType Type, uint8_t Pack[])
{
	// 设置数据包类型
	Pack[0] = Type;
	// 获取时间值并设置到二进制编码区
	Self->Time((uint32_t *) Self->Zip.DevTm, (uint16_t *) Self->Zip.DevMs);
	// 对二进制数据进行编码
	BaseSevenEncode(Pack + sizeof(Type), ZwBinaryLen, (uint8_t *)&Self->Zip, sizeof(Self->Zip));
}

bool ZwEncodeGetDevIP(uint8_t Pack[], uint8_t PackLen, uint8_t *DevIP)
{
	if (((PackLen < ZwTranMax) && (0 == ZwTranCheck(Pack, PackLen))))
	{
		// 解码部分核心数据
		uint8_t tmp[BsDecodeLen];
		// 根据结构，设备IP在编码区第一部分。
		if (BaseSevenDecode(Pack + ZwTranTypeLen, BsDecodeLen, tmp, sizeof(tmp)))
		{
			// 获得DevIP数据
			memcpy(DevIP, tmp, sizeof(*DevIP));
			return true;
		}
	}
	return false;
}

bool ZwEncodeSetDevIP(uint8_t Pack[], uint8_t PackLen, uint8_t DevIP)
{
	if (((PackLen < ZwTranMax) && (0 == ZwTranCheck(Pack, PackLen))))
	{
		// 编码缓冲区
		uint8_t tmp[BsEncodeLen];
		// 根据协议，数据IP在编码区第一部分，数据解码。
		if (BaseSevenDecode(Pack + ZwTranTypeLen, BsDecodeLen, tmp, sizeof(tmp)))
		{
			// 修改DevIP数据
			memcpy(tmp, &DevIP, sizeof(DevIP));
			// 重新写入编码数据
			if (BaseSevenEncode(Pack + ZwTranTypeLen, BsDecodeLen, tmp, sizeof(tmp)))
			{
				// 重新设置包尾的校验码
				ZwTranSetCheck(Pack, PackLen - ZwTranCrcLen);
				return true;
			}
		}
	}
	return false;
}

uint8_t ZwEncodeCollect(ZwEncode * Self, uint8_t Pack[], uint8_t SrcLen, uint8_t Src[], uint8_t DataLen, uint8_t Data[])
{
	uint8_t pack_len = (ZwEncodeCoreLen + sizeof(SrcLen) + sizeof(DataLen)) + SrcLen + DataLen - 1;
	if (pack_len < ZwTranMax && DataLen <= ZwDataMax)
	{
		// 编码核心数据
		ZwEncodeCore(Self, ZwTranTypeCollect, Pack);
		// 加密封包数据
		uint8_t * pack = Pack + ZwTranTypeLen + ZwBinaryLen;
		*pack = SrcLen, pack += sizeof(SrcLen);
		memcpy(pack, Src, SrcLen), pack += SrcLen;
		*pack = DataLen, pack += sizeof(DataLen);
		memcpy(pack, Data, DataLen), pack[DataLen] = '\0';
		ZwTranCrypt(Pack + ZwTranTypeLen + ZwBinaryLen, Self->Crypt);
		// 计算当前数据校验码并添加在末位
		ZwTranSetCheck(Pack, pack_len - ZwTranCrcLen);
		Pack[pack_len] = '\0';
		return pack_len;
	}
	return 0;
}

uint8_t ZwEncodeCommand(ZwEncode * Self, uint8_t Pack[], uint8_t CmdLen, uint8_t Cmd[])
{
	uint8_t pack_len = ZwEncodeCoreLen + sizeof(CmdLen) + CmdLen - 1;
	if (pack_len < ZwTranMax)
	{
		// 编码核心数据
		ZwEncodeCore(Self, ZwTranTypeCommand, Pack);
		// 加密封包数据
		uint8_t * pack = Pack + ZwTranTypeLen + ZwBinaryLen;
		*pack = CmdLen, pack += sizeof(CmdLen);
		memcpy(pack, Cmd, CmdLen), pack[CmdLen] = '\0';
		ZwTranCrypt(Pack + ZwTranTypeLen + ZwBinaryLen, Self->Crypt);
		// 计算当前数据校验码并添加在末位
		ZwTranSetCheck(Pack, pack_len - ZwTranCrcLen);
		Pack[pack_len] = '\0';
		return pack_len;
	}
	return 0;
}

// 初始化解包操作所需结构
void ZwDecodeInit(ZwDecode *Self, uint8_t Crypt)
{
	memset(Self, '\0', sizeof(*Self));
	Self->Crypt = Crypt;
}

// 数据校验、解码、解密框架，返回Crypt区索引。
uint8_t* ZwDecodeCore(ZwDecode * Self, uint8_t *Pack, uint8_t PackLen)
{
	if (PackLen < ZwTranMax)
	{
		if (0 == ZwTranCheck(Pack, PackLen))
		{
			Pack[PackLen - 1] = '\0'; // 移除校验码
			if (BaseSevenDecode(Pack + ZwTranTypeLen, ZwBinaryLen, (uint8_t *)&Self->Zip, sizeof(Self->Zip)))
			{
				Pack += ZwTranTypeLen + ZwBinaryLen;
				ZwTranCrypt(Pack, Self->Crypt);
				return Pack;
			}
			else
			{
				// LogOut:编码区数据异常。
				PutInfo("UnPackCoreCheck : 编码区数据异常\n");
			}
		}
		else
		{
			// LogOut:校验值数据异常。
			PutInfo("UnPackCoreCheck : 校验值数据异常\n");
		}
	}
	else
	{
		// LogOut:数据超过缓冲区。
		PutInfo("UnPackCoreCheck : 数据超过缓冲区\n");
	}
	return NULL;
}

bool ZwDecodeCollect(uint8_t String[], uint8_t *SrcLen, uint8_t Src[], uint8_t *DataLen, uint8_t Data[])
{
	// 取出Crypt区数据
	if (String[0] <= ZwSourceMax)
	{
		*SrcLen = String[0];
		memcpy(Src, String + sizeof(*SrcLen), *SrcLen);
		String += *SrcLen + sizeof(*SrcLen);
		if (String[0] <= ZwDataMax)
		{
			*DataLen = String[0];
			memcpy(Data, String + sizeof(*DataLen), *DataLen);
			return true;
		}
	}
	return false;
}

bool ZwDecodeCommand(uint8_t String[], uint8_t * CmdLen, uint8_t Cmd[])
{
	if (String[0] <= ZwCmdMax)
	{
		*CmdLen = String[0];
		memcpy((char *) Cmd, (char *)String + sizeof(*CmdLen), *CmdLen);
		return true;
	}
	return false;
}

#ifdef UNIT_TEST
#include <assert.h>

#include <time.h>
#include <stdlib.h>

void GetTime(uint32_t *sec, uint16_t *ms)
{
	*sec = time(NULL), *ms = rand();
}

void UnitTestZwTransit()
{
	const uint8_t RsaDe = 43, RsaEn = 3;
	ZwTranInit();
	ZwEncode En;
	ZwEncodeInit(&En, RsaDe, 123, "0123456789ABCDEF", 21, GetTime);
	ZwDecode De;
	ZwDecodeInit(&De, RsaEn);
	time_t NTimeTm, LTimeTm;
	uint16_t NTimeMs, LTimeMs;

	// 以下测试指令包

	uint8_t buffer[ZwTranMax], buflen, *String;

#define __command "0123456789ABCDEF0123456789ABCDEF0123456789AB"

	assert(strlen(__command) == ZwCmdMax);
	buflen = ZwEncodeCommand(&En, buffer, ZwCmdMax, __command);
	assert(buflen == ZwTranMax - 1);

	LTimeTm = *(uint32_t *) En.Zip.DevTm;
	LTimeMs = *(uint32_t *) En.Zip.DevMs;

	uint8_t Command[ZwContentMax], CmdLen;

	String = ZwDecodeCore(&De, buffer, buflen);
	assert(NULL != String);

	assert(ZwDecodeCommand(String, &CmdLen, Command));
	assert(CmdLen == ZwCmdMax && !memcmp(Command, __command, CmdLen));

	NTimeTm = *(uint32_t *) De.Zip.DevTm;
	NTimeMs = *(uint32_t *) De.Zip.DevMs;
	assert(NTimeTm == LTimeTm && NTimeMs == LTimeMs);

	// 以下测试采集包
	uint8_t pack_len = ZwEncodeCollect(&En, buffer, 8, "DM000000", 20, "12345123451234512345");
	assert(pack_len == strlen(buffer)); // 意味着这是一个字符串

	LTimeTm = *(uint32_t *) En.Zip.DevTm;
	LTimeMs = *(uint32_t *) En.Zip.DevMs;

	// 未解包时设置和获取设备地址
	uint8_t DevIP;
	ZwEncodeGetDevIP(buffer, pack_len, &DevIP);
	assert(DevIP == 21);
	ZwEncodeSetDevIP(buffer, pack_len, 45);
	ZwEncodeGetDevIP(buffer, pack_len, &DevIP);
	assert(DevIP == 45);

	uint8_t Src[8] = "DM000000", SrcLen = 8;
	uint8_t Data[20], DataLen;

	// 解包时获取所有数据
	String = ZwDecodeCore(&De, buffer, pack_len);
	assert(NULL != String);

	// assert(true == ZwEncodeCollectCmpSrc(&En, buffer, pack_len, SrcLen, Src));

	assert(ZwDecodeCollect(String, &SrcLen, Src, &DataLen, Data));
	assert(SrcLen == 8 && !memcmp(Src, "DM000000", SrcLen));
	assert(DataLen == 20 && !memcmp(Data, "12345123451234512345", DataLen));
	assert(*((uint8_t *) De.Zip.DevIP) == 45);
	assert(*((uint8_t *) De.Zip.EntID) == 123);
	assert(!memcmp(De.Zip.DevID, "0123456789ACBDEFG", sizeof(De.Zip.DevID)));

	NTimeTm = *(uint32_t *) De.Zip.DevTm;
	NTimeMs = *(uint32_t *) De.Zip.DevMs;
	assert(NTimeTm == LTimeTm && NTimeMs == LTimeMs);

	// char TimeStr[20];
	// strftime(TimeStr, sizeof(TimeStr), "%Y-%m-%d %H:%M:%S", localtime(&NTimeTm));
}

int main()
{
	UnitTestZwTransit();
}

#endif

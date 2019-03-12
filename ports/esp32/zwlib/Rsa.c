
#include "Rsa.h"

RsaVar RsaCrypt(RsaVar Data, RsaVar Crypt, RsaVar Max)
{
	return CryptFastPowMod(Data, Crypt, Max);
}

#ifdef UNIT_TEST

#include "stdio.h"

// 初始化RsaVar模块，生成对称密钥，密钥变量须外部提供。
static void RsaCryptInit(RsaVar Max, RsaVar * Encrypt, RsaVar * Decrypt)
{
	// N 数据规模， En = φ(N)（欧拉函数结果）， Encrypt 与 Decrypt 分别为两把密钥
	RsaVar N = Max, En = CryptEular(N);
	if (*Encrypt < 2) *Encrypt = 2;
	// 生成__RSA_H__参数
	do
	{
		// 选取 Encrypt 与 En 互质的数
		while (1 != CryptGcd(En, *Encrypt))
			*Encrypt += 1;
		// 求解其乘法逆元，若 Decrypt == -1 则表示 En 与 Encrypt 不互质
		*Decrypt = CryptEgcdInverse(*Encrypt, En);
	} while (-1 == *Decrypt);
	// 打印密钥参数
	printf("En:%llu\n", En);
	printf("Encrypt:%llu\n", *Encrypt);
	printf("Decrypt:%llu\n", *Decrypt);
}

#include <stdlib.h>
#include <assert.h>

#include <stdio.h>

int main()
{
	// N 数据规模， En = φ(N)（欧拉函数结果），Encrypt 与 Decrypt 分别为两把密钥
	RsaVar N = UINT8_MAX, Encrypt = 7, Decrypt;
	RsaCryptInit(N, &Encrypt, &Decrypt);

	// 若陷入死循环必然是因为没能还原回原数据导致无法进入下一索引(++)，可自行测试大于N的数据规模
	// Test Encrypt Key
	RsaVar src, tmp;
	for (src = 0; src != N; src++)
	{
		tmp = RsaCrypt(src, Encrypt, N);
		tmp = RsaCrypt(tmp, Decrypt, N);
		assert(src == tmp);
		printf("%02hhX\n", src);
	}
	// Test Decrypt Key
	for (src = 0; src != N; src++)
	{
		tmp = RsaCrypt(src, Decrypt, N);
		tmp = RsaCrypt(tmp, Encrypt, N);
		assert(src == tmp);
		printf("%02hhX\n", src);
	}
	return 0;
}

#endif

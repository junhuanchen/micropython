
#ifndef __CRYPT_H__
#define __CRYPT_H__

#ifndef __IAR_SYSTEMS_ICC__
#include <stdint.h>
#else
#include "HardWare.h"
#endif

// Rsa 运算一定是无符号的整数。
// Ecc 运算一定是有符号的（因为有负元）。
// 注：模幂函数只支持无符号的。
// 负数是有符号，反之为无符号。
// 密码（运算）安全宏定义中的汇编不一定通用，尤其是64位环境下要对应移植。
#define ECC_RSA_8
#ifdef ECC_RSA_8
// 目前使用的 RSA 配置
typedef int8_t CryptInt;
typedef uint8_t CryptUint;
#elif defined(ECC_RSA_64)
typedef int64_t CryptInt;
typedef uint64_t CryptUint;
// #define CRYPT_SUB_SAFE -64
// #define CRYPT_ADD_SAFE 64
#else
#error "select a config"
#endif

// Montgomery运算函数指针
typedef CryptUint(*CryptMontgomeryPointer)(CryptUint, CryptUint, CryptUint);

// return (a - b) % mod 定义宏可以确保运行安全。
static CryptInt CryptSafeSubMod(CryptInt a, CryptInt b, CryptInt mod)
{
#ifdef CRYPT_SUB_SAFE
	// 取出标记寄存器
	unsigned short flag = 0;
	// 运算结果
	CryptInt result;
	// 作取出汇编代码用
	// result = (a - b);
#if (CRYPT_SUB_SAFE == -32) // 有符号32位整型
	__asm
	{
		mov         eax, dword ptr[a]
		sub         eax, dword ptr[b]
		pushf
		pop flag
		mov         dword ptr[result], eax
	}
#elif  (CRYPT_SUB_SAFE == -64)
	// /Z7、/Zi、/ZI（调试信息格式）不同会产生不同的汇编代码，这部分汇编不通用。
	// result = (a - b);
	__asm
	{
		mov         ecx, dword ptr[a]
		sub         ecx, dword ptr[b]
		mov         edx, dword ptr[ebp + 0Ch]
		sbb         edx, dword ptr[ebp + 14h]
		pushf
		pop			flag
		mov         dword ptr[result], ecx
		mov         dword ptr[ebp - 8], edx
	}
	//__asm
	//{
	//	mov         eax, dword ptr[a]
	//	sub         eax, dword ptr[b]
	//	mov         ecx, dword ptr[ebp + 0Ch]
	//	sbb         ecx, dword ptr[ebp + 14h]
	//	pushf
	//	pop			flag
	//	mov         dword ptr[result], eax
	//	mov         dword ptr[ebp - 14h], ecx
	//}
#else
	result = (a - b);
#endif

	// 判断是否溢出，非寄存器汇编形式则判断做差后符号位。
	// a > 0 && b < 0 && res < 0(或直接检查符号位) => 上溢出
	// a < 0 && b > 0 && res > 0(或直接检查符号位) => 下溢出
	// 当状态寄存器存在OF进位标记的时候0x800 == (flag & 0x800)。
	if (0x800 == (flag & 0x800))
	{
		// a != 0，否则怎会溢出。
		// a - b 可能 上溢出或下溢出
		// 推断 a > 0 => b < 0 才能上溢出
		// 推断 a < 0 => b > 0 才能下溢出
		b += (a > 0) ? mod : -mod;
		result = (a - b);
	}
	// 有符号的小于零修正回正整数,因为外部PowMod的底数和幂指数均为正数。
	while (result < 0) result += mod;
	return result;

#else

	CryptInt result = (a - b) % mod;
	while (result < 0) result += mod;
	return result % mod;

#endif
}

static CryptUint CryptSafeAddMod(CryptUint a, CryptUint b, CryptUint mod)
{
#ifdef CRYPT_ADD_SAFE
#if (CRYPT_ADD_SAFE < 0)
	// 假设已知是有符号参数
	// 可以利用c >= a, c >= b => a + b <= 2*c
	// 其中2*c即为无符号数的最大值（符号位）。
	CryptUint m = (a + b);
	while (m > mod) m -= mod;
	return m;
#else  // 假设已知是有符号参数，读取FLAG寄存器CF值判断是否溢出，后将其纠正。
	// 由于(a + a)中a为最大值结合时会导致数据溢出，溢出值保留余数为0，位状态保存在CF寄存器，
	// 但实际上通过mod操作后应该回滚小于其最大值，所以该加法对这一个情况进行调整。
	// 取出标记寄存器
	unsigned short flag = 0;
	// 运算结果
	CryptInt result;
	// result = (a + b);
#if (CRYPT_ADD_SAFE == 32)
#define TypeMax UINT32_MAX
	__asm
	{
		mov         eax, dword ptr[a]
			add         eax, dword ptr[b]
			pushf
			pop flag
			mov         dword ptr[result], eax
	}
#elif (CRYPT_ADD_SAFE == 64)
#define TypeMax UINT64_MAX
	// result = (a + b);
	__asm
	{
		mov         ecx, dword ptr[a]
		add         ecx, dword ptr[b]
		mov         edx, dword ptr[ebp + 0Ch]
		adc         edx, dword ptr[ebp + 14h]
		pushf
		pop			flag
		mov         dword ptr[result], ecx
		mov         dword ptr[ebp - 8], edx
	}
	
#elif ((CRYPT_ADD_SAFE == 16) || (CRYPT_ADD_SAFE == 8))
#define TypeMax 0
	// 实践发现状态寄存器CF位并未变化
	// 无符号也无法判断符号位，此时只能强转更大类型容纳数据。
	result = ((size_t) a + b) % mod;
	return result;
#else 
#error "set a type W"
#endif
	// 当状态寄存器存在CF进位标记的时候1 == (flag & 1)。
	if (1 == (flag & 1))
	{
		// a + b > TypeMax, (TypeMax - a - b) < 0, 将其为转正即可
		return -(TypeMax - a - b);
	}
	return result % mod;
#endif
#else 
	// 默认操作
	return ((a + b) % mod);
#endif
}

// Montgomery 二进制算法框架
static void CryptMontgomeryFrame(CryptUint *ans, CryptMontgomeryPointer Func, CryptUint a, CryptUint b, CryptUint mod)
{
WHILE:
	if (b & 1)// LSB位为 1
	{
		b--, *ans = Func(*ans, a, mod);
	}
	b >>= 1;
	if (0 == b)
	{
		return ;
	}
	a = Func(a, a, mod);
	goto WHILE;
}

static CryptUint CryptFastMulMod(CryptUint a, CryptUint b, CryptUint mod)
{
	CryptUint ans = 0;
	CryptMontgomeryFrame(&ans, CryptSafeAddMod, a, b, mod);
	return ans;
}

// 幂模运算a^b%k（纪念版）
// 通常思维暴力求解版本（请不要使用该版本，不然会哭的）
static CryptUint NormalPowMod(CryptUint a, CryptUint b, CryptUint mod)
{
	CryptUint ans = 1;
	while (b--) ans *= a;
	return ans % mod;
}

// 幂模运算a^b%k（纪念版）
// （也请不要使用该版本，不然也会哭的，递归溢出）
// 递归版本 a ^ b (mod mod)
// 模运算结合律：(a *a) mod mod =( (a mod mod) *a ) mod mod
// C 语言表达：((a *b) % p = (a % p *b) % p)
static CryptUint RecursionPowMod(CryptUint a, CryptUint b, CryptUint mod)
{
	return (b ? (a *RecursionPowMod(a, b - 1, mod)) : 1) % mod;
}

static CryptUint CryptFastPowMod(CryptUint a, CryptUint b, CryptUint mod)
{
	CryptUint ans = 1;
	CryptMontgomeryFrame(&ans, CryptFastMulMod, a, b, mod);
	return ans;
}

//  费马小定理求解乘法逆元：假如p是质数，且gcd(a,p)=1，那么 a^(p?1) ≡ 1 (mod p)。
// 所以对于a的逆元x，有ax≡1(mod p)，当a与p互质时，推出x = a^(p-2),即为其乘法逆元。
// 实践中发现其实现的在与拓展欧几里何求逆的运算比较其效率并不高，尤其是n极大的时候。
static CryptInt CryptFermatInverse(CryptInt a, CryptInt n)
{
	return CryptFastPowMod(a, n - 2, n);
}

#include <stdio.h>

// 二进制求逆：前提是 p 是素数，且大于二，a 属于[1, p-1]。
// 已知前提：p 为质数，大于二的质数为一定是奇数。
// 解：a * x1 + p * y1 = u mod p ①
// 	  a * x2 + p * y2 = v mod p ②
// 当 u = 1 或 v = 1 时，x1 或 y1 将为逆元。
// 可知 ① 或 ② 的 u 与 p 满足偶数时，其式可除二。
// 例如：若 u 为偶数除二，直到除为一时，a * x1 均可除二，如果x1为偶数则除二
// 如果x1为奇数，则可以p*(y1 - 1)借奇数合并为偶数进行除二，直到u为奇数。
// 同样的，v也满足上述转换，直到v为奇数。
// 当 u 与 v 均为奇数时，将做差改变较大值一方为偶数，此时又可以回到上述进行偶数转换为奇数
// 直到某一方满足了等于一，则说明此时的x已经满足 a * x + p * y = 1, y 可以为任意整数，故a * x = 1 mod p。
// 解得其逆元x。
static CryptInt CryptBinaryInverse(CryptInt a, CryptInt n)
{
	CryptInt u = a, v = n;
	CryptInt x1 = 1, x2 = 0;
	while (1 != u && 1 != v)
	{
		while ((u & 1) == 0)
		{
			u >>= 1;
			x1 = ((x1 & 1) == 0) ? x1 >> 1 : (x1 + n) >> 1;
		}
		while ((v & 1) == 0)
		{
			v >>= 1;
			x2 = ((x2 & 1) == 0) ? x2 >> 1 : (x2 + n) >> 1;
		}
		if (u >= v)
		{
			u = u - v, x1 = x1 - x2;
		}
		else
		{
			v = v - u, x2 = x2 - x1;
		}
	}
	if (1 == u)
	{
		while (x1 < 0) x1 += n;
		return x1;
	}
	else
	{
		while (x2 < 0) x2 += n;
		return x2;
	}
	// return (1 == u) ? x1 % n : x2 % n;
}

// 欧拉函数：φ(x) = x ∏ (1-1/p)（P是数N的质因数）
static CryptUint CryptEular(CryptUint n)
{
	CryptUint ret = 1;
	for (CryptUint i = 2; i * i <= n; i++)
	{
		if (0 == n % i)
		{
			n /= i, ret *= i - 1;
			while (0 == n % i)
			{
				n /= i, ret *= i;
			}
		}
	}
	if (n > 1)
	{
		ret *= n - 1;
	}
	return ret;
}

// 欧几里何函数：Gcd(a, b）= Gcd(b, a%b)
static CryptUint CryptGcd(CryptUint a, CryptUint b)
{
	return 0 == b ? a : CryptGcd(b, a % b);
}

// 拓展欧几里何函数：a*x + b*y = gcd(a, b)
//	  gcd(a, b) = gcd(b, a%b)
//    a%b = a-(a/b)*b
//    a*x + b*y = gcd(a, b) = gcd(b, a%b) = b*x1 + (a-(a/b)*b)*y1
//        = b*x1 + a*y1–(a/b)*b*y1
//        = a*y1 + b*(x1–a/b*y1)
static CryptInt CryptEgcd(CryptInt a, CryptInt b, CryptInt *x, CryptInt *y)
{
	CryptInt result, tmp;
	// 递归终止条件
	if (0 == b)
	{
		*x = 1, *y = 0;
		return a;
	}
	result = CryptEgcd(b, a % b, x, y);
	tmp = *x, *x = *y;
	// 可以考虑乘法优化
	*y = tmp - (a / b) * (*y);
	return result;
}

// 求解乘法逆元函数 a * (return value) ≡ 1 (mod n)。
// 前提是两者互为质素，通过拓展欧几里何来算。
// 可以有效的降低a与n的运算量，比在n较大的时候比费马小定理更有效。
static CryptInt CryptEgcdInverse(CryptInt a, CryptInt n)
{
	CryptInt x = 0, y = 0;
	if (CryptEgcd(n, a, &x, &y) != 1)
	{
		return 0;
	}
	// 确保余数与被除数符号一致
	// if (a < 0) y = -y; // 负数取模有正负模，默认为无符号取模。
	// while (y > n) y -= n;// y %= n; // 修正y值回模域。
	if (y > n) y %= n; // 修正y值回模域。
	if (y < 0) y += n;
	return y;
}

#include <stdlib.h>

//生成[ 0 , n ]的随机数
static CryptUint CryptRandom(CryptUint n)
{
	return (CryptUint) ((double) rand() / RAND_MAX * n + 0.5);
}

//miller_rabin算法的判断元素
static uint8_t CryptWitness(CryptUint a, CryptUint n)
{
	//用检验算子a来检验n是不是素数
	CryptUint tmp = n - 1;
	uint8_t len = 0;

	while (tmp % 2 == 0)
	{
		tmp /= 2, len++;
	}

	//将n-1拆分为a^r * s
	CryptUint x = CryptFastPowMod(a, tmp, n); //得到a^r mod n
	//余数为1则为素数
	if (x == 1 || x == n - 1)
	{
		return 1;
	}
	//否则试验条件2看是否有满足的 j
	while (len--)
	{
		x = CryptFastMulMod(x, x, n);
		if (x == n - 1)
		{
			return 1;
		}
	}
	return 0;
}

//检验n是否是素数
static uint8_t CryptMillerRabin(CryptUint n)
{
	if (n == 2)
	{
		return 1;
	}

	//如果是2则是素数，如果<2或者是>2的偶数则不是素数
	if (n < 2 || n % 2 == 0)
	{
		return 0;
	}

	//	做times次随机检验
	for (uint8_t i = 1; i != 20; i++)
	{
		//得到随机检验算子 a
		CryptUint a = CryptRandom(n - 2) + 1;
		//用a检验n是否是素数
		if (!CryptWitness(a, n))
		{
			return 0;
		}
	}
	return 1;
}

static CryptUint CryptGetPrime(CryptUint n)
{
	while (0 == CryptMillerRabin(n)) n--;
	return n;
}

#endif // __CRYPT_H__

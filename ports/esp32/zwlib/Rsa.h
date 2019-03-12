#ifndef __RSA_H__
#define __RSA_H__

#include "Crypt.h"

typedef CryptUint RsaVar;

RsaVar RsaCrypt(RsaVar Data, RsaVar Crypt, RsaVar Max);

#endif // __RSA_H__

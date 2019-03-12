
#include "Crc.h"
#include <stdbool.h>

/*
*Derive parameters from the standard-specific parameters in crc.h.
 */
#if defined(CRC_8)
typedef unsigned char crc;
#elif defined(CRC_16)
typedef unsigned short  crc;
#elif defined(CRC_32)
typedef unsigned long  crc;
#else
#error "One of CRC_8, CRC_CCITT, CRC_16, or CRC_32 must be #define'd."
#endif

#define TEST_STR "123456789"

#define FEATURES

#ifdef FEATURES

#define CRC_NAME			"CRC-8"
#define POLYNOMIAL			0xAD
#define INITIAL_REMAINDER	(~0)
#define FINAL_XOR_VALUE		0
#define REVERSE_DATA		false
#define REVERSE_REMAINDER	false
#define CHECK_VALUE			0xF7

#else

#if defined(CRC_8)

#define CRC_NAME			"CRC-8"
#define POLYNOMIAL			0xD5
#define INITIAL_REMAINDER	(-1)
#define FINAL_XOR_VALUE		0
#define REVERSE_DATA		false
#define REVERSE_REMAINDER	false
#define CHECK_VALUE			0x7C

#elif defined(CRC_16)

#define CRC_NAME			"CRC-16"
#define POLYNOMIAL			0x8005
#define INITIAL_REMAINDER	0
#define FINAL_XOR_VALUE		0
#define REVERSE_DATA		true
#define REVERSE_REMAINDER	false
#define CHECK_VALUE			0xBB3D

#elif defined(CRC_32)

#define CRC_NAME			"CRC-32"
#define POLYNOMIAL			0x04C11DB7
#define INITIAL_REMAINDER	(-1)
#define FINAL_XOR_VALUE		(-1)
#define REVERSE_DATA		true
#define REVERSE_REMAINDER	true
#define CHECK_VALUE			0xCBF43926

#else
#error "One of CRC_8, CRC_CCITT, CRC_16, or CRC_32 must be #define'd."
#endif
#endif

#if !defined(REVERSE_DATA)
#undef  REVERSE_DATA
#define REVERSE_DATA(X)			((unsigned char) Reflect((X), 8))
#else
#undef  REVERSE_DATA
#define REVERSE_DATA(X)			(X)
#endif

#if !defined(REVERSE_REMAINDER)
#undef  REVERSE_REMAINDER
#define REVERSE_REMAINDER(X)	((crc) Reflect((X), WIDTH))
#else
#undef  REVERSE_REMAINDER
#define REVERSE_REMAINDER(X)	(X)
#endif

#define WIDTH    (8 * sizeof(crc))
#define TOPBIT   (1 << (WIDTH - 1))

/*********************************************************************
*
* Function:    Reflect()
*
* Description: Reorder the bits of a binary sequence, by reflecting
*				them about the middle position.
*
* Notes:		No checking is done that nBits <= 32.
*
* Returns:		The reflection of the original data.
*
******************************************************************** */
static unsigned long Reflect(unsigned long data, unsigned char nBits)
{
    unsigned long  reflection = 0x00000000;
    unsigned char  bit;

    /*
    *Reflect the data about the center bit.
     */
    for ( bit = 0; bit < nBits; ++bit )
    {
        /*
        *If the LSB bit is set, set the reflection of it.
         */
        if ( data & 0x01 )
        {
            reflection |= (1 << ((nBits - 1) - bit));
        }

        data = (data >> 1);
    }

    return (reflection);

}	/*Reflect()  */

/*********************************************************************
*
*Function:    CrcSlow()
*
*Description: Compute the CRC of a given message.
*
*Notes:
*
*Returns:		The CRC of the message.
*
******************************************************************** */
crc CrcSlow(unsigned char const message[ ], int nBytes)
{
    crc            remainder = INITIAL_REMAINDER;
    int            byte;
    unsigned char  bit;


    /*
    *Perform modulo-2 division, a byte at a time.
     */
    for ( byte = 0; byte < nBytes; ++byte )
    {
        /*
        *Bring the next byte into the remainder.
         */
        remainder ^= (REVERSE_DATA(message[byte]) << (WIDTH - 8));

        /*
        *Perform modulo-2 division, a bit at a time.
         */
        for ( bit = 8; bit > 0; --bit )
        {
            /*
            *Try to divide the current data bit.
             */
            if ( remainder & TOPBIT )
            {
                remainder = (remainder << 1) ^ POLYNOMIAL;
            }
            else
            {
                remainder = (remainder << 1);
            }
        }
    }

    /*
    *The final remainder is the CRC result.
     */
    return (REVERSE_REMAINDER(remainder) ^ FINAL_XOR_VALUE);

}   /*CrcSlow()  */


static crc  CrcTable[256];

/*********************************************************************
*
*Function:    CrcInit()
*
*Description: Populate the partial CRC lookup table.
*
*Notes:		Self function must be rerun any time the CRC standard
*				is changed.  If desired, it can be run "offline" and
*				the table results stored in an embedded system's ROM.
*
*Returns:		None defined.
*
******************************************************************** */
void CrcInit(void)
{
    crc			   remainder;
    int			   dividend;
    unsigned char  bit;


    /*
    *Compute the remainder of each possible dividend.
     */
    for ( dividend = 0; dividend < 256; ++dividend )
    {
        /*
        *Start with the dividend followed by zeros.
         */
        remainder = dividend << (WIDTH - 8);

        /*
        *Perform modulo-2 division, a bit at a time.
         */
        for ( bit = 8; bit > 0; --bit )
        {
            /*
            *Try to divide the current data bit.
             */
            if ( remainder & TOPBIT )
            {
                remainder = (remainder << 1) ^ POLYNOMIAL;
            }
            else
            {
                remainder = (remainder << 1);
            }
        }

        /*
        *Store the result into the table.
         */
        CrcTable[dividend] = remainder;
    }

}   /*crcInit()  */


/*********************************************************************
*
*Function:    CrcFast()
*
*Description: Compute the CRC of a given message.
*
*Notes:		crcInit() must be called first.
*
*Returns:		The CRC of the message.
*
******************************************************************** */
crc CrcFast(unsigned char const message[ ], int nBytes)
{
    crc	           remainder = INITIAL_REMAINDER;
    unsigned char  data;
    int            byte;

    /*
    *Divide the message by the polynomial, a byte at a time.
     */
    for ( byte = 0; byte < nBytes; ++byte )
    {
        data = REVERSE_DATA(message[byte]) ^ (remainder >> (WIDTH - 8));
        remainder = CrcTable[data] ^ (remainder << 8);
    }

    /*
    *The final remainder is the CRC.
     */
    return (REVERSE_REMAINDER(remainder) ^ FINAL_XOR_VALUE);

}   /*crcFast()  */

#ifdef UNIT_TEST

#include <assert.h>

static int main()
{
    assert(CHECK_VALUE == CrcSlow(TEST_STR, strlen(TEST_STR)));
    CrcInit();
    assert(CHECK_VALUE == CrcFast(TEST_STR, strlen(TEST_STR)));
}   /*main()  */

#endif


#ifndef CRC_H
#define CRC_H

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Select the CRC standard from the list that follows.
 * for example : #define CRC_8
 */

#define CRC_8

#if defined(CRC_8)
	typedef unsigned char crc;
#elif defined(CRC_16)
	typedef unsigned short crc;
#elif defined(CRC_32)
	typedef unsigned long crc;
#else
#error "One of CRC_8, CRC_CCITT, CRC_16, or CRC_32 must be #define'd."
#endif

	void CrcInit(void);
  crc  CrcSlow(unsigned char const message[ ], int nBytes);
	crc  CrcFast(unsigned char const message[], int nBytes);

#ifdef __cplusplus
}
#endif

#endif /*CRC_H */

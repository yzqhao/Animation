#pragma once

#include "System.h"

class UsualFile
{
public:
	enum //Open Mode
	{
		OM_RB,
		OM_WB,
		OM_RT,
		OM_WT,
		OM_MAX
	};
	enum 
	{
		VSMAX_PATH = 256
	};
	enum	//Seek Flag
	{
		SF_CUR,
		SF_END,
		SF_SET,
		SF_MAX

	};
	UsualFile();
	~UsualFile();
	bool Flush();

	bool Seek(uint uiOffset, uint uiOrigin);
	bool Open(const char* pFileName, uint uiOpenMode);
	bool Write(const void *pBuffer, uint uiSize, uint uiCount = 1);
	bool Read(void *pBuffer, uint uiSize, uint uiCount);
	
	inline uint GetFileSize()const
	{
		return m_uiFileSize;
	}
	static bool IsFileExists(const char * pFileName);
protected:
	static char ms_cOpenMode[OM_MAX][5];
	static uint m_uiSeekFlag[];
	FILE  *m_pFileHandle;
	uint m_uiOpenMode;
	uint m_uiFileSize;
	char m_tcFileName[VSMAX_PATH];
	
};

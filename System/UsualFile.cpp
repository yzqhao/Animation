#include "UsualFile.h"

#include <tchar.h>
#include <sys/stat.h>

char UsualFile::ms_cOpenMode[OM_MAX][5] = 
{
	("rb"),
	("wb"),
	("rt"),
	("wt"),
};

uint UsualFile::m_uiSeekFlag[] = 
{
	SEEK_CUR,
	SEEK_END,
	SEEK_SET
};

bool UsualFile::IsFileExists(const char * pFileName)
{
	struct _stat64i32 kStat;
	if (_tstat(pFileName, &kStat) != 0)
	{
		return false;
	}
	return true;
}

UsualFile::UsualFile()
{
	m_pFileHandle = NULL;
	m_uiOpenMode = OM_MAX;
	m_uiFileSize = 0;
}

UsualFile::~UsualFile()
{
	if (m_pFileHandle)
	{
		fclose(m_pFileHandle);
		m_pFileHandle = NULL;
	}
}

bool UsualFile::Seek(uint uiOffset,uint uiOrigin)
{
	JY_ASSERT(m_pFileHandle);
	fseek(m_pFileHandle,uiOffset,uiOrigin);
	return true;
}

bool UsualFile::Open(const char * pFileName, uint uiOpenMode)
{
	if (m_pFileHandle)
	{
		fclose(m_pFileHandle);
	}
	JY_ASSERT(!m_pFileHandle);
	JY_ASSERT(uiOpenMode < OM_MAX);
	uint uiLen = strlen(pFileName);
	if (uiLen < VSMAX_PATH - 1)
	{
		if(!Memcpy(m_tcFileName,pFileName,uiLen + 1))
			return false;
	}
	else
	{
		return false;
	}

	m_uiOpenMode = uiOpenMode;
	if (m_uiOpenMode == OM_RB || m_uiOpenMode == OM_RT)
	{
		struct _stat64i32 kStat;
		if (_tstat(pFileName,&kStat) != 0)
		{
			return false;
		}
		m_uiFileSize = kStat.st_size;
	}

	errno_t uiError = _tfopen_s(&m_pFileHandle,pFileName,ms_cOpenMode[m_uiOpenMode]);
	if (uiError)
	{
		return 0;
	}
	if (!m_pFileHandle)
	{
		return 0;
	}

	return true;
}

bool UsualFile::Write(const void *pBuffer, uint uiSize, uint uiCount)
{
	JY_ASSERT(m_pFileHandle);
	JY_ASSERT(pBuffer);
	JY_ASSERT(uiSize);
	JY_ASSERT(uiCount);
	fwrite(pBuffer,uiSize,uiCount,m_pFileHandle);
	return true;
}

bool UsualFile::Read(void *pBuffer,uint uiSize,uint uiCount)
{
	JY_ASSERT(m_pFileHandle);
	JY_ASSERT(pBuffer);
	JY_ASSERT(uiSize);
	JY_ASSERT(uiCount);
	fread(pBuffer,uiSize,uiCount,m_pFileHandle);
	return true;
}

bool UsualFile::Flush()
{
	return(fflush(m_pFileHandle) == 0);
}
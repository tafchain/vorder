#ifndef VBFS_CONFIG_SELECT_DEF_H_9878945613516354984964518574841
#define VBFS_CONFIG_SELECT_DEF_H_9878945613516354984964518574841

#include "dsc/db/per/persistence.h"
#include "dsc/dsc_database_factory.h"


class CVbfsConfigOnlyFileSize
{
public:
	CVbfsConfigOnlyFileSize()
		: m_fileSize("FILE_SIZE")
		, m_deviceType("DEVICE_TYPE")
	{
	}

public:
	PER_BIND_ATTR(m_fileSize, m_deviceType);

public:
	CColumnWrapper< ACE_UINT32 > m_fileSize; //读取的单位是GB
	CColumnWrapper< CDscString > m_deviceType;
};

class CVbfsConfigWriteSetVersion
{
public:
	CVbfsConfigWriteSetVersion()
		: m_fileSize("FILE_SIZE")
		, m_pageSize("WRITE_SET_VERSION_TABLE_PAGE_SIZE")
		, m_pageCacheCount("WRITE_SET_VERSION_TABLE_PAGE_CACHE_COUNT")
	{
	}

public:
	PER_BIND_ATTR(m_fileSize, m_pageSize, m_pageCacheCount);

public:
	CColumnWrapper< ACE_UINT32 > m_fileSize; //文件大小，读取的单位是GB
	CColumnWrapper< ACE_UINT32 > m_pageSize; //页大小，读取的单位是KB
	CColumnWrapper< ACE_UINT32 > m_pageCacheCount; //缓存页个数
};

class CVbfsConfigBlockIndex
{
public:
	CVbfsConfigBlockIndex()
		: m_fileSize("FILE_SIZE")
		, m_pageSize("BLOCK_INDEX_TABLE_PAGE_SIZE")
		, m_pageCacheCount("BLOCK_INDEX_TABLE_PAGE_CACHE_COUNT")
	{
	}

public:
	PER_BIND_ATTR(m_fileSize, m_pageSize, m_pageCacheCount);

public:
	CColumnWrapper< ACE_UINT32 > m_fileSize; //文件大小，读取的单位是GB
	CColumnWrapper< ACE_UINT32 > m_pageSize; //页大小，读取的单位是KB
	CColumnWrapper< ACE_UINT32 > m_pageCacheCount; //缓存页个数
};

class CVbfsConfigWriteSetIndex
{
public:
	CVbfsConfigWriteSetIndex()
		: m_fileSize("FILE_SIZE")
		, m_pageSize("WRITE_SET_INDEX_TABLE_PAGE_SIZE")
		, m_pageCacheCount("WRITE_SET_INDEX_TABLE_PAGE_CACHE_COUNT")
	{
	}

public:
	PER_BIND_ATTR(m_fileSize, m_pageSize, m_pageCacheCount);

public:
	CColumnWrapper< ACE_UINT32 > m_fileSize; //文件大小，读取的单位是GB
	CColumnWrapper< ACE_UINT32 > m_pageSize; //页大小，读取的单位是KB
	CColumnWrapper< ACE_UINT32 > m_pageCacheCount; //缓存页个数
};

class CVbfsConfigWriteSetHistory
{
public:
	CVbfsConfigWriteSetHistory()
		: m_fileSize("FILE_SIZE")
		, m_pageSize("WRITE_SET_HISTORY_TABLE_PAGE_SIZE")
		, m_pageCacheCount("WRITE_SET_HISTORY_TABLE_PAGE_CACHE_COUNT")
	{
	}

public:
	PER_BIND_ATTR(m_fileSize, m_pageSize, m_pageCacheCount);

public:
	CColumnWrapper< ACE_UINT32 > m_fileSize; //文件大小，读取的单位是GB
	CColumnWrapper< ACE_UINT32 > m_pageSize; //页大小，读取的单位是KB
	CColumnWrapper< ACE_UINT32 > m_pageCacheCount; //缓存页个数
};

class CVbfsCriterion : public CSelectCriterion
{
public:
	CVbfsCriterion(const ACE_UINT32 nChannelID)
		:m_nChannelID(nChannelID)
	{
	}

public:
	virtual void SetCriterion(CPerSelect& rPerSelect)
	{
		rPerSelect.Where(rPerSelect["CHANNEL_ID"] == m_nChannelID);
	}

private:
	const ACE_UINT32 m_nChannelID;
};


class CVbfsDeviceConfig
{
public:
	CVbfsDeviceConfig()
		: m_deviceIndex("DEVICE_INDEX")
		, m_deviceUrl("DEVICE_URL")
		, m_fileNum("FILE_NUMBER")
	{
	}

public:
	PER_BIND_ATTR(m_deviceIndex, m_deviceUrl, m_fileNum);

public:
	CColumnWrapper< ACE_INT32 > m_deviceIndex;
	CColumnWrapper< CDscString > m_deviceUrl;
	CColumnWrapper< ACE_INT32 > m_fileNum;
};


class CVbfsDeviceCriterion : public CSelectCriterion
{
public:
	CVbfsDeviceCriterion(const ACE_UINT32 nChannelID)
		:m_nChannelID(nChannelID)
	{
	}

public:
	virtual void SetCriterion(CPerSelect& rPerSelect)
	{
		rPerSelect.Where(rPerSelect["CHANNEL_ID"] == m_nChannelID);
		rPerSelect.OrderByAsc("DEVICE_INDEX"); //升序
	}

private:
	const ACE_UINT32 m_nChannelID;
};

class CVbfsNewDeviceCriterion : public CSelectCriterion
{
public:
	CVbfsNewDeviceCriterion(const ACE_UINT32 nChannelID, const ACE_UINT32 nDeviceIndex)
	: m_nChannelID(nChannelID)
	, m_nDeviceIndex(nDeviceIndex)
	{
	}

public:
	virtual void SetCriterion(CPerSelect& rPerSelect)
	{
		rPerSelect.Where( (rPerSelect["CHANNEL_ID"] == m_nChannelID) && (rPerSelect["DEVICE_INDEX"] > m_nDeviceIndex));
		rPerSelect.OrderByAsc("DEVICE_INDEX"); //升序
	}

private:
	const ACE_UINT32 m_nChannelID;
	const ACE_UINT32 m_nDeviceIndex;
};

#endif

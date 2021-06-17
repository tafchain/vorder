#ifndef VBH_PAGE_TABLE_83429872341861237123321123713256
#define VBH_PAGE_TABLE_83429872341861237123321123713256

#include "dsc/container/dsc_string.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"

class VBH_SERVER_COMM_DEF_EXPORT CVbhPageTable
{
public:
	CVbhPageTable(VBFS::CVbfs* pVbfs, const ACE_UINT32 nTableType, const ACE_UINT32 nPageSize, const ACE_UINT32 nFilePageNum);

public:
	//从文件读取1页 //nPageID从0开始连续编号
	ACE_INT32 ReadPage(char* pPage, const ACE_UINT64 nPageID);
	//写入1页到文件 //nPageID从0开始连续编号
	ACE_INT32 WritePage(const char* pPage, const ACE_UINT64 nPageID);
	ACE_INT32 WriteNewPage(const char* pPage, const ACE_UINT64 nPageID);

private:
	//根据PageID获取FileID，FileID从0开始编号
	ACE_UINT64 GetFileID(const ACE_UINT64 nPageID) const;
	ACE_UINT64 GetOffset(const ACE_UINT64 nPageID) const;

private:
	VBFS::CVbfs* m_pVbfs; //
	const ACE_UINT32 m_nTableType;
	const ACE_UINT32 m_nPageSize; //1页大小
	const ACE_UINT32 m_nFilePageNum; //1个文件中的页数
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_page_table.inl"

#endif

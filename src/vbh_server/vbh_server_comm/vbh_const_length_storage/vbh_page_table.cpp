#include "ace/OS_NS_sys_stat.h"
#include "ace/OS_NS_fcntl.h"

#include "dsc/dsc_log.h"
#include "dsc/mem_mng/dsc_allocator.h"

#include "vbh_server_comm/vbh_const_length_storage/vbh_page_table.h"

CVbhPageTable::CVbhPageTable(VBFS::CVbfs* pVbfs, const ACE_UINT32 nTableType, const ACE_UINT32 nPageSize, const ACE_UINT32 nFilePageNum)
	: m_pVbfs(pVbfs)
	, m_nTableType(nTableType)
	, m_nPageSize(nPageSize)
	, m_nFilePageNum(nFilePageNum)
{
}

ACE_INT32 CVbhPageTable::WriteNewPage(const char* pPage, const ACE_UINT64 nPageID)
{
	const ACE_UINT32 nFileID = GetFileID(nPageID);

	if (!(nPageID % m_nFilePageNum))
	{//需要底层新分配文件
		if (m_pVbfs->AllocatFile(m_nTableType, nFileID))
		{
			DSC_RUN_LOG_ERROR("AllocatFile failed, table-type:%d, table-name:%s", m_nTableType, VBH_CLS::GetClsTableName(m_nTableType));

			return -1;
		}
	}

	return m_pVbfs->Write(m_nTableType, nFileID, pPage, m_nPageSize, this->GetOffset(nPageID));
}

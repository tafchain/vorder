inline ACE_UINT64 CVbhRecordTable::GetPageID(const ACE_UINT64 nRecordID) const
{
	return nRecordID / m_nPageRecordNum;
}

inline ACE_OFF_T CVbhRecordTable::GetOffset(const ACE_UINT64 nRecordID) const
{
	return (nRecordID % m_nPageRecordNum) * m_nRecordLen;
}

inline ACE_INT32 CVbhRecordTable::WritePage(CPage* pPage)
{
	ACE_ASSERT(pPage->m_bDirty);

	if (DSC_UNLIKELY(pPage->m_bNewPage))
	{
		return this->m_pVbhPageTable->WriteNewPage(pPage->m_pContent, pPage->m_nPageID);
	}
	else
	{
		return this->m_pVbhPageTable->WritePage(pPage->m_pContent, pPage->m_nPageID);
	}
}

inline ACE_INT32 CVbhRecordTable::WritePage(const char* pPageContent, const ACE_UINT64 nPageID, const bool bNewPage)
{
	if (bNewPage)
	{
		return this->m_pVbhPageTable->WriteNewPage(pPageContent, nPageID);
	}
	else
	{
		return this->m_pVbhPageTable->WritePage(pPageContent, nPageID);
	}
}

inline char* CVbhRecordTable::AllocPageContent(void)
{
	char* pContent = m_freePageContentCache.Pop();

	if (pContent)
	{
		return pContent;
	}
	else
	{
		DSC_MEM_ALIGN(pContent, m_nPageSize);

		return pContent;
	}
}

inline void CVbhRecordTable::FreePageContent(char* pContent)
{
	m_freePageContentCache.Push(pContent);
}

inline CVbhRecordTable::CPage* CVbhRecordTable::AllocPage(void)
{
	CPage* pPage = DSC_THREAD_TYPE_NEW(CPage) CPage;

	pPage->m_pContent = AllocPageContent();

	return pPage;
}

inline void CVbhRecordTable::FreePage(CPage* pPage)
{
	ACE_ASSERT(pPage->m_pContent);

	FreePageContent(pPage->m_pContent);

	DSC_THREAD_TYPE_DELETE(pPage);
}

inline void CVbhRecordTable::ReleasePage(CPage* pPage)
{
	m_queuePage.Erase(pPage);
	m_mapPage.Erase(pPage);
	--m_nMapPageNum;
	this->FreePage(pPage);
}

inline char* CVbhRecordTable::GetRecordPtr(CPage* pPage, const ACE_UINT64 nRecordID) const
{
	return pPage->m_pContent + this->GetOffset(nRecordID);
}

inline char* CVbhRecordTable::GetRecordPtrByPageRecordID(CPage* pPage, const ACE_UINT64 nPageRecordID) const
{
	return pPage->m_pContent + nPageRecordID * m_nRecordLen;
}

inline ACE_UINT64 CVbhRecordTable::GetPageRecordNum(void) const
{
	return m_nPageRecordNum;
}

inline ACE_UINT64 CVbhRecordTable::GetRecordLen(void) const
{
	return m_nRecordLen;
}

inline ACE_UINT32 CVbhRecordTable::GetPageSize(void) const
{
	return m_nPageSize;
}

inline ACE_INT32 CVbhRecordTable::ReadPage(char* pPageContent, const ACE_UINT64 nPageID)
{
	return m_pVbhPageTable->ReadPage(pPageContent, nPageID);
}

template<typename RECORD_TYPE>
ACE_INT32 CVbhRecordTable::Read(RECORD_TYPE& rRecord, const ACE_UINT64 nRecordID)
{
	CPage* pPage = this->GetPage(this->GetPageID(nRecordID));
	
	if (pPage)
	{
		DSC::HostDecode(rRecord, this->GetRecordPtr(pPage, nRecordID), this->GetRecordLen());

		return 0;
	}
	else
	{
		return -1;
	}
}

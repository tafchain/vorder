inline ACE_UINT64 CVbhPageTable::GetFileID(const ACE_UINT64 nPageID) const
{
	return nPageID / m_nFilePageNum;
}

inline ACE_UINT64 CVbhPageTable::GetOffset(const ACE_UINT64 nPageID) const
{
	return (nPageID % m_nFilePageNum) * m_nPageSize;
}

inline ACE_INT32 CVbhPageTable::WritePage(const char* pPage, const ACE_UINT64 nPageID)
{
	return m_pVbfs->Write(m_nTableType, GetFileID(nPageID), pPage, m_nPageSize, this->GetOffset(nPageID));
}

inline ACE_INT32 CVbhPageTable::ReadPage(char* pPage, const ACE_UINT64 nPageID)
{
	return m_pVbfs->Read(m_nTableType, GetFileID(nPageID), pPage, m_nPageSize, this->GetOffset(nPageID));
}


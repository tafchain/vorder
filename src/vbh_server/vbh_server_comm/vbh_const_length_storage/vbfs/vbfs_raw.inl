
inline ACE_UINT32 VBFS::CRawVbfs::GetAlocPhyFileNum(void)
{
	return m_nStartFreePhyFileIdx;
}

inline ACE_UINT32 VBFS::CRawVbfs::GetMaxPhyFileNum(void)
{
	return m_fsCfgParam.m_nFileNum;
}

inline ACE_UINT32 VBFS::CRawVbfs::GetAlocLogicFileNum(const ACE_UINT32 nFileType)
{
	ACE_ASSERT(nFileType < EN_MAX_FILE_TYPE);

	return m_arrLogicFileIdx[nFileType].m_nCurNum;
}



inline void CVbhMultiUpdateTable::CommitteTransaction(void)
{
	CModifyPackage* pModifyPackage = m_queueModifyPackage.PopFront();

	FreeModifyPackage(pModifyPackage);
}

template<typename RECORD_TYPE>
ACE_INT32 CVbhMultiUpdateTable::Append(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord)
{
	ACE_ASSERT(DSC::GetSize(rRecord) == this->GetRecordLen());

	//将更改前信息保存至日志，update存储信息
	const ACE_UINT64 nPageRecordID = nRecordID % this->GetPageRecordNum();

	if (DSC_LIKELY(nPageRecordID))//不是首条记录，继续填写·未满的页
	{
		CPage* pPage = this->GetPage(this->GetPageID(nRecordID));

		if (pPage)
		{
			DSC::CDscHostCodecEncoder encoder(this->GetRecordPtrByPageRecordID(pPage, nPageRecordID));

			rRecord.Encode(encoder);
			if (!pPage->m_bDirty)
			{
				pPage->m_bDirty = true;
				m_lstCurDirtyPage.push_back(pPage);
			}
		}
		else
		{
			DSC_RUN_LOG_ERROR("append failed, nRecordID:%llu.", nRecordID);

			return -1;
		}
	}
	else
	{
		CPage* pPage = this->AllocPage();

		pPage->m_nPageID = this->GetPageID(nRecordID);
		pPage->m_bDirty = true; //设置dirty防止被换出
		pPage->m_bNewPage = true;
		this->InsertPage(pPage);//入缓存，以便后续查询

		DSC::CDscHostCodecEncoder encoder(this->GetRecordPtrByPageRecordID(pPage, nPageRecordID));

		rRecord.Encode(encoder);
		m_lstCurDirtyPage.push_back(pPage);
	}

	return 0;
}

template<typename RECORD_TYPE>
ACE_INT32 CVbhMultiUpdateTable::Update(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord)
{
	ACE_ASSERT(DSC::GetSize(rRecord) == this->GetRecordLen());

	//将更改前信息保存至日志，update存储信息
	CPage* pPage = this->GetPage(this->GetPageID(nRecordID));

	if (pPage)
	{
		DSC::CDscHostCodecEncoder encoder(this->GetRecordPtr(pPage, nRecordID));

		rRecord.Encode(encoder);
		if (!pPage->m_bDirty)
		{
			pPage->m_bDirty = true;
			m_lstCurDirtyPage.push_back(pPage);
		}

		return 0;
	}
	else
	{
		DSC_RUN_LOG_ERROR("update failed, nRecordID:%llu.", nRecordID);

		return -1;
	}
}

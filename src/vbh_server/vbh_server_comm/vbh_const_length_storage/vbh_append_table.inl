inline ACE_INT32 CVbhAppendTable::SaveToLog(void)
{
	m_pLog->m_nLastRecordID = m_pCfg->m_nLastRecordID;

	return 0;
}

inline ACE_INT32 CVbhAppendTable::RollbackTransaction(void)
{
	this->RollbackCache();
	m_pCfg->m_nLastRecordID = m_pLog->m_nLastRecordID;

	return 0;
}

inline ACE_INT32 CVbhAppendTable::RecoverFromLog(void)
{
	m_pCfg->m_nLastRecordID = m_pLog->m_nLastRecordID;
	m_nLastRecordID = m_pLog->m_nLastRecordID;

	return 0;
}

template<typename RECORD_TYPE>
ACE_INT32 CVbhAppendTable::Append(ACE_UINT64& nRecordID, RECORD_TYPE& rRecord)
{
	ACE_ASSERT(DSC::GetSize(rRecord) == this->GetRecordLen());

	nRecordID = m_nLastRecordID;

	const ACE_UINT64 nPageID = this->GetPageID(nRecordID);
	if (DSC_LIKELY(m_pAppendPage))
	{
		if (nPageID != m_pAppendPage->m_nPageID) //PageID = 当前Page中最大的Item的ItemID //表示当前item是属于下一个页的item
		{
			ACE_ASSERT(nPageID == (m_pAppendPage->m_nPageID + 1));

			m_queueAppendPage.push_back(m_pAppendPage);

			m_pAppendPage = this->AllocPage();
			m_pAppendPage->m_nPageID = nPageID;
			m_pAppendPage->m_bDirty = true;//设置dirty防止被换出
			m_pAppendPage->m_bNewPage = true;

			this->InsertPage(m_pAppendPage);//入缓存，以便后续查询
		}

		DSC::CDscHostCodecEncoder encoder(this->GetRecordPtr(m_pAppendPage, nRecordID));

		rRecord.Encode(encoder);
	}
	else
	{
		const ACE_UINT64 nPageRecordID = nRecordID % this->GetPageRecordNum();

		if (nPageRecordID)//不是首条记录，继续填写·未满的页
		{
			m_pAppendPage = this->GetPage(nPageID);
			if (m_pAppendPage)
			{
				m_pAppendPage->m_bNewPage = false;
				m_pAppendPage->m_bDirty = true;
			}
			else
			{
				DSC_RUN_LOG_ERROR("GetPage() failed, nRecordID:%llu, m_nLastRecordID:%llu.", nRecordID, nRecordID);

				return -1;
			}
		}
		else //是首条记录//需要准备m_appendPage
		{
			m_pAppendPage = this->AllocPage();
			m_pAppendPage->m_nPageID = nPageID;
			m_pAppendPage->m_bDirty = true;//设置dirty防止被换出
			m_pAppendPage->m_bNewPage = true;

			this->InsertPage(m_pAppendPage);//入缓存，以便后续查询
		}

		DSC::CDscHostCodecEncoder encoder(this->GetRecordPtrByPageRecordID(m_pAppendPage, nPageRecordID));

		rRecord.Encode(encoder);
	}

	++m_nLastRecordID;

	return 0;
}

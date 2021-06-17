//ֻ֧�ֶ�����¼׷�ӣ�//Լ���ϲ�ҵ��ID��1��ʼ���������룬�����пն�
template<typename RECORD_TYPE>
ACE_INT32 CVbhUpdateTable::Append(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord)
{
	ACE_ASSERT(DSC::GetSize(rRecord) <= this->GetRecordLen());
	
	//������ǰ��Ϣ��������־��update�洢��Ϣ
	const ACE_UINT64 nPageRecordID = nRecordID % this->GetPageRecordNum();

	if (DSC_LIKELY(nPageRecordID))//����������¼��������д��δ����ҳ
	{
		CPage* pPage = this->GetPage(this->GetPageID(nRecordID));

		if (pPage)
		{
			DSC::CDscHostCodecEncoder encoder(this->GetRecordPtrByPageRecordID(pPage, nPageRecordID));

			rRecord.Encode(encoder);

			if (!pPage->m_bDirty)
			{
				pPage->m_bDirty = true;
				m_lstDirtyPage.push_back(pPage);
			}
		}
		else
		{
			DSC_RUN_LOG_ERROR("update failed, nRecordID:%llu.", nRecordID);
			return -1;
		}
	}
	else
	{
		CPage* pPage = this->AllocPage();

		pPage->m_nPageID = this->GetPageID(nRecordID);
		pPage->m_bDirty = true;//����dirty��ֹ������
		pPage->m_bNewPage = true;

		this->InsertPage(pPage);//�뻺�棬�Ա������ѯ

		DSC::CDscHostCodecEncoder encoder(this->GetRecordPtrByPageRecordID(pPage, nPageRecordID));

		rRecord.Encode(encoder);
		m_lstDirtyPage.push_back(pPage);
	}

	return 0;
}

template<typename RECORD_TYPE>
ACE_INT32 CVbhUpdateTable::Update(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord)
{
	ACE_ASSERT(DSC::GetSize(rRecord) <= this->GetRecordLen());

	//������ǰ��Ϣ��������־��update�洢��Ϣ
	CModifyMemLogItem* pUpdateRecordLog = DSC_THREAD_TYPE_NEW(CModifyMemLogItem) CModifyMemLogItem;
	const ACE_UINT64 nPageID = this->GetPageID(nRecordID);

	pUpdateRecordLog->m_pPage = this->GetPage(nPageID);
	if (pUpdateRecordLog->m_pPage)
	{
		ACE_ASSERT(!pUpdateRecordLog->m_pPage->m_bNewPage); //user-id���ԣ�������������һ��������(һ��������)������user-id���ᱻ���£���������±ȴ�user-id�����user-id;

		char* pOldRecord = this->GetRecordPtr(pUpdateRecordLog->m_pPage, nRecordID);

		pUpdateRecordLog->m_nRecordID = nRecordID;
		pUpdateRecordLog->m_itemOriginalValue.AllocBuffer(this->GetRecordLen());
		memcpy(pUpdateRecordLog->m_itemOriginalValue.GetBuffer(), pOldRecord, this->GetRecordLen());
		
		DSC::CDscHostCodecEncoder encoder(pOldRecord);

		rRecord.Encode(encoder);		
		m_modifyMemLog.m_queueUpdateLog.PushBack(pUpdateRecordLog);

		if (!pUpdateRecordLog->m_pPage->m_bDirty)
		{
			pUpdateRecordLog->m_pPage->m_bDirty = true;
			m_lstDirtyPage.push_back(pUpdateRecordLog->m_pPage);
		}

		return 0;
	}
	else
	{
		DSC_THREAD_TYPE_DELETE(pUpdateRecordLog);
		DSC_RUN_LOG_ERROR("update failed, nRecordID:%llu.", nRecordID);

		return -1;
	}
}


inline ACE_INT32 CVbhUpdateTable::RollbackTransaction(void)
{
	RollbackCache();

	return RecoverFromLog();
}


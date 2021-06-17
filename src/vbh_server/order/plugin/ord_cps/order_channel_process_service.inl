
inline void CChannelProcessService::CPackTimerHandler::OnTimer(void)
{
	m_rCps.OnTimePackBlock();
}

inline void CChannelProcessService::CRepersistBlockQueueHeaderSession::OnTimer(void)
{
	m_rCps.OnTimeRepersistence(this);
}

inline void CChannelProcessService::CCsConnectSessionHandler::OnTimer(void)
{
	m_rCps.OnTimeDistributeBlock(this);
}


inline void CChannelProcessService::SetChannelprocessAgentService(IOrderChannelprocessAgentService* pOrderChannelprocessAgentService)
{

	m_pOrderChannelprocessAgentService = pOrderChannelprocessAgentService;

}
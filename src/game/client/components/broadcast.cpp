/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/client/gameclient.h>

#include <game/client/components/motd.h>
#include <game/client/components/scoreboard.h>

#include "broadcast.h"

void CBroadcast::OnReset()
{
	m_BroadcastTime = 0;
}

void CBroadcast::OnRender()
{
	if(m_pClient->m_pScoreboard->Active() || m_pClient->m_pMotd->IsActive() || !g_Config.m_ClShowBroadcasts)
		return;

	if(time_get() < m_BroadcastTime)
	{
		// m_aBroadcastText
	}
}

void CBroadcast::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_BROADCAST)
	{
		CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;
		str_copy(m_aBroadcastText, pMsg->m_pMessage, sizeof(m_aBroadcastText));
		if (g_Config.m_ClPrintBroadcasts)
		{
			char aBuf[1024];
			int i, ii;
			for (i = 0, ii = 0; i < str_length(m_aBroadcastText); i++)
			{
				if (m_aBroadcastText[i] == '\n')
				{
					aBuf[ii] = '\0';
					ii = 0;
					if (aBuf[0])
						m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "broadcast", aBuf, true);
				}
				else
				{
					aBuf[ii] = m_aBroadcastText[i];
					ii++;
				}
			}
			aBuf[ii] = '\0';
			if (aBuf[0])
				m_pClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "broadcast", aBuf, true);
		}
	}
}

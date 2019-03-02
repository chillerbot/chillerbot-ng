#include <engine/shared/config.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <game/generated/client_data.h>
#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/client/components/motd.h>
#include <game/client/components/statboard.h>

CStatboard::CStatboard()
{
	m_Active = false;
	m_ScreenshotTaken = false;
	m_ScreenshotTime = -1;
	m_pCSVstr = 0;
}

void CStatboard::OnReset()
{
	for(int i=0; i<MAX_CLIENTS; i++)
		m_pClient->m_aStats[i].Reset();
	m_Active = false;
	m_ScreenshotTaken = false;
	m_ScreenshotTime = -1;
}

void CStatboard::OnRelease()
{
	m_Active = false;
}

void CStatboard::ConKeyStats(IConsole::IResult *pResult, void *pUserData)
{
	((CStatboard *)pUserData)->m_Active = pResult->GetInteger(0) != 0;
}

void CStatboard::OnConsoleInit()
{
	Console()->Register("+statboard", "", CFGFLAG_CLIENT, ConKeyStats, this, "Show stats");
}

bool CStatboard::IsActive()
{
	return m_Active;
}

void CStatboard::OnMessage(int MsgType, void *pRawMsg)
{

}

void CStatboard::OnRender()
{

}

void CStatboard::RenderGlobalStats()
{

}

void CStatboard::AutoStatScreenshot()
{

}

void CStatboard::AutoStatCSV()
{

}

char* CStatboard::ReplaceCommata(char* pStr)
{
	if(!str_find(pStr, ","))
		return pStr;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s", pStr);

	char aOutbuf[256];
	mem_zero(aOutbuf, sizeof(aOutbuf));

	for(int i = 0, skip = 0; i < 64; i++)
	{
		if(aBuf[i] == ',')
		{
			aOutbuf[i + skip++] = '%';
			aOutbuf[i + skip++] = '2';
			aOutbuf[i + skip] = 'C';
		}
		else
			aOutbuf[i + skip] = aBuf[i];
	}

	unsigned int len = str_length(aOutbuf);
	char* buf = new char[len];
	mem_copy(buf, aOutbuf, len);
	return buf;
}

void CStatboard::FormatStats()
{

}

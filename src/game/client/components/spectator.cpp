/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>

#include "spectator.h"


void CSpectator::ConKeySpectator(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;

	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active || pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
		pSelf->m_Active = pResult->GetInteger(0) != 0;
	else
		pSelf->m_Active = false;
}

void CSpectator::ConSpectate(IConsole::IResult *pResult, void *pUserData)
{
	((CSpectator *)pUserData)->Spectate(pResult->GetInteger(0));
}

void CSpectator::ConSpectateNext(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	int NewSpectatorID;
	bool GotNewSpectatorID = false;

	int CurPos = -1;
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i] && pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_ClientID == pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID)
		CurPos = i;

	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i] || pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_Team == TEAM_SPECTATORS)
				continue;

			NewSpectatorID = pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_ClientID;
			GotNewSpectatorID = true;
			break;
		}
	}
	else
	{
		for(int i = CurPos + 1; i < MAX_CLIENTS; i++)
		{
			if(!pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i] || pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_Team == TEAM_SPECTATORS)
				continue;

			NewSpectatorID = pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_ClientID;
			GotNewSpectatorID = true;
			break;
		}

		if(!GotNewSpectatorID)
		{
			for(int i = 0; i < CurPos; i++)
			{
				if(!pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i] || pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_Team == TEAM_SPECTATORS)
					continue;

				NewSpectatorID = pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_ClientID;
				GotNewSpectatorID = true;
				break;
			}
		}
	}
	if(GotNewSpectatorID)
		pSelf->Spectate(NewSpectatorID);
}

void CSpectator::ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	int NewSpectatorID;
	bool GotNewSpectatorID = false;

	int CurPos = -1;
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i] && pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_ClientID == pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID)
		CurPos = i;

	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW)
	{
		for(int i = MAX_CLIENTS -1; i > -1; i--)
		{
			if(!pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i] || pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_Team == TEAM_SPECTATORS)
				continue;

			NewSpectatorID = pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_ClientID;
			GotNewSpectatorID = true;
			break;
		}
	}
	else
	{
		for(int i = CurPos - 1; i > -1; i--)
		{
			if(!pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i] || pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_Team == TEAM_SPECTATORS)
				continue;

			NewSpectatorID = pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_ClientID;
			GotNewSpectatorID = true;
			break;
		}

		if(!GotNewSpectatorID)
		{
			for(int i = MAX_CLIENTS - 1; i > CurPos; i--)
			{
				if(!pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i] || pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_Team == TEAM_SPECTATORS)
					continue;

			NewSpectatorID = pSelf->m_pClient->m_Snap.m_paInfoByDDTeam[i]->m_ClientID;
			GotNewSpectatorID = true;
				break;
			}
		}
	}
	if(GotNewSpectatorID)
		pSelf->Spectate(NewSpectatorID);
}

CSpectator::CSpectator()
{
	OnReset();
	m_OldMouseX = m_OldMouseY = 0.0f;
}

void CSpectator::OnConsoleInit()
{
	Console()->Register("+spectate", "", CFGFLAG_CLIENT, ConKeySpectator, this, "Open spectator mode selector");
	Console()->Register("spectate", "i[spectator-id]", CFGFLAG_CLIENT, ConSpectate, this, "Switch spectator mode");
	Console()->Register("spectate_next", "", CFGFLAG_CLIENT, ConSpectateNext, this, "Spectate the next player");
	Console()->Register("spectate_previous", "", CFGFLAG_CLIENT, ConSpectatePrevious, this, "Spectate the previous player");
}

bool CSpectator::OnMouseMove(float x, float y)
{
	return false;
}

void CSpectator::OnRelease()
{
	OnReset();
}

void CSpectator::OnRender()
{

}

void CSpectator::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedSpectatorID = NO_SELECTION;
}

void CSpectator::Spectate(int SpectatorID)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_pClient->m_DemoSpecID = clamp(SpectatorID, (int)SPEC_FOLLOW, MAX_CLIENTS-1);
		return;
	}

	if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SpectatorID)
		return;

	CNetMsg_Cl_SetSpectatorMode Msg;
	Msg.m_SpectatorID = SpectatorID;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

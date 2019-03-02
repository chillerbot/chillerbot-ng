/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include "nameplates.h"
#include "controls.h"
#include "camera.h"

#include "players.h"

void CNamePlates::MapscreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup)
{
	float Points[4];
}

void CNamePlates::RenderNameplate(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CNetObj_PlayerInfo *pPlayerInfo
	)
{
	float IntraTick = Client()->IntraGameTick();
	int ClientID = pPlayerInfo->m_ClientID;

	vec2 Position;
	if((!m_pClient->AntiPingPlayers() && !pPlayerInfo->m_Local) || m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		Position = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pPlayerChar->m_X, pPlayerChar->m_Y), IntraTick);
	}
	else if(!m_pClient->AntiPingPlayers() && pPlayerInfo->m_Local)
	{
		Position = vec2(m_pClient->m_LocalCharacterPos.x, m_pClient->m_LocalCharacterPos.y);
	}
	else
	{
		Position = m_pPlayers->m_CurPredictedPos[ClientID];
	}

	bool OtherTeam;

	if(m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team == TEAM_SPECTATORS && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW)
		OtherTeam = false;
	else if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		OtherTeam = m_pClient->m_Teams.Team(pPlayerInfo->m_ClientID) != m_pClient->m_Teams.Team(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID);
	else
		OtherTeam = m_pClient->m_Teams.Team(pPlayerInfo->m_ClientID) != m_pClient->m_Teams.Team(m_pClient->m_Snap.m_LocalClientID);

	float FontSize = 18.0f + 20.0f * g_Config.m_ClNameplatesSize / 100.0f;
	float FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNameplatesClanSize / 100.0f;
	// render name plate
	if(!pPlayerInfo->m_Local || g_Config.m_ClNameplatesOwn)
	{
		float a = 1;
		if(g_Config.m_ClNameplatesAlways == 0)
			a = clamp(1-powf(distance(m_pClient->m_pControls->m_TargetPos[g_Config.m_ClDummy], Position)/200.0f,16.0f), 0.0f, 1.0f);

		const char *pName = m_pClient->m_aClients[pPlayerInfo->m_ClientID].m_aName;
		if(str_comp(pName, m_aNamePlates[ClientID].m_aName) != 0 || FontSize != m_aNamePlates[ClientID].m_NameTextFontSize)
		{
			mem_copy(m_aNamePlates[ClientID].m_aName, pName, sizeof(m_aNamePlates[ClientID].m_aName));
		}

		if(g_Config.m_ClNameplatesClan)
		{
			const char *pClan = m_pClient->m_aClients[ClientID].m_aClan;
			if(str_comp(pClan, m_aNamePlates[ClientID].m_aClanName) != 0 || FontSizeClan != m_aNamePlates[ClientID].m_ClanNameTextFontSize)
			{
				mem_copy(m_aNamePlates[ClientID].m_aClanName, pClan, sizeof(m_aNamePlates[ClientID].m_aClanName));
			}
		}

		if(g_Config.m_Debug) // render client id when in debug as well
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf),"%d", pPlayerInfo->m_ClientID);
			float Offset = g_Config.m_ClNameplatesClan ? (FontSize * 2 + FontSizeClan) : (FontSize * 2);
		}
	}
}

void CNamePlates::OnRender()
{
	if(!g_Config.m_ClNameplates)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// only render active characters
		if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;

		const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);

		if(pInfo)
		{
			RenderNameplate(
				&m_pClient->m_Snap.m_aCharacters[i].m_Prev,
				&m_pClient->m_Snap.m_aCharacters[i].m_Cur,
				(const CNetObj_PlayerInfo *)pInfo);
		}
	}
}

void CNamePlates::SetPlayers(CPlayers* pPlayers)
{
	m_pPlayers = pPlayers;
}

void CNamePlates::ResetNamePlates()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aNamePlates[i].Reset();
	}

}

void CNamePlates::OnWindowResize()
{
	ResetNamePlates();
}

void CNamePlates::OnInit()
{
	ResetNamePlates();
}

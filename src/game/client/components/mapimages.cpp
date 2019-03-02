/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/map.h>
#include <engine/storage.h>
#include <engine/serverbrowser.h>
#include <game/client/component.h>
#include <game/mapitems.h>

#include "mapimages.h"

CMapImages::CMapImages()
{
	m_Count = 0;
	m_EntitiesTextures = -1;
	m_OverlayBottomTexture = -1;
	m_OverlayTopTexture = -1;
	m_OverlayCenterTexture = -1;
}

void CMapImages::OnInit()
{

}

void CMapImages::OnMapLoad()
{

}

void CMapImages::LoadBackground(class IMap *pMap)
{

}

int CMapImages::GetEntities()
{
  return 0;
}

int CMapImages::GetOverlayBottom()
{
	return m_OverlayBottomTexture;
}

int CMapImages::GetOverlayTop()
{
	return m_OverlayTopTexture;
}

int CMapImages::GetOverlayCenter()
{
	return m_OverlayCenterTexture;
}

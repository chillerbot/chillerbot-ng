/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/mapitems.h>
#include <game/layers.h>
#include "flow.h"

CFlow::CFlow()
{
	m_pCells = 0;
	m_Height = 0;
	m_Width = 0;
	m_Spacing = 16;
}

void CFlow::DbgRender()
{

}

void CFlow::Init()
{
	free(m_pCells);
	m_pCells = 0;

	CMapItemLayerTilemap *pTilemap = Layers()->GameLayer();
	m_Width = pTilemap->m_Width*32/m_Spacing;
	m_Height = pTilemap->m_Height*32/m_Spacing;

	// allocate and clear
	m_pCells = (CCell *)calloc(m_Width * m_Height, sizeof(CCell));
	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			m_pCells[y*m_Width+x].m_Vel = vec2(0.0f, 0.0f);
}

void CFlow::Update()
{
	if(!m_pCells)
		return;

	for(int y = 0; y < m_Height; y++)
		for(int x = 0; x < m_Width; x++)
			m_pCells[y*m_Width+x].m_Vel *= 0.85f;
}

vec2 CFlow::Get(vec2 Pos)
{
	if(!m_pCells)
		return vec2(0,0);

	int x = (int)(Pos.x / m_Spacing);
	int y = (int)(Pos.y / m_Spacing);
	if(x < 0 || y < 0 || x >= m_Width || y >= m_Height)
		return vec2(0,0);

	return m_pCells[y*m_Width+x].m_Vel;
}

void CFlow::Add(vec2 Pos, vec2 Vel, float Size)
{
	if(!m_pCells)
		return;

	int x = (int)(Pos.x / m_Spacing);
	int y = (int)(Pos.y / m_Spacing);
	if(x < 0 || y < 0 || x >= m_Width || y >= m_Height)
		return;

	m_pCells[y*m_Width+x].m_Vel += Vel;
}

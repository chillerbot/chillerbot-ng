/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/keys.h>
#include <engine/demo.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/layers.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>

#include <game/client/components/camera.h>
#include <game/client/components/mapimages.h>

#include <game/generated/client_data.h>

#include "maplayers.h"

CMapLayers::CMapLayers(int t)
{
	m_Type = t;
	m_pLayers = 0;
	m_CurrentLocalTick = 0;
	m_LastLocalTick = 0;
	m_EnvelopeUpdate = false;
}

void CMapLayers::OnInit()
{
	m_pLayers = Layers();
	m_pImages = m_pClient->m_pMapimages;
}

void CMapLayers::EnvelopeUpdate()
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		m_CurrentLocalTick = pInfo->m_CurrentTick;
		m_LastLocalTick = pInfo->m_CurrentTick;
		m_EnvelopeUpdate = true;
	}
}


void CMapLayers::MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom)
{

}

void CMapLayers::EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser)
{
	CMapLayers *pThis = (CMapLayers *)pUser;
	pChannels[0] = 0;
	pChannels[1] = 0;
	pChannels[2] = 0;
	pChannels[3] = 0;

	CEnvPoint *pPoints = 0;

	{
		int Start, Num;
		pThis->m_pLayers->Map()->GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);
		if(Num)
			pPoints = (CEnvPoint *)pThis->m_pLayers->Map()->GetItem(Start, 0, 0);
	}

	int Start, Num;
	pThis->m_pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);

	if(Env >= Num)
		return;

	CMapItemEnvelope *pItem = (CMapItemEnvelope *)pThis->m_pLayers->Map()->GetItem(Start+Env, 0, 0);

	static float s_Time = 0.0f;
	static float s_LastLocalTime = pThis->Client()->LocalTime();
	if(pThis->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = pThis->DemoPlayer()->BaseInfo();

		if(!pInfo->m_Paused || pThis->m_EnvelopeUpdate)
		{
			if(pThis->m_CurrentLocalTick != pInfo->m_CurrentTick)
			{
				pThis->m_LastLocalTick = pThis->m_CurrentLocalTick;
				pThis->m_CurrentLocalTick = pInfo->m_CurrentTick;
			}

			s_Time = mix(pThis->m_LastLocalTick / (float)pThis->Client()->GameTickSpeed(),
						pThis->m_CurrentLocalTick / (float)pThis->Client()->GameTickSpeed(),
						pThis->Client()->IntraGameTick());
		}
	}
	else
	{
		if(pThis->m_pClient->m_Snap.m_pGameInfoObj) // && !(pThis->m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
		{
			if(pItem->m_Version < 2 || pItem->m_Synchronized)
			{
				s_Time = mix((pThis->Client()->PrevGameTick()-pThis->m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / (float)pThis->Client()->GameTickSpeed(),
							(pThis->Client()->GameTick()-pThis->m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / (float)pThis->Client()->GameTickSpeed(),
							pThis->Client()->IntraGameTick());
			}
			else
				s_Time += pThis->Client()->LocalTime()-s_LastLocalTime;
		}
		s_LastLocalTime = pThis->Client()->LocalTime();
	}
}

struct STmpTile
{
	vec2 m_TopLeft;
	vec2 m_TopRight;
	vec2 m_BottomRight;
	vec2 m_BottomLeft;
};
struct STmpTileTexCoord
{
	STmpTileTexCoord()
	{
		m_TexCoordTopLeftRightOrBottom[0] = m_TexCoordTopLeftRightOrBottom[1] = 0;
		m_TexCoordBottomLeftRightOrBottom[0] = 0; m_TexCoordBottomLeftRightOrBottom[1] = 1;
		m_TexCoordTopRightRightOrBottom[0] = 1; m_TexCoordTopRightRightOrBottom[1] = 0;
		m_TexCoordBottomRightRightOrBottom[0] = m_TexCoordBottomRightRightOrBottom[1] = 1;
	}
	unsigned char m_TexCoordTopLeft[2];
	unsigned char m_TexCoordTopLeftRightOrBottom[2];
	unsigned char m_TexCoordTopRight[2];
	unsigned char m_TexCoordTopRightRightOrBottom[2];
	unsigned char m_TexCoordBottomRight[2];
	unsigned char m_TexCoordBottomRightRightOrBottom[2];
	unsigned char m_TexCoordBottomLeft[2];
	unsigned char m_TexCoordBottomLeftRightOrBottom[2];
};

void FillTmpTileSpeedup(STmpTile* pTmpTile, STmpTileTexCoord* pTmpTex, unsigned char Flags, unsigned char Index, int x, int y, int Scale, CMapItemGroup* pGroup, short AngleRotate)
{
	if(pTmpTex)
	{
		unsigned char x0 = 0;
		unsigned char y0 = 0;
		unsigned char x1 = 16;
		unsigned char y1 = 0;
		unsigned char x2 = 16;
		unsigned char y2 = 16;
		unsigned char x3 = 0;
		unsigned char y3 = 16;
		
		unsigned char bx0 = 0;
		unsigned char by0 = 0;
		unsigned char bx1 = 1;
		unsigned char by1 = 0;
		unsigned char bx2 = 1;
		unsigned char by2 = 1;
		unsigned char bx3 = 0;
		unsigned char by3 = 1;
	
		pTmpTex->m_TexCoordTopLeft[0] = x0;
		pTmpTex->m_TexCoordTopLeft[1] = y0;
		pTmpTex->m_TexCoordBottomLeft[0] = x3;
		pTmpTex->m_TexCoordBottomLeft[1] = y3;
		pTmpTex->m_TexCoordTopRight[0] = x1;
		pTmpTex->m_TexCoordTopRight[1] = y1;
		pTmpTex->m_TexCoordBottomRight[0] = x2;
		pTmpTex->m_TexCoordBottomRight[1] = y2;
	
		pTmpTex->m_TexCoordTopLeftRightOrBottom[0] = bx0;
		pTmpTex->m_TexCoordTopLeftRightOrBottom[1] = by0;
		pTmpTex->m_TexCoordBottomLeftRightOrBottom[0] = bx3;
		pTmpTex->m_TexCoordBottomLeftRightOrBottom[1] = by3;
		pTmpTex->m_TexCoordTopRightRightOrBottom[0] = bx1;
		pTmpTex->m_TexCoordTopRightRightOrBottom[1] = by1;
		pTmpTex->m_TexCoordBottomRightRightOrBottom[0] = bx2;
		pTmpTex->m_TexCoordBottomRightRightOrBottom[1] = by2;
	}
	
	float Angle = (float)AngleRotate*(3.14159265f/180.0f);
	float c = cosf(Angle);
	float s = sinf(Angle);
	float xR, yR;
	int i;
	
	int ScaleSmaller = 2;
	pTmpTile->m_TopLeft.x = x*Scale + ScaleSmaller;
	pTmpTile->m_TopLeft.y = y*Scale + ScaleSmaller;
	pTmpTile->m_BottomLeft.x = x*Scale + ScaleSmaller;
	pTmpTile->m_BottomLeft.y = y*Scale + Scale - ScaleSmaller;
	pTmpTile->m_TopRight.x = x*Scale + Scale - ScaleSmaller;
	pTmpTile->m_TopRight.y = y*Scale + ScaleSmaller;
	pTmpTile->m_BottomRight.x = x*Scale + Scale - ScaleSmaller;
	pTmpTile->m_BottomRight.y = y*Scale + Scale - ScaleSmaller;
	
	float* pTmpTileVertices = (float*)pTmpTile;

	vec2 Center;
	Center.x = pTmpTile->m_TopLeft.x + (Scale-ScaleSmaller)/2.f;
	Center.y = pTmpTile->m_TopLeft.y + (Scale-ScaleSmaller)/2.f;

	for(i = 0; i < 4; i++)
	{
		xR = pTmpTileVertices[i*2] - Center.x;
		yR = pTmpTileVertices[i*2+1] - Center.y;
		pTmpTileVertices[i*2] = xR * c - yR * s + Center.x;
		pTmpTileVertices[i*2+1] = xR * s + yR * c + Center.y;
	}
}

void FillTmpTile(STmpTile* pTmpTile, STmpTileTexCoord* pTmpTex, unsigned char Flags, unsigned char Index, int x, int y, int Scale, CMapItemGroup* pGroup)
{
	if(pTmpTex)
	{
		unsigned char tx = Index%16;
		unsigned char ty = Index/16;
		unsigned char x0 = tx;
		unsigned char y0 = ty;
		unsigned char x1 = tx+1;
		unsigned char y1 = ty;
		unsigned char x2 = tx+1;
		unsigned char y2 = ty+1;
		unsigned char x3 = tx;
		unsigned char y3 = ty+1;
		
		unsigned char bx0 = 0;
		unsigned char by0 = 0;
		unsigned char bx1 = 1;
		unsigned char by1 = 0;
		unsigned char bx2 = 1;
		unsigned char by2 = 1;
		unsigned char bx3 = 0;
		unsigned char by3 = 1;
		
		

		if(Flags&TILEFLAG_VFLIP)
		{
			x0 = x2;
			x1 = x3;
			x2 = x3;
			x3 = x0;
			
			bx0 = bx2;
			bx1 = bx3;
			bx2 = bx3;
			bx3 = bx0;
		}

		if(Flags&TILEFLAG_HFLIP)
		{
			y0 = y3;
			y2 = y1;
			y3 = y1;
			y1 = y0;
			
			by0 = by3;
			by2 = by1;
			by3 = by1;
			by1 = by0;
		}

		if(Flags&TILEFLAG_ROTATE)
		{
			unsigned char Tmp = x0;
			x0 = x3;
			x3 = x2;
			x2 = x1;
			x1 = Tmp;
			Tmp = y0;
			y0 = y3;
			y3 = y2;
			y2 = y1;
			y1 = Tmp;
			
			Tmp = bx0;
			bx0 = bx3;
			bx3 = bx2;
			bx2 = bx1;
			bx1 = Tmp;
			Tmp = by0;
			by0 = by3;
			by3 = by2;
			by2 = by1;
			by1 = Tmp;
		}
	
		pTmpTex->m_TexCoordTopLeft[0] = x0;
		pTmpTex->m_TexCoordTopLeft[1] = y0;
		pTmpTex->m_TexCoordBottomLeft[0] = x3;
		pTmpTex->m_TexCoordBottomLeft[1] = y3;
		pTmpTex->m_TexCoordTopRight[0] = x1;
		pTmpTex->m_TexCoordTopRight[1] = y1;
		pTmpTex->m_TexCoordBottomRight[0] = x2;
		pTmpTex->m_TexCoordBottomRight[1] = y2;
	
		pTmpTex->m_TexCoordTopLeftRightOrBottom[0] = bx0;
		pTmpTex->m_TexCoordTopLeftRightOrBottom[1] = by0;
		pTmpTex->m_TexCoordBottomLeftRightOrBottom[0] = bx3;
		pTmpTex->m_TexCoordBottomLeftRightOrBottom[1] = by3;
		pTmpTex->m_TexCoordTopRightRightOrBottom[0] = bx1;
		pTmpTex->m_TexCoordTopRightRightOrBottom[1] = by1;
		pTmpTex->m_TexCoordBottomRightRightOrBottom[0] = bx2;
		pTmpTex->m_TexCoordBottomRightRightOrBottom[1] = by2;
	}
	
	pTmpTile->m_TopLeft.x = x*Scale;
	pTmpTile->m_TopLeft.y = y*Scale;
	pTmpTile->m_BottomLeft.x = x*Scale;
	pTmpTile->m_BottomLeft.y = y*Scale + Scale;
	pTmpTile->m_TopRight.x = x*Scale + Scale;
	pTmpTile->m_TopRight.y = y*Scale;
	pTmpTile->m_BottomRight.x = x*Scale + Scale;
	pTmpTile->m_BottomRight.y = y*Scale + Scale;
}

bool CMapLayers::STileLayerVisuals::Init(unsigned int Width, unsigned int Height) 
{
	m_Width = Width;
	m_Height = Height;
	if(Width == 0 || Height == 0)
		return false;
	
	m_TilesOfLayer = new CMapLayers::STileLayerVisuals::STileVisual[Height*Width];
	
	if(Width > 2)
	{
		m_BorderTop = new CMapLayers::STileLayerVisuals::STileVisual[Width-2];
		m_BorderBottom = new CMapLayers::STileLayerVisuals::STileVisual[Width-2];
	}
	if(Height > 2)
	{
		m_BorderLeft = new CMapLayers::STileLayerVisuals::STileVisual[Height-2];
		m_BorderRight = new CMapLayers::STileLayerVisuals::STileVisual[Height-2];
	}
	return true;
}

CMapLayers::STileLayerVisuals::~STileLayerVisuals() 
{
	if(m_TilesOfLayer)
	{
		delete[] m_TilesOfLayer;
	}
	
	if(m_BorderTop)
		delete[] m_BorderTop;
	if(m_BorderBottom)
		delete[] m_BorderBottom;
	if(m_BorderLeft)
		delete[] m_BorderLeft;
	if(m_BorderRight)
		delete[] m_BorderRight;
	
	m_TilesOfLayer = NULL;
	m_BorderTop = NULL;
	m_BorderBottom = NULL;
	m_BorderLeft = NULL;
	m_BorderRight = NULL;
}

bool AddTile(std::vector<STmpTile>& TmpTiles, std::vector<STmpTileTexCoord>& TmpTileTexCoords, unsigned char Index, unsigned char Flags, int x, int y, CMapItemGroup* pGroup, bool DoTextureCoords, bool FillSpeedup = false, int AngleRotate = -1)
{
	if(Index)
	{
		TmpTiles.push_back(STmpTile());
		STmpTile& Tile = TmpTiles.back();
		STmpTileTexCoord* pTileTex = NULL;
		if(DoTextureCoords)
		{
			TmpTileTexCoords.push_back(STmpTileTexCoord());
			STmpTileTexCoord& TileTex = TmpTileTexCoords.back();
			pTileTex = &TileTex;
		}
		if(FillSpeedup)
			FillTmpTileSpeedup(&Tile, pTileTex, Flags, 0, x, y, 32.f, pGroup, AngleRotate);
		else
			FillTmpTile(&Tile, pTileTex, Flags, Index, x, y, 32.f, pGroup);
		
		return true;
	}
	return false;
}

struct STmpQuadVertexTextured
{
	float m_X, m_Y, m_CenterX, m_CenterY;
	float m_U, m_V;
	unsigned char m_R, m_G, m_B, m_A;
};

struct STmpQuadVertex
{
	float m_X, m_Y, m_CenterX, m_CenterY;
	unsigned char m_R, m_G, m_B, m_A;
};

struct STmpQuad
{
	STmpQuadVertex m_aVertices[4];
};

struct STmpQuadTextured
{
	STmpQuadVertexTextured m_aVertices[4];
};

void mem_copy_special(void *pDest, void *pSource, size_t Size, size_t Count, size_t Steps)
{
	size_t CurStep = 0;
	for(size_t i = 0; i < Count; ++i)
	{
		mem_copy(((char*)pDest) + CurStep + i * Size, ((char*)pSource) + i * Size, Size);
		CurStep += Steps;
	}
}

void CMapLayers::OnMapLoad()
{
	if(true)
		return;
	//clear everything and destroy all buffers
	if(m_TileLayerVisuals.size() != 0)
	{
		int s = m_TileLayerVisuals.size();
		for(int i = 0; i < s; ++i)
		{
			delete m_TileLayerVisuals[i];
		}
		m_TileLayerVisuals.clear();
	}
	if(m_QuadLayerVisuals.size() != 0)
	{
		int s = m_QuadLayerVisuals.size();
		for(int i = 0; i < s; ++i)
		{
			delete m_QuadLayerVisuals[i];
		}
		m_QuadLayerVisuals.clear();
	}

	CServerInfo Info;
	Client()->GetServerInfo(&Info);
	
	bool PassedGameLayer = false;
	//prepare all visuals for all tile layers	
	std::vector<STmpTile> tmpTiles;
	std::vector<STmpTileTexCoord> tmpTileTexCoords;
	std::vector<STmpTile> tmpBorderTopTiles;
	std::vector<STmpTileTexCoord> tmpBorderTopTilesTexCoords;
	std::vector<STmpTile> tmpBorderLeftTiles;
	std::vector<STmpTileTexCoord> tmpBorderLeftTilesTexCoords;
	std::vector<STmpTile> tmpBorderRightTiles;
	std::vector<STmpTileTexCoord> tmpBorderRightTilesTexCoords;
	std::vector<STmpTile> tmpBorderBottomTiles;
	std::vector<STmpTileTexCoord> tmpBorderBottomTilesTexCoords;
	std::vector<STmpTile> tmpBorderCorners;
	std::vector<STmpTileTexCoord> tmpBorderCornersTexCoords;

	std::vector<STmpQuad> tmpQuads;
	std::vector<STmpQuadTextured> tmpQuadsTextured;

	for(int g = 0; g < m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);
		if(!pGroup)
		{
			dbg_msg("maplayers", "error group was null, group number = %d, total groups = %d", g, m_pLayers->NumGroups());
			dbg_msg("maplayers", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("maplayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}
		
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+l);
			bool IsFrontLayer = false;
			bool IsSwitchLayer = false;
			bool IsTeleLayer = false;
			bool IsSpeedupLayer = false;
			bool IsTuneLayer = false;
			bool IsGameLayer = false;
			bool IsEntityLayer = false;
			
			if(pLayer == (CMapItemLayer*)m_pLayers->GameLayer())
			{
				IsGameLayer = true;
				IsEntityLayer = true;
				PassedGameLayer = true;
			}
			
			if(pLayer == (CMapItemLayer*)m_pLayers->FrontLayer())
				IsEntityLayer = IsFrontLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->SwitchLayer())
				IsEntityLayer = IsSwitchLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->TeleLayer())
				IsEntityLayer = IsTeleLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->SpeedupLayer())
				IsEntityLayer = IsSpeedupLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->TuneLayer())
				IsEntityLayer = IsTuneLayer = true;
			
			if(m_Type <= TYPE_BACKGROUND_FORCE)
			{
				if(PassedGameLayer)
					return;
			}
			else if(m_Type == TYPE_FOREGROUND)
			{
				if(!PassedGameLayer)
					continue;
			}
			
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{				
				bool DoTextureCoords = false;
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				if(pTMap->m_Image == -1)
				{
					if(IsEntityLayer)
						DoTextureCoords = true;
				}
				else
					DoTextureCoords = true;
				

				int DataIndex = 0;
				unsigned int TileSize = 0;
				int OverlayCount = 0;
				if(IsFrontLayer)
				{
					DataIndex = pTMap->m_Front;
					TileSize = sizeof(CTile);
				}
				else if(IsSwitchLayer)
				{
					DataIndex = pTMap->m_Switch;
					TileSize = sizeof(CSwitchTile);
					OverlayCount = 2;
				}
				else if(IsTeleLayer)
				{
					DataIndex = pTMap->m_Tele;
					TileSize = sizeof(CTeleTile);
					OverlayCount = 1;
				}
				else if(IsSpeedupLayer)
				{
					DataIndex = pTMap->m_Speedup;
					TileSize = sizeof(CSpeedupTile);
					OverlayCount = 2;
				}
				else if(IsTuneLayer)
				{
					DataIndex = pTMap->m_Tune;
					TileSize = sizeof(CTuneTile);
				}
				else
				{
					DataIndex = pTMap->m_Data;
					TileSize = sizeof(CTile);
				}
				unsigned int Size = m_pLayers->Map()->GetDataSize(DataIndex);
				void* pTiles = (void*)m_pLayers->Map()->GetData(DataIndex);

				if(Size >= pTMap->m_Width*pTMap->m_Height*TileSize)
				{
					int CurOverlay = 0;
					while(CurOverlay < OverlayCount + 1)
					{
						// We can later just count the tile layers to get the idx in the vector
						m_TileLayerVisuals.push_back(new STileLayerVisuals());
						STileLayerVisuals& Visuals = *m_TileLayerVisuals.back();
						if(!Visuals.Init(pTMap->m_Width, pTMap->m_Height))
						{
							++CurOverlay;
							continue;
						}
						Visuals.m_IsTextured = DoTextureCoords;
						
						tmpTiles.clear();
						tmpTileTexCoords.clear();
						
						tmpBorderTopTiles.clear();
						tmpBorderLeftTiles.clear();
						tmpBorderRightTiles.clear();
						tmpBorderBottomTiles.clear();	
						tmpBorderCorners.clear();	
						tmpBorderTopTilesTexCoords.clear();
						tmpBorderLeftTilesTexCoords.clear();
						tmpBorderRightTilesTexCoords.clear();
						tmpBorderBottomTilesTexCoords.clear();	
						tmpBorderCornersTexCoords.clear();
						
						int x = 0;
						int y = 0;
						for(y = 0; y < pTMap->m_Height; ++y)
						{
							for(x = 0; x < pTMap->m_Width; ++x)
							{
								
								unsigned char Index = 0;
								unsigned char Flags = 0;
								int AngleRotate = -1;
								if(IsEntityLayer)
								{								
									if(IsGameLayer)
									{ 
										Index = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Index;
										Flags = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Flags;
										if(IsDDNet(&Info) && !IsValidGameTile(Index))
											Index = 0;
									}
									if(IsFrontLayer)
									{ 
										Index = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Index;
										Flags = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Flags;
										if(!IsValidFrontTile(Index))
											Index = 0;
									}
									if(IsSwitchLayer)
									{
										Flags = 0;
										Index = ((CSwitchTile*)pTiles)[y*pTMap->m_Width+x].m_Type;
										if(!IsValidSwitchTile(Index))
											Index = 0;
										else if(CurOverlay == 0)
										{
											Flags = ((CSwitchTile*)pTiles)[y*pTMap->m_Width+x].m_Flags;
											if(Index == TILE_SWITCHTIMEDOPEN) Index = 8;
										}
										else if(CurOverlay == 1)
											Index = ((CSwitchTile*)pTiles)[y*pTMap->m_Width+x].m_Number;
										else if(CurOverlay == 2)
											Index = ((CSwitchTile*)pTiles)[y*pTMap->m_Width+x].m_Delay;
									}
									if(IsTeleLayer)
									{
										Index = ((CTeleTile*)pTiles)[y*pTMap->m_Width+x].m_Type;
										Flags = 0;
										if(!IsValidTeleTile(Index))
											Index = 0;
										else if(CurOverlay == 1)
										{
											if(Index != TILE_TELECHECKIN && Index != TILE_TELECHECKINEVIL)
												Index = ((CTeleTile*)pTiles)[y*pTMap->m_Width+x].m_Number;
											else Index = 0;
										}
									}
									if(IsSpeedupLayer)
									{
										Index = ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_Type;
										Flags = 0;
										AngleRotate = ((CSpeedupTile*)pTiles)[y*pTMap->m_Width + x].m_Angle;
										if(!IsValidSpeedupTile(Index) || ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_Force == 0)
											Index = 0;
										else if(CurOverlay == 1)
											Index = ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_Force;
										else if(CurOverlay == 2)
											Index = ((CSpeedupTile*)pTiles)[y*pTMap->m_Width+x].m_MaxSpeed;
									}
									if(IsTuneLayer)
									{							
										Index = ((CTuneTile*)pTiles)[y*pTMap->m_Width+x].m_Type;
										if(Index != TILE_TUNE1)
											Index = 0;		
										Flags = 0;
									}
								} else
								{
									Index = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Index;
									Flags = ((CTile*)pTiles)[y*pTMap->m_Width+x].m_Flags;
								}
								
								//the amount of tiles handled before this tile
								int TilesHandledCount = tmpTiles.size();
								Visuals.m_TilesOfLayer[y*pTMap->m_Width + x].SetIndexBufferByteOffset((offset_ptr32)(TilesHandledCount*6*sizeof(unsigned int)));
									
								bool AddAsSpeedup = false;
								if(IsSpeedupLayer && CurOverlay == 0)
									AddAsSpeedup = true;

								if(AddTile(tmpTiles, tmpTileTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
									Visuals.m_TilesOfLayer[y*pTMap->m_Width + x].Draw(true);
								
								//do the border tiles
								if(x == 0)
								{
									if(y == 0)
									{
										Visuals.m_BorderTopLeft.SetIndexBufferByteOffset((offset_ptr32)(tmpBorderCorners.size()*6*sizeof(unsigned int)));
										if(AddTile(tmpBorderCorners, tmpBorderCornersTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
											Visuals.m_BorderTopLeft.Draw(true);
									}
									else if(y == pTMap->m_Height - 1)
									{
										Visuals.m_BorderBottomLeft.SetIndexBufferByteOffset((offset_ptr32)(tmpBorderCorners.size()*6*sizeof(unsigned int)));
										if(AddTile(tmpBorderCorners, tmpBorderCornersTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
											Visuals.m_BorderBottomLeft.Draw(true);
											
									}
									else 
									{
										Visuals.m_BorderLeft[y-1].SetIndexBufferByteOffset((offset_ptr32)(tmpBorderLeftTiles.size()*6*sizeof(unsigned int)));
										if(AddTile(tmpBorderLeftTiles, tmpBorderLeftTilesTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
											Visuals.m_BorderLeft[y-1].Draw(true);
									}
								}
								else if(x == pTMap->m_Width - 1)
								{
									if(y == 0)
									{
										Visuals.m_BorderTopRight.SetIndexBufferByteOffset((offset_ptr32)(tmpBorderCorners.size()*6*sizeof(unsigned int)));
										if(AddTile(tmpBorderCorners, tmpBorderCornersTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
											Visuals.m_BorderTopRight.Draw(true);
											
									}
									else if(y == pTMap->m_Height - 1)
									{
										Visuals.m_BorderBottomRight.SetIndexBufferByteOffset((offset_ptr32)(tmpBorderCorners.size()*6*sizeof(unsigned int)));
										if(AddTile(tmpBorderCorners, tmpBorderCornersTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
											Visuals.m_BorderBottomRight.Draw(true);
											
									}
									else
									{
										Visuals.m_BorderRight[y-1].SetIndexBufferByteOffset((offset_ptr32)(tmpBorderRightTiles.size()*6*sizeof(unsigned int)));
										if(AddTile(tmpBorderRightTiles, tmpBorderRightTilesTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
											Visuals.m_BorderRight[y-1].Draw(true);
									}
								}
								else if(y == 0)
								{
									if(x > 0 && x < pTMap->m_Width - 1)
									{
										Visuals.m_BorderTop[x-1].SetIndexBufferByteOffset((offset_ptr32)(tmpBorderTopTiles.size()*6*sizeof(unsigned int)));
										if(AddTile(tmpBorderTopTiles, tmpBorderTopTilesTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
											Visuals.m_BorderTop[x-1].Draw(true);
									}
								}
								else if(y == pTMap->m_Height - 1)
								{
									if(x > 0 && x < pTMap->m_Width - 1)
									{
										Visuals.m_BorderBottom[x-1].SetIndexBufferByteOffset((offset_ptr32)(tmpBorderBottomTiles.size()*6*sizeof(unsigned int)));
										if(AddTile(tmpBorderBottomTiles, tmpBorderBottomTilesTexCoords, Index, Flags, x, y, pGroup, DoTextureCoords, AddAsSpeedup, AngleRotate))
											Visuals.m_BorderBottom[x-1].Draw(true);
									}
								}
							}
						}
												
						//append one kill tile to the gamelayer
						if(IsGameLayer)
						{
							Visuals.m_BorderKillTile.SetIndexBufferByteOffset((offset_ptr32)(tmpTiles.size() * 6 * sizeof(unsigned int)));
							if(AddTile(tmpTiles, tmpTileTexCoords, TILE_DEATH, 0, 0, 0, pGroup, DoTextureCoords))								
								Visuals.m_BorderKillTile.Draw(true);
						}
						
						//add the border corners, then the borders and fix their byte offsets
						int TilesHandledCount = tmpTiles.size();
						Visuals.m_BorderTopLeft.AddIndexBufferByteOffset(TilesHandledCount*6*sizeof(unsigned int));
						Visuals.m_BorderTopRight.AddIndexBufferByteOffset(TilesHandledCount*6*sizeof(unsigned int));
						Visuals.m_BorderBottomLeft.AddIndexBufferByteOffset(TilesHandledCount*6*sizeof(unsigned int));
						Visuals.m_BorderBottomRight.AddIndexBufferByteOffset(TilesHandledCount*6*sizeof(unsigned int));
						//add the Corners to the tiles
						tmpTiles.insert(tmpTiles.end(), tmpBorderCorners.begin(), tmpBorderCorners.end());
						tmpTileTexCoords.insert(tmpTileTexCoords.end(), tmpBorderCornersTexCoords.begin(), tmpBorderCornersTexCoords.end());
						
						//now the borders
						TilesHandledCount = tmpTiles.size();
						if(pTMap->m_Width > 2)
						{
							for(int i = 0; i < pTMap->m_Width-2; ++i)
							{
								Visuals.m_BorderTop[i].AddIndexBufferByteOffset(TilesHandledCount * 6 * sizeof(unsigned int));
							}
						}
						tmpTiles.insert(tmpTiles.end(), tmpBorderTopTiles.begin(), tmpBorderTopTiles.end());
						tmpTileTexCoords.insert(tmpTileTexCoords.end(), tmpBorderTopTilesTexCoords.begin(), tmpBorderTopTilesTexCoords.end());
						
						TilesHandledCount = tmpTiles.size();
						if(pTMap->m_Width > 2)
						{
							for(int i = 0; i < pTMap->m_Width-2; ++i)
							{
								Visuals.m_BorderBottom[i].AddIndexBufferByteOffset(TilesHandledCount * 6 * sizeof(unsigned int));
							}
						}
						tmpTiles.insert(tmpTiles.end(), tmpBorderBottomTiles.begin(), tmpBorderBottomTiles.end());
						tmpTileTexCoords.insert(tmpTileTexCoords.end(), tmpBorderBottomTilesTexCoords.begin(), tmpBorderBottomTilesTexCoords.end());
						
						TilesHandledCount = tmpTiles.size();
						if(pTMap->m_Height > 2)
						{
							for(int i = 0; i < pTMap->m_Height-2; ++i)
							{
								Visuals.m_BorderLeft[i].AddIndexBufferByteOffset(TilesHandledCount * 6 * sizeof(unsigned int));
							}
						}
						tmpTiles.insert(tmpTiles.end(), tmpBorderLeftTiles.begin(), tmpBorderLeftTiles.end());
						tmpTileTexCoords.insert(tmpTileTexCoords.end(), tmpBorderLeftTilesTexCoords.begin(), tmpBorderLeftTilesTexCoords.end());
						
						TilesHandledCount = tmpTiles.size();
						if(pTMap->m_Height > 2)
						{
							for(int i = 0; i < pTMap->m_Height-2; ++i)
							{
								Visuals.m_BorderRight[i].AddIndexBufferByteOffset(TilesHandledCount * 6 * sizeof(unsigned int));
							}
						}
						tmpTiles.insert(tmpTiles.end(), tmpBorderRightTiles.begin(), tmpBorderRightTiles.end());
						tmpTileTexCoords.insert(tmpTileTexCoords.end(), tmpBorderRightTilesTexCoords.begin(), tmpBorderRightTilesTexCoords.end());
						
						//setup params
						float* pTmpTiles = (tmpTiles.size() == 0) ? NULL : (float*)&tmpTiles[0];
						unsigned char* pTmpTileTexCoords = (tmpTileTexCoords.size() == 0) ? NULL : (unsigned char*)&tmpTileTexCoords[0];

						size_t UploadDataSize = tmpTileTexCoords.size() * sizeof(STmpTileTexCoord) + tmpTiles.size() * sizeof(STmpTile);
						char* pUploadData = new char[UploadDataSize];

						mem_copy_special(pUploadData, pTmpTiles, sizeof(vec2), tmpTiles.size() * 4, (DoTextureCoords ? (sizeof(unsigned char) * 2 * 2) : 0));
						if(DoTextureCoords)
						{
							mem_copy_special(pUploadData + sizeof(vec2), pTmpTileTexCoords, sizeof(unsigned char) * 2 * 2, tmpTiles.size() * 4, (DoTextureCoords ? (sizeof(vec2)) : 0));
						}

						// first create the buffer object
						int BufferObjectIndex = 0;
						delete[] pUploadData;
						++CurOverlay;
					}
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{

      }
		}
	}
}

void CMapLayers::RenderTileLayer(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup)
{
	STileLayerVisuals& Visuals = *m_TileLayerVisuals[LayerIndex];
	if(Visuals.m_BufferContainerIndex == -1)
		return; //no visuals were created
	
	float ScreenX0 = 0.0f, ScreenY0 = 0.0f, ScreenX1 = 10.0f, ScreenY1 = 10.0f;
	
	float r=1, g=1, b=1, a=1;
	if(pTileLayer->m_ColorEnv >= 0)
	{
		float aChannels[4];
		EnvelopeEval(pTileLayer->m_ColorEnvOffset/1000.0f, pTileLayer->m_ColorEnv, aChannels, this);
		r = aChannels[0];
		g = aChannels[1];
		b = aChannels[2];
		a = aChannels[3];
	}
	
	int BorderX0, BorderY0, BorderX1, BorderY1;
	bool DrawBorder = false;
	
	int Y0 = BorderY0 = (int)floorf((ScreenY0)/32);
	int X0 = BorderX0 = (int)floorf((ScreenX0)/32);
	int Y1 = BorderY1 = (int)floorf((ScreenY1)/32);
	int X1 = BorderX1 = (int)floorf((ScreenX1)/32);
	
	if(X0 <= 0) 
	{
		X0 = 0;
		DrawBorder = true;
	}
	if(Y0 <= 0)
	{
		Y0 = 0;
		DrawBorder = true;
	}
	if(X1 >= pTileLayer->m_Width - 1)
	{
		X1 = pTileLayer->m_Width - 1;
		DrawBorder = true;
	}
	if(Y1 >= pTileLayer->m_Height - 1)
	{
		Y1 = pTileLayer->m_Height - 1;
		DrawBorder = true;
	}
	
	bool DrawLayer = true;
	if(X1 < 0)
		DrawLayer = false;
	if(Y1 < 0)
		DrawLayer = false;
	if(X0 >= pTileLayer->m_Width)
		DrawLayer = false;
	if(Y0 >= pTileLayer->m_Height)
		DrawLayer = false;
	
	if(DrawLayer)
	{
		//create the indice buffers we want to draw -- reuse them
		static std::vector<char*> s_IndexOffsets;
		static std::vector<unsigned int> s_DrawCounts;
		
		s_IndexOffsets.clear();
		s_DrawCounts.clear();
		
		unsigned long long Reserve = absolute(Y1 - Y0) + 1;
		s_IndexOffsets.reserve(Reserve);
		s_DrawCounts.reserve(Reserve);
		
		for(int y = Y0; y <= Y1; ++y)
		{
			if(X0 > X1) 
				continue;
			
			dbg_assert(Visuals.m_TilesOfLayer[y*pTileLayer->m_Width + X1].IndexBufferByteOffset() >= Visuals.m_TilesOfLayer[y*pTileLayer->m_Width + X0].IndexBufferByteOffset(), "Tile count wrong.");
			
			unsigned int NumVertices = ((Visuals.m_TilesOfLayer[y*pTileLayer->m_Width + X1].IndexBufferByteOffset() - Visuals.m_TilesOfLayer[y*pTileLayer->m_Width + X0].IndexBufferByteOffset()) / sizeof(unsigned int)) + (Visuals.m_TilesOfLayer[y*pTileLayer->m_Width + X1].DoDraw() ? 6lu : 0lu);
			
			if(NumVertices)
			{
				s_IndexOffsets.push_back((offset_ptr_size)Visuals.m_TilesOfLayer[y*pTileLayer->m_Width + X0].IndexBufferByteOffset());
				s_DrawCounts.push_back(NumVertices);
			}
		}
		
		pColor->x *= r;
		pColor->y *= g;
		pColor->z *= b;
		pColor->w *= a;
	}
}

void CMapLayers::RenderTileBorderCornerTiles(int WidthOffsetToOrigin, int HeightOffsetToOrigin, int TileCountWidth, int TileCountHeight, int BufferContainerIndex, float* pColor, offset_ptr_size IndexBufferOffset, float* pOffset, float* pDir)
{
	// if border is still in range of the original corner, it doesn't needs to be redrawn
	bool CornerVisible = (WidthOffsetToOrigin - 1 < TileCountWidth) && (HeightOffsetToOrigin - 1 < TileCountHeight);

	int CountX = min(WidthOffsetToOrigin, TileCountWidth);
	int CountY = min(HeightOffsetToOrigin, TileCountHeight);
}

void CMapLayers::RenderTileBorder(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup, int BorderX0, int BorderY0, int BorderX1, int BorderY1, int ScreenWidthTileCount, int ScreenHeightTileCount)
{
	STileLayerVisuals& Visuals = *m_TileLayerVisuals[LayerIndex];
	
	int Y0 = BorderY0;
	int X0 = BorderX0;
	int Y1 = BorderY1;
	int X1 = BorderX1;
	
	int CountWidth = ScreenWidthTileCount;
	int CountHeight = ScreenHeightTileCount;

	if(X0 < 1)
		X0 = 1;
	if(Y0 < 1)
		Y0 = 1;
	if(X1 >= pTileLayer->m_Width - 1)
		X1 = pTileLayer->m_Width - 2;
	if(Y1 >= pTileLayer->m_Height - 1)
		Y1 = pTileLayer->m_Height - 2;
	
	if(BorderX0 <= 0)
	{
		// Draw corners on left side
		if(BorderY0 <= 0)
		{
			if(Visuals.m_BorderTopLeft.DoDraw())
			{
				vec2 Offset;
				Offset.x = BorderX0 * 32.f;
				Offset.y = BorderY0 * 32.f;
				vec2 Dir;
				Dir.x = 32.f;
				Dir.y = 32.f;

				RenderTileBorderCornerTiles(absolute(BorderX0) + 1, absolute(BorderY0) + 1, CountWidth, CountHeight, Visuals.m_BufferContainerIndex, (float*)pColor, (offset_ptr_size)Visuals.m_BorderTopLeft.IndexBufferByteOffset(), (float*)&Offset, (float*)&Dir);
			}
		}
		if(BorderY1 >= pTileLayer->m_Height - 1)
		{
			if(Visuals.m_BorderBottomLeft.DoDraw())
			{
				vec2 Offset;
				Offset.x = BorderX0 * 32.f;
				Offset.y = (BorderY1 - (pTileLayer->m_Height - 1)) * 32.f;
				vec2 Dir;
				Dir.x = 32.f;
				Dir.y = -32.f;

				RenderTileBorderCornerTiles(absolute(BorderX0) + 1, (BorderY1 - (pTileLayer->m_Height - 1)) + 1, CountWidth, CountHeight, Visuals.m_BufferContainerIndex, (float*)pColor, (offset_ptr_size)Visuals.m_BorderBottomLeft.IndexBufferByteOffset(), (float*)&Offset, (float*)&Dir);
			}
		}
	}
	if(BorderX0 < 0)
	{
		// Draw left border
		if(Y0 < pTileLayer->m_Height - 1 && Y1 > 0)
		{
			unsigned int DrawNum = ((Visuals.m_BorderLeft[Y1 - 1].IndexBufferByteOffset() - Visuals.m_BorderLeft[Y0 - 1].IndexBufferByteOffset()) / sizeof(unsigned int)) + (Visuals.m_BorderLeft[Y1 - 1].DoDraw() ? 6lu : 0lu);
			offset_ptr_size pOffset = (offset_ptr_size)Visuals.m_BorderLeft[Y0-1].IndexBufferByteOffset();
			vec2 Offset;
			Offset.x = 32.f * BorderX0;
			Offset.y = 0.f;
			vec2 Dir;
			Dir.x = 32.f;
			Dir.y = 0.f;
		}
	}
	
	if(BorderX1 >= pTileLayer->m_Width - 1)
	{
		// Draw corners on right side
		if(BorderY0 <= 0)
		{
			if(Visuals.m_BorderTopRight.DoDraw())
			{
				vec2 Offset;
				Offset.x = (BorderX1 - (pTileLayer->m_Width - 1)) * 32.f;
				Offset.y = BorderY0 * 32.f;
				vec2 Dir;
				Dir.x = -32.f;
				Dir.y = 32.f;

				RenderTileBorderCornerTiles((BorderX1 - (pTileLayer->m_Width - 1)) + 1, absolute(BorderY0) + 1, CountWidth, CountHeight, Visuals.m_BufferContainerIndex, (float*)pColor, (offset_ptr_size)Visuals.m_BorderTopRight.IndexBufferByteOffset(), (float*)&Offset, (float*)&Dir);
			}
		}
		if(BorderY1 >= pTileLayer->m_Height - 1)
		{
			if(Visuals.m_BorderBottomRight.DoDraw())
			{
				vec2 Offset;
				Offset.x = (BorderX1 - (pTileLayer->m_Width - 1)) * 32.f;
				Offset.y = (BorderY1 - (pTileLayer->m_Height - 1)) * 32.f;
				vec2 Dir;
				Dir.x = -32.f;
				Dir.y = -32.f;

				RenderTileBorderCornerTiles((BorderX1 - (pTileLayer->m_Width - 1)) + 1, (BorderY1 - (pTileLayer->m_Height - 1)) + 1, CountWidth, CountHeight, Visuals.m_BufferContainerIndex, (float*)pColor, (offset_ptr_size)Visuals.m_BorderBottomRight.IndexBufferByteOffset(), (float*)&Offset, (float*)&Dir);
			}
		}
	}
	if(BorderX1 > pTileLayer->m_Width - 1)
	{
		// Draw right border
		if(Y0 < pTileLayer->m_Height - 1 && Y1 > 0)
		{
			unsigned int DrawNum = ((Visuals.m_BorderRight[Y1 - 1].IndexBufferByteOffset() - Visuals.m_BorderRight[Y0 - 1].IndexBufferByteOffset()) / sizeof(unsigned int)) + (Visuals.m_BorderRight[Y1 - 1].DoDraw() ? 6lu : 0lu);
			offset_ptr_size pOffset = (offset_ptr_size)Visuals.m_BorderRight[Y0-1].IndexBufferByteOffset();
			vec2 Offset;
			Offset.x = 32.f * (BorderX1 - (pTileLayer->m_Width - 1));
			Offset.y = 0.f;
			vec2 Dir;
			Dir.x = -32.f;
			Dir.y = 0.f;
		}
	}
	if(BorderY0 < 0)
	{
		// Draw top border
		if(X0 < pTileLayer->m_Width - 1 && X1 > 0)
		{			
			unsigned int DrawNum = ((Visuals.m_BorderTop[X1 - 1].IndexBufferByteOffset() - Visuals.m_BorderTop[X0 - 1].IndexBufferByteOffset()) / sizeof(unsigned int)) + (Visuals.m_BorderTop[X1 - 1].DoDraw() ? 6lu : 0lu);
			offset_ptr_size pOffset = (offset_ptr_size)Visuals.m_BorderTop[X0-1].IndexBufferByteOffset();
			vec2 Offset;
			Offset.x = 0.f;
			Offset.y = 32.f * BorderY0;
			vec2 Dir;
			Dir.x = 0.f;
			Dir.y = 32.f;
		}
	}
	if(BorderY1 >= pTileLayer->m_Height)
	{
		// Draw bottom border
		if(X0 < pTileLayer->m_Width - 1 && X1 > 0)
		{
			unsigned int DrawNum = ((Visuals.m_BorderBottom[X1 - 1].IndexBufferByteOffset() - Visuals.m_BorderBottom[X0 - 1].IndexBufferByteOffset()) / sizeof(unsigned int)) + (Visuals.m_BorderBottom[X1 - 1].DoDraw() ? 6lu : 0lu);
			offset_ptr_size pOffset = (offset_ptr_size)Visuals.m_BorderBottom[X0-1].IndexBufferByteOffset();
			vec2 Offset;
			Offset.x = 0.f;
			Offset.y = 32.f * (BorderY1 - (pTileLayer->m_Height - 1));
			vec2 Dir;
			Dir.x = 0.f;
			Dir.y = -32.f;
		}
	}
}

void CMapLayers::RenderKillTileBorder(int LayerIndex, vec4* pColor, CMapItemLayerTilemap* pTileLayer, CMapItemGroup* pGroup)
{
	STileLayerVisuals& Visuals = *m_TileLayerVisuals[LayerIndex];
	if(Visuals.m_BufferContainerIndex == -1) return; //no visuals were created
	
	float ScreenX0 = 0.0f, ScreenY0 = 0.0f, ScreenX1 = 10.0f, ScreenY1 = 10.0f;
	
	bool DrawBorder = false;
	
	int BorderY0 = (int)(ScreenY0/32)-1;
	int BorderX0 = (int)(ScreenX0/32)-1;
	int BorderY1 = (int)(ScreenY1/32)+1;
	int BorderX1 = (int)(ScreenX1/32)+1;
	
	if(BorderX0 < -201)
		DrawBorder = true;
	if(BorderY0 < -201)
		DrawBorder = true;
	if(BorderX1 >= pTileLayer->m_Width + 201)
		DrawBorder = true;
	if(BorderY1 >= pTileLayer->m_Height + 201)
		DrawBorder = true;
	
	if(!DrawBorder)
		return;
	if(!Visuals.m_BorderKillTile.DoDraw())
		return;
	
	if(BorderX0 < -300)
		BorderX0 = -300;
	if(BorderY0 < -300)
		BorderY0 = -300;
	if(BorderX1 >= pTileLayer->m_Width + 300)
		BorderX1 = pTileLayer->m_Width + 299;
	if(BorderY1 >= pTileLayer->m_Height + 300)
		BorderY1 = pTileLayer->m_Height + 299;	
	
	if(BorderX1 < -300)
		BorderX1 = -300;
	if(BorderY1 < -300)
		BorderY1 = -300;
	if(BorderX0 >= pTileLayer->m_Width + 300)
		BorderX0 = pTileLayer->m_Width + 299;
	if(BorderY0 >= pTileLayer->m_Height + 300)
		BorderY0 = pTileLayer->m_Height + 299;	
	
	// Draw left kill tile border
	if(BorderX0 < -201)
	{
		vec2 Offset;
		Offset.x = BorderX0 * 32.f;
		Offset.y = BorderY0 * 32.f;
		vec2 Dir;
		Dir.x = 32.f;
		Dir.y = 32.f;
	}
	// Draw top kill tile border
	if(BorderY0 < -201)
	{
		vec2 Offset;
		int OffX0 = (BorderX0 < -201 ? -201 : BorderX0);
		int OffX1 = (BorderX1 >= pTileLayer->m_Width + 201 ? pTileLayer->m_Width + 201 : BorderX1);
		Offset.x = OffX0 * 32.f;
		Offset.y = BorderY0 * 32.f;
		vec2 Dir;
		Dir.x = 32.f;
		Dir.y = 32.f;
	}
	if(BorderX1 >= pTileLayer->m_Width + 201)
	{
		vec2 Offset;
		Offset.x = (pTileLayer->m_Width + 201) * 32.f;
		Offset.y = BorderY0 * 32.f;
		vec2 Dir;
		Dir.x = 32.f;
		Dir.y = 32.f;
	}
	if(BorderY1 >= pTileLayer->m_Height + 201)
	{
		vec2 Offset;
		int OffX0 = (BorderX0 < -201 ? -201 : BorderX0);
		int OffX1 = (BorderX1 >= pTileLayer->m_Width + 201 ? pTileLayer->m_Width + 201 : BorderX1);
		Offset.x = OffX0 * 32.f;
		Offset.y = (pTileLayer->m_Height + 201) * 32.f;
		vec2 Dir;
		Dir.x = 32.f;
		Dir.y = 32.f;
	}
}

void CMapLayers::RenderQuadLayer(int LayerIndex, CMapItemLayerQuads* pQuadLayer, CMapItemGroup* pGroup, bool Force)
{

}

void CMapLayers::LayersOfGroupCount(CMapItemGroup* pGroup, int& TileLayerCount, int& QuadLayerCount, bool& PassedGameLayer)
{	
	int TileLayerCounter = 0;
	int QuadLayerCounter = 0;
	for(int l = 0; l < pGroup->m_NumLayers; l++)
	{
		CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+l);
		bool IsFrontLayer = false;
		bool IsSwitchLayer = false;
		bool IsTeleLayer = false;
		bool IsSpeedupLayer = false;
		bool IsTuneLayer = false;

		if(pLayer == (CMapItemLayer*)m_pLayers->GameLayer())
		{
			PassedGameLayer = true;
		}

		if(pLayer == (CMapItemLayer*)m_pLayers->FrontLayer())
			IsFrontLayer = true;

		if(pLayer == (CMapItemLayer*)m_pLayers->SwitchLayer())
			IsSwitchLayer = true;

		if(pLayer == (CMapItemLayer*)m_pLayers->TeleLayer())
			IsTeleLayer = true;

		if(pLayer == (CMapItemLayer*)m_pLayers->SpeedupLayer())
			IsSpeedupLayer = true;

		if(pLayer == (CMapItemLayer*)m_pLayers->TuneLayer())
			IsTuneLayer = true;
		

		if(m_Type <= TYPE_BACKGROUND_FORCE)
		{
			if(PassedGameLayer)
				break;
		}
		else if(m_Type == TYPE_FOREGROUND)
		{
			if(!PassedGameLayer)
				continue;
		}
		
		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
			int DataIndex = 0;
			unsigned int TileSize = 0;
			int TileLayerAndOverlayCount = 0;
			if(IsFrontLayer)
			{
				DataIndex = pTMap->m_Front;
				TileSize = sizeof(CTile);
				TileLayerAndOverlayCount = 1;
			}
			else if(IsSwitchLayer) 
			{
				DataIndex = pTMap->m_Switch;
				TileSize = sizeof(CSwitchTile);
				TileLayerAndOverlayCount = 3;
			}
			else if(IsTeleLayer) 
			{
				DataIndex = pTMap->m_Tele;
				TileSize = sizeof(CTeleTile);
				TileLayerAndOverlayCount = 2;
			}
			else if(IsSpeedupLayer) 
			{
				DataIndex = pTMap->m_Speedup;
				TileSize = sizeof(CSpeedupTile);
				TileLayerAndOverlayCount = 3;
			}
			else if(IsTuneLayer) 
			{
				DataIndex = pTMap->m_Tune;
				TileSize = sizeof(CTuneTile);
				TileLayerAndOverlayCount = 1;
			}
			else 
			{
				DataIndex = pTMap->m_Data;
				TileSize = sizeof(CTile);
				TileLayerAndOverlayCount = 1;
			}

			unsigned int Size = m_pLayers->Map()->GetDataSize(DataIndex);
			if(Size >= pTMap->m_Width*pTMap->m_Height*TileSize)
			{
				TileLayerCounter += TileLayerAndOverlayCount;
			}
		}
		else if(pLayer->m_Type == LAYERTYPE_QUADS)
		{
			++QuadLayerCounter;
		}
	}

	TileLayerCount += TileLayerCounter;
	QuadLayerCount += QuadLayerCounter;
}

void CMapLayers::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	vec2 Center = m_pClient->m_pCamera->m_Center;

	bool PassedGameLayer = false;
	int TileLayerCounter = 0;
	int QuadLayerCounter = 0;

	for(int g = 0; g < m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);

		if(!pGroup)
		{
			dbg_msg("maplayers", "error group was null, group number = %d, total groups = %d", g, m_pLayers->NumGroups());
			dbg_msg("maplayers", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("maplayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}

		if(!g_Config.m_GfxNoclip && pGroup->m_Version >= 2 && pGroup->m_UseClipping)
		{
			// set clipping
			float Points[4];
			MapScreenToGroup(Center.x, Center.y, m_pLayers->GameGroup(), m_pClient->m_pCamera->m_Zoom);
			float x0 = (pGroup->m_ClipX - Points[0]) / (Points[2]-Points[0]);
			float y0 = (pGroup->m_ClipY - Points[1]) / (Points[3]-Points[1]);
			float x1 = ((pGroup->m_ClipX+pGroup->m_ClipW) - Points[0]) / (Points[2]-Points[0]);
			float y1 = ((pGroup->m_ClipY+pGroup->m_ClipH) - Points[1]) / (Points[3]-Points[1]);

			if(x1 < 0.0f || x0 > 1.0f || y1 < 0.0f || y0 > 1.0f)
			{
				//check tile layer count of this group
				LayersOfGroupCount(pGroup, TileLayerCounter, QuadLayerCounter, PassedGameLayer);
				continue;
			}
		}

		if(!g_Config.m_ClZoomBackgroundLayers && !pGroup->m_ParallaxX && !pGroup->m_ParallaxY)
		{
			MapScreenToGroup(Center.x, Center.y, pGroup, 1.0);
		} else
			MapScreenToGroup(Center.x, Center.y, pGroup, m_pClient->m_pCamera->m_Zoom);

		// for(int l = pGroup->m_NumLayers + 1; l < pGroup->m_NumLayers; l++) // chillerbot
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer+l);
			bool Render = false;
			bool IsGameLayer = false;
			bool IsFrontLayer = false;
			bool IsSwitchLayer = false;
			bool IsTeleLayer = false;
			bool IsSpeedupLayer = false;
			bool IsTuneLayer = false;
			bool IsEntityLayer = false;

      // printf("current layer=%d\n", pLayer->m_Type);
			if(pLayer == (CMapItemLayer*)m_pLayers->GameLayer())
			{
				IsEntityLayer = IsGameLayer = true;
				PassedGameLayer = true;
        // printf("is game layer %d\n", pLayer->m_Type);
			}

			if(pLayer == (CMapItemLayer*)m_pLayers->FrontLayer())
				IsEntityLayer = IsFrontLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->SwitchLayer())
				IsEntityLayer = IsSwitchLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->TeleLayer())
				IsEntityLayer = IsTeleLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->SpeedupLayer())
				IsEntityLayer = IsSpeedupLayer = true;

			if(pLayer == (CMapItemLayer*)m_pLayers->TuneLayer())
				IsEntityLayer = IsTuneLayer = true;
			
			if(m_Type == -1)
				Render = true;
			else if(m_Type <= TYPE_BACKGROUND_FORCE)
			{
				if(PassedGameLayer)
					return;
				Render = true;
				
				if(m_Type == TYPE_BACKGROUND_FORCE)
				{
					if(pLayer->m_Type == LAYERTYPE_TILES && !g_Config.m_ClBackgroundShowTilesLayers)
						continue;
				}
			}
			else // TYPE_FOREGROUND
			{
				if(PassedGameLayer && !IsGameLayer)
					Render = true;
			}

			if(Render && pLayer->m_Type == LAYERTYPE_TILES && Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyIsPressed(KEY_LSHIFT) && Input()->KeyPress(KEY_KP_0))
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				CTile *pTiles = (CTile *)m_pLayers->Map()->GetData(pTMap->m_Data);
				CServerInfo CurrentServerInfo;
				Client()->GetServerInfo(&CurrentServerInfo);
				char aFilename[256];
				str_format(aFilename, sizeof(aFilename), "dumps/tilelayer_dump_%s-%d-%d-%dx%d.txt", CurrentServerInfo.m_aMap, g, l, pTMap->m_Width, pTMap->m_Height);
				IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
				if(File)
				{
					for(int y = 0; y < pTMap->m_Height; y++)
					{
						for(int x = 0; x < pTMap->m_Width; x++)
							io_write(File, &(pTiles[y*pTMap->m_Width + x].m_Index), sizeof(pTiles[y*pTMap->m_Width + x].m_Index));
						io_write_newline(File);
					}
					io_close(File);
				}
			}

			if((Render || IsGameLayer) && pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				int DataIndex = 0;
				unsigned int TileSize = 0;
				int TileLayerAndOverlayCount = 0;
				if(IsFrontLayer)
				{
					DataIndex = pTMap->m_Front;
					TileSize = sizeof(CTile);
					TileLayerAndOverlayCount = 1;
				}
				else if(IsSwitchLayer)
				{
					DataIndex = pTMap->m_Switch;
					TileSize = sizeof(CSwitchTile);
					TileLayerAndOverlayCount = 3;
				}
				else if(IsTeleLayer)
				{
					DataIndex = pTMap->m_Tele;
					TileSize = sizeof(CTeleTile);
					TileLayerAndOverlayCount = 2;
				}
				else if(IsSpeedupLayer)
				{
					DataIndex = pTMap->m_Speedup;
					TileSize = sizeof(CSpeedupTile);
					TileLayerAndOverlayCount = 3;
				}
				else if(IsTuneLayer)
				{
					DataIndex = pTMap->m_Tune;
					TileSize = sizeof(CTuneTile);
					TileLayerAndOverlayCount = 1;
				}
				else
				{
					DataIndex = pTMap->m_Data;
					TileSize = sizeof(CTile);
					TileLayerAndOverlayCount = 1;
				}

				unsigned int Size = m_pLayers->Map()->GetDataSize(DataIndex);
				if(Size >= pTMap->m_Width*pTMap->m_Height*TileSize)
				{
					TileLayerCounter += TileLayerAndOverlayCount;
				}
			}
			else if(Render && pLayer->m_Type == LAYERTYPE_QUADS)
			{
				++QuadLayerCounter;
			}

			// skip rendering if detail layers if not wanted, or is entity layer and we are a background map
			if((pLayer->m_Flags&LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && !IsGameLayer) || (m_Type == TYPE_BACKGROUND_FORCE && IsEntityLayer))
				continue;
			
			if((Render && g_Config.m_ClOverlayEntities < 100 && !IsGameLayer && !IsFrontLayer && !IsSwitchLayer && !IsTeleLayer && !IsSpeedupLayer && !IsTuneLayer) || (g_Config.m_ClOverlayEntities && IsGameLayer) || (m_Type == TYPE_BACKGROUND_FORCE))
			{
				if(pLayer->m_Type == LAYERTYPE_TILES)
				{
					CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;

					CTile *pTiles = (CTile *)m_pLayers->Map()->GetData(pTMap->m_Data);
					unsigned int Size = m_pLayers->Map()->GetDataSize(pTMap->m_Data);

					if(Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTile))
					{
						vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f);
						if(IsGameLayer && g_Config.m_ClOverlayEntities)
							Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*g_Config.m_ClOverlayEntities/100.0f);
						else if(!IsGameLayer && g_Config.m_ClOverlayEntities && !(m_Type == TYPE_BACKGROUND_FORCE))
							Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*(100-g_Config.m_ClOverlayEntities)/100.0f);
            RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, 0,
                            this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
          }
				}
				else if(pLayer->m_Type == LAYERTYPE_QUADS)
				{
					CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;

					CQuad *pQuads = (CQuad *)m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);
				}
			}
			else if(Render && g_Config.m_ClOverlayEntities && IsFrontLayer)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;

				CTile *pFrontTiles = (CTile *)m_pLayers->Map()->GetData(pTMap->m_Front);
				unsigned int Size = m_pLayers->Map()->GetDataSize(pTMap->m_Front);

				if(Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTile))
				{
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f*g_Config.m_ClOverlayEntities/100.0f);						
				}
			}
			else if(Render && g_Config.m_ClOverlayEntities && IsSwitchLayer)
			{

			}
			else if(Render && g_Config.m_ClOverlayEntities && IsTeleLayer)
			{

			}
			else if(Render && g_Config.m_ClOverlayEntities && IsSpeedupLayer)
			{

			}
			else if(Render && g_Config.m_ClOverlayEntities && IsTuneLayer)
			{
	
			}
		}
	}
}

// chillerbot-ng

void CMapLayers::RenderTilemap(CTile *pTiles, int w, int h, float Scale, vec4 Color, int RenderFlags, void *pUser, int ColorEnv, int ColorEnvOffset)
{
	if (!g_Config.m_ClChillerRender)
		return;
	if (!m_pClient->m_Snap.m_pLocalCharacter) // TODO: also render map if tee is dead and also add possibility to spectate
		return;

	char aFrame[32][64]; // tee aka center is at 8/16   y/x
	for (int i = 0; i < 32; i++)
	{
		str_copy(aFrame[i], "                               ", sizeof(aFrame[i]));
	}
	int rendered_tiles = 0;
	dbg_msg("render", "teeX: %.2f", static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f);
	Scale = 1.0; // chillerbot tile size is always one character
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	int render_dist = 16; // render tiles surroundign the tee
	ScreenX0 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f) - (render_dist);
	ScreenY0 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f) - (render_dist / 2);
	ScreenX1 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f) + (render_dist);
	ScreenY1 = (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f) + (render_dist / 2);
	dbg_msg("screen", "x0: %2.f y0: %.2f x1: %.2f y1: %.2f", ScreenX0, ScreenY0, ScreenX1, ScreenY1);

	// calculate the final pixelsize for the tiles
	float TilePixelSize = 1024/32.0f;
	float FinalTileSize = Scale/(ScreenX1-ScreenX0) * 16; // 16 = chillerbot screen width
	float FinalTilesetScale = FinalTileSize/TilePixelSize;

	float r=1, g=1, b=1, a=1;
	if(ColorEnv >= 0)
	{
		float aChannels[4];
		r = aChannels[0];
		g = aChannels[1];
		b = aChannels[2];
		a = aChannels[3];
	}

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	// adjust the texture shift according to mipmap level
	float TexSize = 1024.0f;
	float Frac = (1.25f/TexSize) * (1/FinalTilesetScale);
	float Nudge = (0.5f/TexSize) * (1/FinalTilesetScale);

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(!"idk what this is")
			{
			if(mx<0)
			mx = 0;
			if(mx>=w)
			mx = w-1;
			if(my<0)
			my = 0;
			if(my>=h)
			my = h-1;
			}
			else
			{
			if(mx<0)
			continue; // mx = 0;
			if(mx>=w)
			continue; // mx = w-1;
			if(my<0)
			continue; // my = 0;
			if(my>=h)
			continue; // my = h-1;
			}

			int c = mx + my*w;
			unsigned char Index = pTiles[c].m_Index;
			if(Index)
			{
				unsigned char Flags = pTiles[c].m_Flags;
				bool Render = true;

				if(Render)
				{
					int tx = Index%16;
					int ty = Index/16;
					int Px0 = tx*(1024/16);
					int Py0 = ty*(1024/16);
					int Px1 = Px0+(1024/16)-1;
					int Py1 = Py0+(1024/16)-1;

					float x0 = Nudge + Px0/TexSize+Frac;
					float y0 = Nudge + Py0/TexSize+Frac;
					float x1 = Nudge + Px1/TexSize-Frac;
					float y1 = Nudge + Py0/TexSize+Frac;
					float x2 = Nudge + Px1/TexSize-Frac;
					float y2 = Nudge + Py1/TexSize-Frac;
					float x3 = Nudge + Px0/TexSize+Frac;
					float y3 = Nudge + Py1/TexSize-Frac;

					if(Flags&TILEFLAG_VFLIP)
					{
						x0 = x2;
						x1 = x3;
						x2 = x3;
						x3 = x0;
					}

					if(Flags&TILEFLAG_HFLIP)
					{
						y0 = y3;
						y2 = y1;
						y3 = y1;
						y1 = y0;
					}

					if(Flags&TILEFLAG_ROTATE)
					{
						float Tmp = x0;
						x0 = x3;
						x3 = x2;
						x2 = x1;
						x1 = Tmp;
						Tmp = y0;
						y0 = y3;
						y3 = y2;
						y2 = y1;
						y1 = Tmp;
					}
					dbg_msg("map", "draw tile=%d at x: %.2f y: %.2f w: %.2f h: %.2f", Index, x*Scale, y*Scale, Scale, Scale);
					int renderX = (x*Scale) - (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f);
					int renderY = (y*Scale) - (static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f);
					dbg_msg("map", "absolut tile x: %d y: %d       tee x: %.2f y: %.2f", renderX, renderY, static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f, static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f);
					renderX += 32;
					renderY += 16;
					dbg_msg("map", "array pos  tile x: %d y: %d\n", renderX, renderY);
					if (renderX > 0 && renderX < 64 && renderY > 0 && renderY < 32)
					{
						aFrame[renderY][renderX] = '#';
						rendered_tiles++;
					}
					aFrame[16][32] = 'o'; // tee in center
					aFrame[15][32] = 'o'; // tee in center
					aFrame[17][32] = 'o'; // tee in center
				}
			}
			else
			{
				// dbg_msg("map", "skip at x: %.2f y: %.2f w: %.2f h: %.2f", x*Scale, y*Scale, Scale, Scale);
			}
			x += pTiles[c].m_Skip;
		}
	}
  if (rendered_tiles == 0)
    return;    

  m_NextRender--;
  if (m_NextRender == 0)
    return;
  m_NextRender = 60; // render every 60 what every this isy
  // system("clear");
  // render frame
  for (int y = 0; y < 32; y++)
  {
    printf("%s\n", aFrame[y]);
  }
  printf("------------- tiles: %d \n\n", rendered_tiles);
  printf("%s\n-------------\n", aFrame[15]);
}

// Spectrum.cpp: implementation of the CSpectrum class.
//
//////////////////////////////////////////////////////////////////////

#include "xmldocument.h"
#include "lib/g-force_core.h"
#include "../include/xbmc_vis_dll.h"

#include <xtl.h>
#include <stdio.h>

#pragma comment (lib, "lib/xbox_dx8.lib" )
#pragma comment (lib, "lib/libgforce.lib" )

extern "C" void d3dSetTextureStageState( int x, DWORD dwY, DWORD dwZ);
extern "C" void d3dSetRenderState(DWORD dwY, DWORD dwZ);

#define GOOM_MIN_WIDTH	16
//#define GOOM_MAX_WIDTH	640
#define GOOM_MIN_HEIGHT	16
//#define GOOM_MAX_HEIGHT	640

#define CONFIG_FILE "special://home/addons/visualization.g-force/config.xml"
#define RESOURCES_DIR "zip://special%3A%2F%2Fhome%2Faddons%2Fvisualization.g-force%2Fresources%2F"
#define ADDON_ROOT "q:\\home\\addons\\visualization.g-force\\"

struct VERTEX { D3DXVECTOR4 p; D3DCOLOR col; FLOAT tu, tv; };
static const DWORD FVF_VERTEX = D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1;
static VIS_INFO					  vInfo;
static LPDIRECT3DDEVICE8					m_pd3dDevice=NULL;
static LPDIRECT3DTEXTURE8					m_pTexture=NULL;				// textures
static LPDIRECT3DVERTEXBUFFER8		m_pVB=NULL;
static D3DCOLOR						m_colDiffuse;
static float						m_sData[2][512];
static unsigned int*	 		m_pFrameBuffer=NULL;
static int                m_iPosX;
static int                m_iPosY;
static int								m_iWidth;
static int								m_iHeight;
static int								m_iMaxWidth=720;
static int								m_iMaxHeight=480;
static char               m_szColorMaps[255];
static char               m_szDeltaFields[255];
static char               m_szParticles[255];
static char               m_szWaveShapes[255];
static bool								m_bGForceInit=false;
static char m_szVisName[128];
static GForceCore *						gForce=NULL;

void SetDefaults()
{
	//OutputDebugString("SetDefaults()\n");
	vInfo.iSyncDelay = 16;
	m_iWidth = 320;
	m_iHeight = 320;
  strcpy(m_szColorMaps, ADDON_ROOT);
  strcpy(m_szColorMaps,"G-Force ColorMaps");
  strcpy(m_szDeltaFields, ADDON_ROOT);
  strcpy(m_szDeltaFields,"G-Force DeltaFields");
  strcpy(m_szParticles, ADDON_ROOT);
  strcpy(m_szParticles,"G-Force Particles");
  strcpy(m_szWaveShapes, ADDON_ROOT);
  strcpy(m_szWaveShapes,"G-Force WaveShapes");
}

// Load settings from the Goom.xml configuration file
void LoadSettings()
{
	XmlNode node, childNode;
	CXmlDocument doc;


	// Set up the defaults
	SetDefaults();
	OutputDebugString("LoadSettings()\n");

  char szXMLFile[1024];
  strcpy(szXMLFile,CONFIG_FILE);

	// Load the config file
	if (doc.Load(szXMLFile)<0)
	{
		OutputDebugString("Failed to load g-force.xml()\n");
		return;
	}
	node = doc.GetNextNode(XML_ROOT_NODE);
	while (node>0)
	{
		if (strcmpi(doc.GetNodeTag(node),"visualisation"))
		{
			node = doc.GetNextNode(node);
			continue;
		}
		if (childNode = doc.GetChildNode(node,"syncdelay"))
		{
			vInfo.iSyncDelay = atoi(doc.GetNodeText(childNode));
			if (vInfo.iSyncDelay < 0)
				vInfo.iSyncDelay = 0;
		}
		if (childNode = doc.GetChildNode(node,"width"))
		{
			m_iWidth = atoi(doc.GetNodeText(childNode));
			if (m_iWidth < GOOM_MIN_WIDTH)
				m_iWidth = GOOM_MIN_WIDTH;
			if (m_iWidth > m_iMaxWidth)
				m_iWidth = m_iMaxWidth;
		}
		if (childNode = doc.GetChildNode(node,"height"))
		{
			m_iHeight = atoi(doc.GetNodeText(childNode));
			if (m_iHeight < GOOM_MIN_HEIGHT)
				m_iHeight = GOOM_MIN_HEIGHT;
			if (m_iHeight > m_iMaxHeight)
				m_iHeight = m_iMaxHeight;
		}
    if (childNode = doc.GetChildNode(node,"ColorMapsFolder"))
    {
      if (strstr(doc.GetNodeText(childNode),".zip")) // this is a zip
        sprintf(m_szColorMaps,"%s%s",RESOURCES_DIR,doc.GetNodeText(childNode));
      else
        sprintf(m_szColorMaps,"%sresources\\%s",ADDON_ROOT,doc.GetNodeText(childNode));
      printf("colormaps %s\n",m_szColorMaps);
    }
    if (childNode = doc.GetChildNode(node,"DeltaFieldsFolder"))
    {
      if (strstr(doc.GetNodeText(childNode),".zip")) // this is a zip
        sprintf(m_szDeltaFields,"%s%s",RESOURCES_DIR,doc.GetNodeText(childNode));
      else
        sprintf(m_szDeltaFields,"%sresources\\%s",ADDON_ROOT,doc.GetNodeText(childNode));
      printf("colormaps %s\n",m_szDeltaFields);
    }
    if (childNode = doc.GetChildNode(node,"ParticlesFolder"))
    {
      if (strstr(doc.GetNodeText(childNode),".zip")) // this is a zip
        sprintf(m_szParticles,"%s%s",RESOURCES_DIR,doc.GetNodeText(childNode));
      else
        sprintf(m_szParticles,"%sresources\\%s",ADDON_ROOT,doc.GetNodeText(childNode));
            printf("colormaps %s\n",m_szParticles);

    }
    if (childNode = doc.GetChildNode(node,"WaveShapesFolder"))
    {
      if (strstr(doc.GetNodeText(childNode),".zip")) // this is a zip
        sprintf(m_szWaveShapes,"%s%s",RESOURCES_DIR,doc.GetNodeText(childNode));
      else
        sprintf(m_szWaveShapes,"%sresources\\%s",ADDON_ROOT,doc.GetNodeText(childNode));
            printf("colormaps %s\n",m_szWaveShapes);

    }
		node = doc.GetNextNode(node);
	}
	doc.Close();
}


extern "C" ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!props)
    return ADDON_STATUS_UNKNOWN;

  VIS_PROPS* visprops = (VIS_PROPS*)props;

  OutputDebugString("Create()\n");
  strcpy(m_szVisName,visprops->name);
  m_iMaxWidth=visprops->width;
  m_iMaxHeight=visprops->height;
  m_iPosX=visprops->x;
  m_iPosY=visprops->y;
	m_colDiffuse	= 0xFFFFFFFF;
	m_pFrameBuffer=NULL;
	m_pTexture=NULL;
	m_pVB=NULL;

	m_pd3dDevice = (LPDIRECT3DDEVICE8)visprops->device;
	vInfo.bWantsFreq = false;

	// Load settings
	LoadSettings();

  return ADDON_STATUS_OK;
}

extern "C" void Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, const char* szSongName)
{
	OutputDebugString("Start\n");
	//LoadSettings();
	// Set up our texture and vertex buffers for rendering

	if (!m_pTexture)
	{
		if (D3D_OK != m_pd3dDevice->CreateTexture( m_iWidth, m_iHeight, 1, 0, D3DFMT_LIN_X8R8G8B8, 0, &m_pTexture ) )
		{
			OutputDebugString("Failed to create Texture\n");
			return;
		}
	}
	if (!m_pVB)
	{
		if (D3D_OK != m_pd3dDevice->CreateVertexBuffer( 4*sizeof(VERTEX), D3DUSAGE_WRITEONLY, 0L, D3DPOOL_DEFAULT, &m_pVB ))
		{
			OutputDebugString("Failed to create vertextbuffer\n");
			return ;
		}
	}
	VERTEX* v;
	m_pVB->Lock( 0, 0, (BYTE**)&v, 0L );

  FLOAT fPosX = m_iPosX;
  FLOAT fPosY = m_iPosY;
	FLOAT fWidth  = m_iMaxWidth;//screen width
	FLOAT fHeight = m_iMaxHeight;//screen height

	v[0].p = D3DXVECTOR4( fPosX - 0.5f,			fPosY - 0.5f,			0, 0 );
	v[0].tu = 0;
	v[0].tv = 0;
	v[0].col= m_colDiffuse;

	v[1].p = D3DXVECTOR4( fPosX + fWidth - 0.5f,	fPosY - 0.5f,			0, 0 );
	v[1].tu = (float)m_iWidth;
	v[1].tv = 0;
	v[1].col= m_colDiffuse;

	v[2].p = D3DXVECTOR4( fPosX + fWidth - 0.5f,	fPosY + fHeight - 0.5f,	0, 0 );
	v[2].tu = (float)m_iWidth;
	v[2].tv = (float)m_iHeight;
	v[2].col= m_colDiffuse;

	v[3].p = D3DXVECTOR4( fPosX - 0.5f,			fPosY + fHeight - 0.5f,	0, 0 );
	v[3].tu = 0;
	v[3].tv = (float)m_iHeight;
	v[3].col= m_colDiffuse;

	m_pVB->Unlock();
	// Initialize the G-Force engine
	// TEST FOR MEMORY LEAKS
	//char wszText[256];
	//MEMORYSTATUS stat;
	//GlobalMemoryStatus(&stat);
	//sprintf(wszText,"Free Memory %i kB ... \n",stat.dwAvailPhys);

	//OutputDebugString(wszText);

	if (!m_bGForceInit)
	{
		OutputDebugString("Init G-Force() ... ");

		//Lib init
		if(gForce==NULL) {
			gForce = new GForceCore(m_szColorMaps,m_szDeltaFields,m_szParticles,m_szWaveShapes);
			Rect rect;
			rect.left = 0;
			rect.top = 0;
      rect.right = (m_iWidth > m_iMaxWidth) ? m_iMaxWidth : m_iWidth;
      rect.bottom = (m_iHeight > m_iMaxHeight) ? m_iMaxHeight : m_iHeight;
      rect.right -= (rect.right % 8);
      rect.bottom -= (rect.bottom % 8);
			gForce->SetWinPort(&rect);
		}
		OutputDebugString(" Done\n");
		m_bGForceInit=true;
	}
}


//-- Audiodata ----------------------------------------------------------------
// Called by XBMC to pass new audio data to the vis
//-----------------------------------------------------------------------------
extern "C" void AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength)
{
	memset(m_sData,0,sizeof(m_sData));
	int ipos=0;
  while (ipos < 512)
  {
	  for (int i=0; i < iAudioDataLength; i+=2)
	  {
		  m_sData[0][ipos] = pAudioData[i];    // left channel
		  m_sData[1][ipos] = pAudioData[i+1];  // right channel
		  ipos++;
		  if (ipos >= 512) break;
	  }
  }
}

extern "C" void Render()
{
	if (!m_pTexture)
		return;
	if (!m_pVB)
		return;
	if (!m_bGForceInit)
		return;

//	OutputDebugString("Render\n");
//	OutputDebugString("Calling Goom_Update() ... ");
//>	m_pFrameBuffer=goom_update (m_sData, 0, -1,NULL, NULL);
	//OutputDebugString(" Done\n");
	if (true)
	{
		//OutputDebugString("Got FrameBuffer\n");	
		D3DLOCKED_RECT rectLocked;
		if ( D3D_OK == m_pTexture->LockRect(0,&rectLocked,NULL,0L  ) )
		{
			//OutputDebugString("Locked rect\n");
			BYTE *pBuff   = (BYTE*)rectLocked.pBits;	
			DWORD strideScreen=rectLocked.Pitch;
			if (pBuff)
			{
				gForce->setDrawParameter((unsigned long *)pBuff, strideScreen);
				gForce->renderSample( ::timeGetTime() , m_sData );
			}	
			m_pTexture->UnlockRect(0);
		}
	}	
	//OutputDebugString("settexture\n");
	// Set state to render the image
	m_pd3dDevice->SetTexture( 0, m_pTexture );

	d3dSetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	d3dSetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	d3dSetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	d3dSetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
	d3dSetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	d3dSetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	d3dSetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	d3dSetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	d3dSetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
	d3dSetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
	
	//OutputDebugString("setrenderstates\n");
	d3dSetRenderState( D3DRS_ZENABLE,      FALSE );
	d3dSetRenderState( D3DRS_FOGENABLE,    FALSE );
	d3dSetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
	d3dSetRenderState( D3DRS_FILLMODE,     D3DFILL_SOLID );
	d3dSetRenderState( D3DRS_CULLMODE,     D3DCULL_CCW );
	d3dSetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
	d3dSetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
	d3dSetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	m_pd3dDevice->SetVertexShader( FVF_VERTEX );

	//OutputDebugString("renderimage\n");
	// Render the image
	m_pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(VERTEX) );
	m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

	//OutputDebugString("done\n");
	return;
}


extern "C" void ADDON_Stop()
{
  OutputDebugString("Calling Goom_Close() ... ");
	if (m_pVB) m_pVB->Release();
	if (m_pTexture) m_pTexture->Release();
	m_pVB = NULL;
	m_pTexture = NULL;
	
	if (m_bGForceInit)
	{
		delete gForce;
		gForce = NULL;
	}
	m_pFrameBuffer = NULL;
	m_bGForceInit=false;
}



extern "C" void GetInfo(VIS_INFO* pInfo)
{
	pInfo->bWantsFreq =vInfo.bWantsFreq;
	pInfo->iSyncDelay=vInfo.iSyncDelay;
}

//-- OnAction -----------------------------------------------------------------
// Handle XBMC actions such as next preset, lock preset, album art changed etc
//-----------------------------------------------------------------------------
extern "C" bool OnAction(long flags, const void *param)
{
  return false;
}

//-- GetPresets ---------------------------------------------------------------
// Return a list of presets to XBMC for display
//-----------------------------------------------------------------------------
extern "C" unsigned int GetPresets(char ***presets)
{
  return 0;
}

//-- GetPreset ----------------------------------------------------------------
// Return the index of the current playing preset
//-----------------------------------------------------------------------------
extern "C" unsigned GetPreset()
{
  return 0;
}

//-- IsLocked -----------------------------------------------------------------
// Returns true if this add-on use settings
//-----------------------------------------------------------------------------
extern "C" bool IsLocked()
{
  return false;
}

//-- Destroy-------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Destroy()
{
  ADDON_Stop();
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" bool ADDON_HasSettings()
{
  return false;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
//-----------------------------------------------------------------------------

extern "C" unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
//-----------------------------------------------------------------------------
extern "C" void ADDON_FreeSettings()
{}

//-- UpdateSetting ------------------------------------------------------------
// Handle setting change request from XBMC
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_SetSetting(const char* id, const void* value)
{
  return ADDON_STATUS_UNKNOWN;
}

//-- GetSubModules ------------------------------------------------------------
// Return any sub modules supported by this vis
//-----------------------------------------------------------------------------
extern "C" unsigned int GetSubModules(char ***names)
{
  return 0; // this vis supports 0 sub modules
}

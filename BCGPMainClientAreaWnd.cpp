//*******************************************************************************
// COPYRIGHT NOTES
// ---------------
// This is a part of the BCGControlBar Library
// Copyright (C) 1998-2016 BCGSoft Ltd.
// All rights reserved.
//
// This source code can be used, distributed or modified
// only under terms and conditions 
// of the accompanying license agreement.
//*******************************************************************************

// BCGPMainClientAreaWnd.cpp : implementation file
//

#include "stdafx.h"
#include <math.h>
#include "BCGCBProVer.h"
#include "BCGPWorkspace.h"
#include "BCGPMainClientAreaWnd.h"
#include "BCGPMDIFrameWnd.h"
#include "BCGPMDIChildWnd.h"
#include "BCGPMenuBar.h"
#include "BCGPDockingControlBar.h"
#include "BCGPBaseTabbedBar.h"
#include "BCGPVisualManager.h"
#include "BCGPGlobalUtils.h"
#include "RegPath.h"
#include "BCGPRegistry.h"
#include "BCGPLocalResource.h"
#include "bcgprores.h"
#include "BCGPDrawManager.h"

extern CBCGPWorkspace* g_pWorkspace;
static HWND g_MDIChildDragWnd = NULL;
static HWND g_hwdTopMDIClientArea = NULL;

CString CBCGPMainClientAreaWnd::m_strRegSectionFmt = _T("%sMDIClientArea-%d");

#define REG_ENTRY_MDITABS_STATE			_T ("MDITabsState")
#define REG_ENTRY_MDI_FRAME_RECT		_T("MDIFrameRect")
#define REG_ENTRY_MDI_FRAME_FLAGS		_T("MDIFrameFlags")
#define REG_ENTRY_MDI_FRAME_SHOW_CMD	_T("MDIFrameShowCmd")

static const CString strMDIClientAreaProfile	= _T("MDIClientArea");

UINT BCGM_ON_MOVETOTABGROUP	= ::RegisterWindowMessage (_T("BCGM_ON_MOVETOTABGROUP"));

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CBCGPMainClientAreaWnd, CWnd)

#define UM_UPDATE_TABS		(WM_USER + 101)
#define RESIZE_MARGIN		40
#define NEW_GROUP_MARGIN	40

class CBCGPDragRectWnd : public CWnd
{
public:
	CBCGPDragRectWnd()
	{
	}
	
	BOOL Create(const CRect& rect, CWnd* pWndOwner)
	{
		CString strClassName = globalData.RegisterWindowClass (_T("BCGPTrackingWnd"));
		
		DWORD dwExStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
		if (globalData.IsWindowsLayerSupportAvailable())
		{
			dwExStyle |= WS_EX_LAYERED;
		}

		if (!CWnd::CreateEx(dwExStyle, strClassName,  _T (""), WS_POPUP | MFS_SYNCACTIVE,  rect, pWndOwner == NULL ? AfxGetMainWnd() : pWndOwner, 0))
		{
			return FALSE;
		}
		
		if (globalData.IsWindowsLayerSupportAvailable())
		{
			globalData.SetLayeredAttrib(GetSafeHwnd (), 0, 127, LWA_ALPHA);
		}

		if (pWndOwner != NULL)
		{
			SetOwner(pWndOwner);
		}
		
		ShowWindow(SW_SHOWNOACTIVATE);
		return TRUE;
	}

	// Generated message map functions
protected:
	//{{AFX_MSG(CBCGPDragRectWnd)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	virtual void PostNcDestroy();
};

BEGIN_MESSAGE_MAP(CBCGPDragRectWnd, CWnd)
//{{AFX_MSG_MAP(CBCGPDragRectWnd)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CBCGPDragRectWnd::OnEraseBkgnd(CDC* /*pDC*/) 
{
	return TRUE;
}
//*******************************************************************************
void CBCGPDragRectWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	CRect rect;
	GetClientRect(&rect);
	
	COLORREF colorFill = RGB(47, 103, 190);
	
    if (globalData.IsWindowsLayerSupportAvailable () && globalData.m_nBitsPerPixel > 8)
    {
        CBrush brFill (CBCGPDrawManager::PixelAlpha (colorFill, 105));
		dc.FillRect (rect, &brFill);
    }
    else
    {
		CBrush brFill (CBCGPDrawManager::PixelAlpha (RGB (
			255 - GetRValue (colorFill), 
			255 - GetGValue (colorFill), 
			255 - GetBValue (colorFill)), 
			50));
		
		CBrush* pBrushOld = dc.SelectObject (&brFill);
		dc.PatBlt (0, 0, rect.Width (), rect.Height (), PATINVERT);
		dc.SelectObject (pBrushOld);
    }
}
//*******************************************************************************
void CBCGPDragRectWnd::PostNcDestroy()
{
	CWnd::PostNcDestroy();
	delete this;
}

/////////////////////////////////////////////////////////////////////////////
// CBCGPMDIChildDragWnd window

class CBCGPMDIChildDragWnd : public CWnd
{
	// Construction
public:
	CBCGPMDIChildDragWnd(HWND hwnMainArea);
	
	// Attributes
public:
	CList<CBCGPMDIChildWnd*, CBCGPMDIChildWnd*>	m_lstMDIChild;
	HWND										m_hwnMainArea;
	HWND										m_hwndDropMDIFrame;
	CBitmap										m_bmpScreenShot;
	CBCGPTabWnd*								m_pWndTab;
	
	// Operations
public:
	void MoveToCursor();
	
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBCGPMDIChildDragWnd)
public:
	virtual BOOL Create(CList<CBCGPMDIChildWnd*, CBCGPMDIChildWnd*>* plstMDIChild, CBCGPTabWnd* pTabWnd);

protected:
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL
	
	// Implementation
public:
	virtual ~CBCGPMDIChildDragWnd();
	
	// Generated message map functions
protected:
	//{{AFX_MSG(CBCGPMDIChildDragWnd)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CBCGPMDIChildDragWnd::CBCGPMDIChildDragWnd(HWND hwnMainArea)
{
	m_hwnMainArea = hwnMainArea;
	m_hwndDropMDIFrame = NULL;
	m_pWndTab = NULL;
}

CBCGPMDIChildDragWnd::~CBCGPMDIChildDragWnd()
{
	if (m_hwndDropMDIFrame != NULL && ::IsWindow(m_hwndDropMDIFrame))
	{
		CBCGPMDIFrameWnd* pDropMDIFrame = DYNAMIC_DOWNCAST(CBCGPMDIFrameWnd, CWnd::FromHandlePermanent(m_hwndDropMDIFrame));
		if (pDropMDIFrame != NULL)
		{
			ASSERT_VALID(pDropMDIFrame);
			pDropMDIFrame->m_wndClientArea.DrawNewGroupRect(NULL);
		}
	}
}

BEGIN_MESSAGE_MAP(CBCGPMDIChildDragWnd, CWnd)
	//{{AFX_MSG_MAP(CBCGPMDIChildDragWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBCGPMDIChildDragWnd message handlers

BOOL CBCGPMDIChildDragWnd::Create(CList<CBCGPMDIChildWnd*, CBCGPMDIChildWnd*>* plstMDIChild, CBCGPTabWnd* pTabWnd)
{
	ASSERT_VALID(pTabWnd);

	m_lstMDIChild.AddHead(plstMDIChild);
	if (m_lstMDIChild.IsEmpty())
	{
		ASSERT(FALSE);
		return FALSE;
	}

	CBCGPMDIChildWnd* pActiveMDIChildFrame = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pTabWnd->GetActiveWnd());
	if (pActiveMDIChildFrame == NULL)
	{
		ASSERT(FALSE);
		return FALSE;
	}

	ASSERT_VALID(pActiveMDIChildFrame);

	if (g_pWorkspace != NULL)
	{
		g_pWorkspace->CreateScreenshot(m_bmpScreenShot, pActiveMDIChildFrame);
	}

	m_pWndTab = pTabWnd;

	CRect rect;
	pActiveMDIChildFrame->GetClientRect(rect);
	pActiveMDIChildFrame->ClientToScreen (&rect);

	rect.bottom += m_pWndTab->GetTabsHeight();

	CString strClassName = ::AfxRegisterWndClass (
		CS_SAVEBITS,
		::LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)(COLOR_BTNFACE + 1), NULL);

	BOOL bRes = CreateEx (WS_EX_TOOLWINDOW | WS_EX_TOPMOST, strClassName, NULL, WS_POPUP | WS_DISABLED, rect, NULL, 0);

	g_MDIChildDragWnd = GetSafeHwnd();
	return bRes;
}
//*******************************************************************************
void CBCGPMDIChildDragWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	CRect rectClient;
	GetClientRect (rectClient);

	dc.FillRect (rectClient, &globalData.brBarFace);

	int nTabsHeight = m_pWndTab->GetTabsHeight();
	int xOffset = 0;
	
	for (POSITION pos = m_lstMDIChild.GetHeadPosition(); pos != NULL;)
	{
		CBCGPMDIChildWnd* pWndMDIChild = m_lstMDIChild.GetNext(pos);
		ASSERT_VALID(pWndMDIChild);

		int nTab = m_pWndTab->GetTabFromHwnd(pWndMDIChild->GetSafeHwnd());

		if (nTab >= 0)
		{
			CRect rectTab;
			m_pWndTab->GetTabRect(nTab, rectTab);

			CPoint ptOffset = rectTab.TopLeft();
			ptOffset.x -= 5 + xOffset;

			if (m_pWndTab->GetLocation() == CBCGPBaseTabWnd::LOCATION_BOTTOM)
			{
				ptOffset.y -= rectClient.Height() - nTabsHeight - 2;
			}
			else
			{
				ptOffset.y -= 3;
			}

			CBCGPTabInfo* pTabInfo = (CBCGPTabInfo*) m_pWndTab->m_arTabs[nTab];
			ASSERT_VALID(pTabInfo);

			CRect rectCloseSaved = pTabInfo->m_rectClose;
			if (!pTabInfo->m_rectClose.IsRectEmpty())
			{
				pTabInfo->m_rectClose.OffsetRect(-ptOffset);
			}

			rectTab.OffsetRect(-ptOffset);

			CFont* pOldFont = NULL;
			
			if (m_pWndTab->m_bIsActiveTabBold && m_pWndTab->m_hFontCustom == NULL && !m_pWndTab->IsCaptionFont())
			{
				dc.SelectObject(&globalData.fontBold);
			}
			else
			{
				dc.SelectObject(m_pWndTab->GetTabFont());
			}

			dc.SetBkMode(TRANSPARENT);

			COLORREF	clrDark;
			COLORREF	clrBlack;
			COLORREF	clrHighlight;
			COLORREF	clrFace;
			COLORREF	clrDarkShadow;
			COLORREF	clrLight;
			CBrush*		pbrFace = NULL;
			CBrush*		pbrBlack = NULL;
			
			CBCGPVisualManager::GetInstance ()->GetTabFrameColors (
				m_pWndTab, clrDark, clrBlack, clrHighlight, clrFace, clrDarkShadow, clrLight,
				pbrFace, pbrBlack);

			CBCGPPenSelector ps(dc, clrDark);

			CBCGPVisualManager::GetInstance()->OnDrawTab(&dc, rectTab, nTab, TRUE, m_pWndTab);

			dc.SelectObject(pOldFont);

			pTabInfo->m_rectClose = rectCloseSaved;

			xOffset += rectTab.Width();
		}
	}

	if (m_bmpScreenShot.GetSafeHandle() != NULL)
	{
		CRect rectView = rectClient;

		if (m_pWndTab->GetLocation() == CBCGPBaseTabWnd::LOCATION_TOP)
		{
			rectView.top += nTabsHeight;
		}
		else
		{
			rectView.bottom -= nTabsHeight;
		}

		dc.DrawState(rectView.TopLeft(), rectView.Size(), m_bmpScreenShot, DSS_NORMAL);
	}

	dc.Draw3dRect(rectClient, globalData.clrBarShadow, globalData.clrBarShadow);
}
//*******************************************************************************
BOOL CBCGPMDIChildDragWnd::OnEraseBkgnd(CDC* /*pDC*/) 
{
	return TRUE;
}
//*******************************************************************************
void CBCGPMDIChildDragWnd::PostNcDestroy() 
{
	g_MDIChildDragWnd = NULL;

	CWnd::PostNcDestroy();
	delete this;
}
//*******************************************************************************
void CBCGPMDIChildDragWnd::MoveToCursor()
{
	CBCGPMDIChildWnd* pWndMDIChild = m_lstMDIChild.GetHead();
	ASSERT_VALID(pWndMDIChild);

	CRect rectWnd;
	GetWindowRect (&rectWnd);

	CPoint ptScreen;
	GetCursorPos(&ptScreen);

	CPoint ptOffset(rectWnd.Width() / 2, ::GetSystemMetrics(SM_CYCAPTION) / 2);

	if (m_pWndTab->GetSafeHwnd() != NULL)
	{
		int nTab = m_pWndTab->GetTabFromHwnd(pWndMDIChild->GetSafeHwnd());
		if (nTab >= 0)
		{
			CRect rectTab;
			m_pWndTab->GetTabRect(nTab, rectTab);

			ptOffset = CPoint(rectTab.Width() / 2, rectTab.Height() / 2);

			if (m_pWndTab->GetLocation() == CBCGPBaseTabWnd::LOCATION_BOTTOM)
			{
				ptOffset.y += rectWnd.Height() - rectTab.Height();
			}
		}
	}
	
	SetWindowPos(&wndTop,  ptScreen.x - ptOffset.x,  ptScreen.y - ptOffset.y,
		-1, -1, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);

	ShowWindow(SW_SHOWNOACTIVATE);
	m_hwndDropMDIFrame = NULL;

	const CList<CFrameWnd*, CFrameWnd*>& lstFrames = CBCGPFrameImpl::GetFrameList();

	for (POSITION pos = lstFrames.GetHeadPosition(); pos != NULL;)
	{
		CBCGPMDIFrameWnd* pMDIFrame = DYNAMIC_DOWNCAST(CBCGPMDIFrameWnd, lstFrames.GetNext(pos));

		if (pMDIFrame->GetSafeHwnd() != NULL && pMDIFrame->m_wndClientArea.GetSafeHwnd() != m_hwnMainArea)
		{
			CRect rectClientArea;
			pMDIFrame->m_wndClientArea.GetWindowRect(rectClientArea);

			if (rectClientArea.PtInRect(ptScreen))
			{
				if (m_hwndDropMDIFrame == NULL)
				{
					pMDIFrame->m_wndClientArea.OnDragForeignMDIChild(pWndMDIChild);
					m_hwndDropMDIFrame = pMDIFrame->GetSafeHwnd();
				}
			}
			else
			{
				pMDIFrame->m_wndClientArea.OnDragForeignMDIChild(NULL);
			}
		}
	}

	if (m_hwndDropMDIFrame == NULL)
	{
		::SetCursor (AfxGetApp ()->LoadStandardCursor (IDC_ARROW));
	}
}

/////////////////////////////////////////////////////////////////////////////
// CBCGPMDITabParams

CBCGPMDITabParams::CBCGPMDITabParams ()
{
	m_tabLocation			= CBCGPTabWnd::LOCATION_TOP;
	m_style					= CBCGPTabWnd::STYLE_3D_SCROLLED;
	m_closeButtonMode		= CBCGPTabWnd::TAB_CLOSE_BUTTON_NONE;
	m_bTabCloseButton		= TRUE;	
	m_bTabCustomTooltips	= FALSE;
	m_bTabIcons				= FALSE;
	m_bAutoColor			= FALSE;
	m_bDocumentMenu			= FALSE;
	m_bEnableTabSwap		= TRUE;
	m_nTabBorderSize		= CBCGPVisualManager::GetInstance ()->GetMDITabsBordersSize ();
	m_bFlatFrame			= TRUE;
	m_bActiveTabCloseButton	= FALSE;
	m_bReuseRemovedTabGroups= FALSE;
	m_bActiveTabBoldFont	= TRUE;
	m_bTabsCaptionFont		= FALSE;
	m_bTabMultipleSelection	= FALSE;
}

void CBCGPMDITabParams::Serialize (CArchive& ar)
{
	if (ar.IsStoring ())
	{
		ar << m_tabLocation;
		ar << m_style;
		ar << m_bTabCloseButton;
		ar << m_bTabIcons;
		ar << m_bAutoColor;
		ar << m_bDocumentMenu;
		ar << m_bEnableTabSwap;
		ar << m_nTabBorderSize;
	}
	else
	{
		int nValue;
		ar >> nValue; 
		m_tabLocation = (CBCGPTabWnd::Location) nValue;

		ar >> nValue;
		m_style = (CBCGPTabWnd::Style) nValue;

		ar >> m_bTabCloseButton;
		ar >> m_bTabIcons;
		ar >> m_bAutoColor;
		ar >> m_bDocumentMenu;
		ar >> m_bEnableTabSwap;
		ar >> m_nTabBorderSize;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CBCGPMainClientAreaWnd

CBCGPMainClientAreaWnd::CBCGPMainClientAreaWnd()
{
	m_bTabIsVisible				= FALSE;
	m_bTabIsEnabled				= FALSE;

	m_bIsMDITabbedGroup			= FALSE;
	m_groupAlignment			= GROUP_NO_ALIGN;
	m_nResizeMargin				= RESIZE_MARGIN;
	m_nNewGroupMargin			= NEW_GROUP_MARGIN;

	m_bDisableUpdateTabs		= FALSE;
	m_bInsideDragComplete		= FALSE;

	m_rectNewTabGroup.SetRectEmpty ();
	m_nTotalResizeRest			= 0;

	m_bActive					= FALSE;
	m_bLastActiveTab			= FALSE;

	m_bEnableTearOffMDIChildren	= FALSE;
	m_pWndMDIChildDrag			= NULL;
	m_bDetachMDIChildrenOnly	= FALSE;

	m_pDragRectWnd				= NULL;

	m_ScrollHorz.SendPosBeforeEndThumb(FALSE);
	m_ScrollVert.SendPosBeforeEndThumb(FALSE);
}
//*************************************************************************************
CBCGPMainClientAreaWnd::~CBCGPMainClientAreaWnd()
{
	if (m_pWndMDIChildDrag != NULL)
	{
		m_pWndMDIChildDrag->DestroyWindow ();
		m_pWndMDIChildDrag = NULL;
	}

	if (m_pDragRectWnd != NULL)
	{
		m_pDragRectWnd->DestroyWindow();
		m_pDragRectWnd = NULL;
	}

	while (!m_lstTabbedGroups.IsEmpty ())
	{
		delete m_lstTabbedGroups.RemoveTail ();
	}

	while (!m_lstRemovedTabbedGroups.IsEmpty ())
	{
		CBCGPTabWnd* pWnd= DYNAMIC_DOWNCAST(CBCGPTabWnd, m_lstRemovedTabbedGroups.RemoveTail ());
		if (pWnd != NULL)
		{
			delete pWnd;
		}
	}

	if (!m_mapTabIcons.IsEmpty())
	{
		for (POSITION pos = m_mapTabIcons.GetStartPosition(); pos != NULL;)
		{
			UINT_PTR hWnd = NULL;
			CImageList* pImageList = NULL;

			m_mapTabIcons.GetNextAssoc(pos, hWnd, pImageList);
			if (pImageList != NULL)
			{
				delete pImageList;
			}
		}

		m_mapTabIcons.RemoveAll();
	}
}

//*************************************************************************************
void CBCGPMainClientAreaWnd::EnableMDITabs (BOOL bEnable, 
											const CBCGPMDITabParams& params)
{
	if (m_bIsMDITabbedGroup)
	{
		EnableMDITabbedGroups (FALSE, params);
	}

	m_bTabIsEnabled = bEnable;
	m_bTabIsVisible = bEnable;
	
	m_mdiTabParams = params;
	ApplyParams (&m_wndTab);
	
	if (bEnable)
	{
		UpdateTabs ();
		if (!IsKeepClientEdge ())
		{
			ModifyStyleEx (WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);
		}
	}
	else
	{
		if (!IsKeepClientEdge ())
		{
			ModifyStyleEx (0, WS_EX_CLIENTEDGE, SWP_FRAMECHANGED);
		}
	}

	if (m_wndTab.GetSafeHwnd () != NULL)
	{
		m_wndTab.ShowWindow (SW_SHOW);
	}

	BringWindowToTop ();

	if (GetSafeHwnd () != NULL && GetParentFrame () != NULL)
	{
		GetParentFrame ()->RecalcLayout ();

		UINT uiRedrawFlags =	RDW_ALLCHILDREN | RDW_FRAME | RDW_INVALIDATE | 
								RDW_UPDATENOW | RDW_ERASE;

		if (m_wndTab.GetSafeHwnd () != NULL)
		{
			m_wndTab.RedrawWindow (NULL, NULL, uiRedrawFlags);
		}

		RedrawWindow (NULL, NULL, uiRedrawFlags);
	}
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::EnableMDITabbedGroups (BOOL bEnable, const CBCGPMDITabParams& mdiTabParams)
{
	if (m_bTabIsEnabled)
	{
		EnableMDITabs (FALSE, mdiTabParams);
	}

	m_wndTab.ShowWindow (SW_HIDE);

	HWND hwndActive = (HWND) SendMessage (WM_MDIGETACTIVE, 0, 0);

	if (m_bIsMDITabbedGroup != bEnable)
	{
		m_bIsMDITabbedGroup = bEnable;

		if (!bEnable)
		{
			for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != 0;)
			{
				CBCGPTabWnd* pNextWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
				ASSERT_VALID (pNextWnd);

				pNextWnd->ShowWindow (SW_HIDE);

				for (int i = 0; i < pNextWnd->GetTabsNum (); i++)
				{
					CWnd* pNextChildWnd = pNextWnd->GetTabWnd (i);
					ASSERT_VALID (pNextChildWnd);
					pNextChildWnd->ModifyStyle (0, CBCGPMDIChildWnd::m_dwExcludeStyle | WS_SYSMENU, SWP_NOZORDER | SWP_FRAMECHANGED);
				}
			}
		}
	}

	
	m_bTabIsVisible = bEnable;

	if (!m_bIsMDITabbedGroup)
	{
		if (!IsKeepClientEdge ())
		{
			ModifyStyleEx (0, WS_EX_CLIENTEDGE, SWP_FRAMECHANGED);
		}

		if (globalData.bIsWindowsVista)
		{
			CWnd* pWndChild = GetWindow (GW_CHILD);
			CList<CBCGPMDIChildWnd*, CBCGPMDIChildWnd*> lst;

			while (pWndChild != NULL)
			{
				ASSERT_VALID (pWndChild);

				CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pWndChild);
				if (pMDIChild != NULL && pMDIChild->CanShowOnMDITabs ())
				{
					lst.AddTail (pMDIChild);
				}

				pWndChild = pWndChild->GetNextWindow ();
			}
			
			m_bDisableUpdateTabs = TRUE;

			for (POSITION pos = lst.GetTailPosition (); pos != NULL;)
			{
				CBCGPMDIChildWnd* pMDIChild = lst.GetPrev (pos);
				pMDIChild->SetWindowPos (NULL, -1, -1, -1, -1,
								SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
			}

			m_bDisableUpdateTabs = FALSE;

			UpdateTabs ();
		}

		return;
	}

	m_mdiTabParams = mdiTabParams;

	if (!IsKeepClientEdge ())
	{
		ModifyStyleEx (WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);
	}

	POSITION pos = NULL;

	for (pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextWnd);
		pNextWnd->ShowWindow (SW_SHOWNA);
		ApplyParams (pNextWnd);
	}

	UpdateMDITabbedGroups (TRUE);	

	for (pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextWnd);
		pNextWnd->RecalcLayout ();
	}

	if (m_bIsMDITabbedGroup)
	{
		SetActiveTab (hwndActive);
	}
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::ApplyParams (CBCGPTabWnd* pTabWnd)
{
	ASSERT_VALID (pTabWnd);

	pTabWnd->ModifyTabStyle (m_mdiTabParams.m_style);
	pTabWnd->SetLocation (m_mdiTabParams.m_tabLocation);
	pTabWnd->m_bCloseBtn = m_mdiTabParams.m_bTabCloseButton;
	
	if (m_mdiTabParams.m_closeButtonMode != CBCGPTabWnd::TAB_CLOSE_BUTTON_NONE)
	{
		pTabWnd->m_TabCloseButtonMode = m_mdiTabParams.m_closeButtonMode;
	}
	else
	{
		pTabWnd->m_TabCloseButtonMode = m_mdiTabParams.m_bActiveTabCloseButton ? CBCGPTabWnd::TAB_CLOSE_BUTTON_ACTIVE : CBCGPTabWnd::TAB_CLOSE_BUTTON_NONE;
	}

	pTabWnd->EnableTabDocumentsMenu (m_mdiTabParams.m_bDocumentMenu);
	pTabWnd->EnableAutoColor (m_mdiTabParams.m_bAutoColor);
	pTabWnd->EnableTabSwap (m_mdiTabParams.m_bEnableTabSwap);
	pTabWnd->SetTabBorderSize (m_mdiTabParams.m_nTabBorderSize);
	pTabWnd->EnableCustomToolTips (m_mdiTabParams.m_bTabCustomTooltips);

	pTabWnd->HideInactiveWindow (FALSE);
	pTabWnd->HideNoTabs ();
	pTabWnd->AutoSizeWindow (FALSE);
	pTabWnd->AutoDestroyWindow (FALSE);
	pTabWnd->SetFlatFrame (m_mdiTabParams.m_bFlatFrame);
	pTabWnd->m_bTransparent = TRUE;
	pTabWnd->m_bTopEdge = TRUE;
	pTabWnd->SetDrawNoPrefix (TRUE, FALSE);
	pTabWnd->SetActiveTabBoldFont (m_mdiTabParams.m_bActiveTabBoldFont);
	pTabWnd->m_bActivateLastVisibleTab = TRUE;
	pTabWnd->m_bActivateTabOnRightClick = TRUE;
	pTabWnd->SetCaptionFont(m_mdiTabParams.m_bTabsCaptionFont);
	pTabWnd->EnableMultipleSelection(m_mdiTabParams.m_bTabMultipleSelection);

	pTabWnd->m_bIsMDITab = TRUE;
}
//*************************************************************************************
BEGIN_MESSAGE_MAP(CBCGPMainClientAreaWnd, CWnd)
	//{{AFX_MSG_MAP(CBCGPMainClientAreaWnd)
	ON_WM_ERASEBKGND()
	ON_WM_STYLECHANGING()
	ON_WM_NCPAINT()
	ON_WM_NCACTIVATE()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_MDISETMENU,OnSetMenu)
	ON_MESSAGE(WM_MDIREFRESHMENU, OnMDIRefreshMenu)
	ON_MESSAGE(WM_MDIDESTROY, OnMDIDestroy)
	ON_MESSAGE(WM_MDINEXT, OnMDINext)
	ON_MESSAGE(UM_UPDATE_TABS, OnUpdateTabs)
	ON_REGISTERED_MESSAGE(BCGM_GETDRAGBOUNDS, OnGetDragBounds)
	ON_REGISTERED_MESSAGE(BCGM_ON_DRAGCOMPLETE, OnDragComplete)
	ON_REGISTERED_MESSAGE(BCGM_ON_TABGROUPMOUSEMOVE, OnTabGroupMouseMove)
	ON_REGISTERED_MESSAGE(BCGM_ON_CANCELTABMOVE, OnCancelTabMove)
	ON_REGISTERED_MESSAGE(BCGM_ON_MOVETABCOMPLETE, OnMoveTabComplete)
	ON_REGISTERED_MESSAGE(BCGM_CHANGE_ACTIVE_TAB, OnActiveTabChanged)
	ON_MESSAGE(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CBCGPMainClientAreaWnd message handlers

LRESULT CBCGPMainClientAreaWnd::OnSetMenu (WPARAM wp, LPARAM lp)
{
	CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParentFrame ());
	if (pMainFrame != NULL && ::IsWindow (pMainFrame->GetSafeHwnd ()))
	{
		if (pMainFrame->OnSetMenu ((HMENU) wp))
		{
			wp = NULL;
		}
	}
	else
	{
		wp = NULL;
	}

	if (pMainFrame->m_bIsMDIChildDetached)
	{
		wp = NULL;
	}

	return DefWindowProc (WM_MDISETMENU, wp, lp);
}
//*********************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnMDIRefreshMenu (WPARAM /*wp*/, LPARAM /*lp*/)
{
	LRESULT lRes = Default ();

	CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParentFrame ());
	if (pMainFrame != NULL && pMainFrame->GetMenuBar () != NULL)
	{
		pMainFrame->m_hmenuWindow = 
			pMainFrame->GetWindowMenuPopup (pMainFrame->GetMenuBar ()->GetHMenu ());
	}

	return lRes;
}
//*********************************************************************************
BOOL CBCGPMainClientAreaWnd::OnEraseBkgnd(CDC* pDC) 
{
	if (m_bIsMDITabbedGroup)
	{
		HWND hWndMDIActive = (HWND)SendMessage(WM_MDIGETACTIVE);
		if (hWndMDIActive != NULL && ::IsWindowVisible(hWndMDIActive))
		{
			return TRUE;
		}
	}

	CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParentFrame ());
	if (pMainFrame != NULL && pMainFrame->OnEraseMDIClientBackground (pDC))
	{
		return TRUE;
	}

	CRect rectClient;
	GetClientRect (rectClient);

	if (CBCGPVisualManager::GetInstance ()->OnEraseMDIClientArea (pDC, rectClient))
	{
		return TRUE;
	}

	return CWnd::OnEraseBkgnd(pDC);
}
//***********************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnMDIDestroy(WPARAM wParam, LPARAM)
{
	LRESULT lRes = 0;
	CBCGPMDIFrameWnd* pParentFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParentFrame ());

	BOOL bCloseParent = FALSE;

	if (pParentFrame->m_bIsMDIChildDetached && !pParentFrame->m_bClosing)
	{
		int nCount = 0;

		CWnd* pWndChild = GetWindow (GW_CHILD);
		while (pWndChild != NULL && nCount < 2)
		{
			ASSERT_VALID (pWndChild);
			
			if (DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pWndChild) != NULL)
			{
				nCount++;
			}

			pWndChild = pWndChild->GetNextWindow ();
		}

		if (nCount == 1)
		{
			bCloseParent = TRUE;
		}
	}
	
	CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, CWnd::FromHandle ((HWND)wParam));
	BOOL bTabHeightChanged = FALSE;

	if (!pParentFrame->m_bClosing && !CBCGPMDIFrameWnd::m_bDisableSetRedraw)
	{
		SetRedraw (FALSE);
	}

	HWND hwndActive = NULL;
	if (pMDIChild != NULL)
	{
		CBCGPTabWnd* pTabWnd = pMDIChild->GetRelatedTabGroup ();
		pMDIChild->SetRelatedTabGroup (NULL);
		if (pTabWnd != NULL)
		{
			int nTabsHeight = pTabWnd->GetTabsHeight ();

			int iTab = pTabWnd->GetTabFromHwnd ((HWND)wParam);
			if (iTab >= 0)
			{
				pMDIChild->m_bToBeDestroyed = TRUE;
			}
			pTabWnd->RemoveTab (iTab);
			
			if (pTabWnd->GetTabsNum () == 0)
			{
				POSITION pos = m_lstTabbedGroups.Find (pTabWnd);
				
				if (pos != NULL)
				{
					// find window to activate next group after the current group has been destroyed
					// we should find the window to activate only if the active group is being destroyed
					if (m_lstTabbedGroups.GetCount () > 1 && 
						pTabWnd->IsActiveInMDITabGroup ())
					{
						m_lstTabbedGroups.GetNext (pos);
						if (pos == NULL)
						{
							pos = m_lstTabbedGroups.GetHeadPosition ();
						}

						if (pos != NULL)
						{
							CBCGPTabWnd* pNextTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetAt (pos));
							if (pNextTabWnd != NULL)
							{
								int iActiveTab = pNextTabWnd->GetActiveTab ();
								if (iActiveTab == -1)
								{
									iActiveTab = 0;
								}
								CWnd* pActiveWnd = pNextTabWnd->GetTabWnd (iActiveTab);
								if (pActiveWnd != NULL)
								{
									ASSERT_VALID (pActiveWnd);
									hwndActive = pActiveWnd->GetSafeHwnd ();
								}
							}
						}
					}
					RemoveTabGroup (pTabWnd);
				}
			}
			else
			{
				bTabHeightChanged = (nTabsHeight != pTabWnd->GetTabsHeight ());
			}
		}
	}

	
	if (m_wndTab.GetSafeHwnd () != NULL)
	{
		int nTabsHeight = m_wndTab.GetTabsHeight ();
		int iTab = m_wndTab.GetTabFromHwnd ((HWND)wParam);
		if (iTab >= 0)
		{
			CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, m_wndTab.GetTabWnd (iTab));
			if (pMDIChild != NULL)
			{
				pMDIChild->m_bToBeDestroyed = TRUE;
			}

			m_wndTab.RemoveTab (iTab);
		}
		bTabHeightChanged = (nTabsHeight != m_wndTab.GetTabsHeight ());
	}
	

	lRes = Default ();
	if (bTabHeightChanged && pParentFrame != NULL)
	{
		pParentFrame->RecalcLayout ();
	}

	if (!pParentFrame->m_bClosing)
	{
		if (IsWindow (hwndActive))
		{
			SetActiveTab (hwndActive);
		}
	}
	if (!pParentFrame->m_bClosing && !CBCGPMDIFrameWnd::m_bDisableSetRedraw)
	{
		SetRedraw (TRUE);
		GetParent ()->RedrawWindow (NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
	}

	if (bCloseParent)
	{
		pParentFrame->PostMessage(WM_CLOSE);
	}

	return lRes;
}
//***********************************************************************************
void CBCGPMainClientAreaWnd::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType) 
{
	if (m_bDisableUpdateTabs)
	{
		return;
	}
	if (m_bIsMDITabbedGroup)
	{
		CalcWindowRectForMDITabbedGroups (lpClientRect, nAdjustType);
		CWnd::CalcWindowRect(lpClientRect, nAdjustType);
		return;
	}

	if (m_wndTab.GetSafeHwnd () != NULL)
	{
		BOOL bRedraw = FALSE;

		if (m_bTabIsVisible)
		{
			CRect rectOld;
			m_wndTab.GetWindowRect (rectOld);

			m_wndTab.SetWindowPos (NULL, 
				lpClientRect->left, lpClientRect->top,
				lpClientRect->right - lpClientRect->left,
				lpClientRect->bottom - lpClientRect->top,
				SWP_NOZORDER | SWP_NOACTIVATE);

			CRect rectTabClient;
			m_wndTab.GetClientRect (rectTabClient);

			CRect rectTabWnd;
			m_wndTab.GetWndArea (rectTabWnd);

			lpClientRect->top += (rectTabWnd.top - rectTabClient.top);
			lpClientRect->bottom += (rectTabWnd.bottom - rectTabClient.bottom);
			lpClientRect->left += (rectTabWnd.left - rectTabClient.left);
			lpClientRect->right += (rectTabWnd.right - rectTabClient.right);

			m_wndTab.ShowWindow (SW_SHOWNA);

			CRect rectNew;
			m_wndTab.GetWindowRect (rectNew);

			bRedraw = (rectOld != rectNew);
		}
		else
		{
			m_wndTab.ShowWindow (SW_HIDE);
		}

		CRect rectOld;
		GetWindowRect (rectOld);
		int nHeightDelta = lpClientRect->bottom - lpClientRect->top - 
							rectOld.Height ();

		SetWindowPos (NULL, 
				lpClientRect->left, lpClientRect->top,
				lpClientRect->right - lpClientRect->left,
				lpClientRect->bottom - lpClientRect->top,
				SWP_NOZORDER | SWP_NOACTIVATE);

		CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParentFrame ());
		if (pMainFrame != NULL)
		{
			pMainFrame->OnSizeMDIClient (rectOld, lpClientRect);
		}

		if (!m_bTabIsVisible)
		{
			CRect rectClient;
			GetClientRect (&rectClient);
			CBCGPMDIFrameWnd* pFrame = (CBCGPMDIFrameWnd*) GetParentFrame ();
			ASSERT_VALID (pFrame);
			HWND hwndT = ::GetWindow(pFrame->m_hWndMDIClient, GW_CHILD);
			
			while (hwndT != NULL)
			{
				DWORD dwStyle = ::GetWindowLong (hwndT, GWL_STYLE);
				if (dwStyle & WS_MAXIMIZE)
				{
					break; // nothing to move;
				}
				if (dwStyle & WS_MINIMIZE)
				{
					CRect rectWnd;
					::GetWindowRect (hwndT, rectWnd);
					ScreenToClient (&rectWnd);

					rectWnd.OffsetRect (0, nHeightDelta);

					if (rectWnd.top < rectClient.top)
					{
						rectWnd.top = rectClient.top;
					}

					::SetWindowPos (hwndT, NULL, 
									rectWnd.left, rectWnd.top, 0, 0, 
									SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
				}

				hwndT=::GetWindow(hwndT,GW_HWNDNEXT);
			}
		}
	}

	CWnd::CalcWindowRect(lpClientRect, nAdjustType);

	int nActiveTab = m_wndTab.GetActiveTab ();
	for (int i = 0; i < m_wndTab.GetTabsNum (); i++)
	{
		CWnd* pWnd = m_wndTab.GetTabWnd (i);
		if (pWnd->GetSafeHwnd () == 0)
		{
			continue;
		}

		// only applies to MDI children in "always maximize" mode
		if ((pWnd->GetStyle () & WS_MINIMIZE) != 0 && 
			((pWnd->GetStyle () & WS_SYSMENU) == 0))
		{
			pWnd->ShowWindow (SW_RESTORE);
		}

		DWORD dwFlags = SWP_NOACTIVATE;
		if (i != nActiveTab)
		{
			dwFlags |= SWP_NOZORDER | SWP_NOREDRAW;
		}

		CRect rect (0, 0, lpClientRect->right - lpClientRect->left, 
							lpClientRect->bottom - lpClientRect->top);
		
	
		CRect rectClient;
		pWnd->GetClientRect (rectClient);
		pWnd->ClientToScreen (rectClient);

		CRect rectScreen;
		pWnd->GetWindowRect (rectScreen);

		rect.left -= rectClient.left - rectScreen.left;
		rect.top -= rectClient.top - rectScreen.top;
		rect.right += rectScreen.right - rectClient.right;
		rect.bottom += rectScreen.bottom - rectClient.bottom;

		if (rectClient == rect)
		{
			break;
		}
		
		if (pWnd != NULL && ((pWnd->GetStyle () & WS_SYSMENU) == 0))
		{
			pWnd->SetWindowPos (&wndTop, rect.left, rect.top, 
								rect.Width (), rect.Height (), dwFlags);
		}
	}

}
//***********************************************************************************
void CBCGPMainClientAreaWnd::CalcWindowRectForMDITabbedGroups(LPRECT lpClientRect, UINT /*nAdjustType*/) 
{
	if (m_bInsideDragComplete)
	{
		return;
	}

	SetWindowPos (&wndBottom, 
				lpClientRect->left, lpClientRect->top,
				lpClientRect->right - lpClientRect->left,
				lpClientRect->bottom - lpClientRect->top,
				SWP_NOACTIVATE); 

	if (m_lstTabbedGroups.IsEmpty ())
	{
		return;
	}
	// special processing for single tab
	
	if (m_lstTabbedGroups.GetCount () == 1)
	{
		CBCGPTabWnd* pNextTab = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetHead ());
		ASSERT_VALID (pNextTab);
		pNextTab->ShowWindow (SW_SHOWNA);
		pNextTab->SetWindowPos (NULL, 0, 0, 
								lpClientRect->right - lpClientRect->left, 
								lpClientRect->bottom - lpClientRect->top, 
								SWP_NOZORDER | SWP_NOACTIVATE);

		AdjustMDIChildren (pNextTab);		
		return;
	}


	ASSERT (m_groupAlignment != GROUP_NO_ALIGN);

	int nTotalSize = 0;
	POSITION pos = NULL;

	for (pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextTab = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextTab);

		CRect rect;
		pNextTab->GetWindowRect (rect);

		nTotalSize += (m_groupAlignment == GROUP_VERT_ALIGN) ? rect.Width () : rect.Height (); 
	}

	int nClientAreaWndSize = (m_groupAlignment == GROUP_VERT_ALIGN) ? 
								lpClientRect->right - lpClientRect->left : 
								lpClientRect->bottom - lpClientRect->top;
	int nOffset = 0;
	int nMinAllowedSize = m_nResizeMargin;

	for (pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextTab = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextTab);
		
		if (pNextTab->GetSafeHwnd () != NULL)
		{
			CRect rect;
			pNextTab->GetWindowRect (rect);

			double dblCurrSize = m_groupAlignment == GROUP_VERT_ALIGN ? rect.Width () : rect.Height();
			double dblSizeRatio = ceil(dblCurrSize / (double)nTotalSize * 100.0);
			int nNewSize = (int)(dblSizeRatio * (double)nClientAreaWndSize / 100.0);

			if (pos == NULL && nClientAreaWndSize - nOffset + nNewSize != 0)
			{
				nNewSize = nClientAreaWndSize - nOffset;
			}

			if (nNewSize < nMinAllowedSize)
			{
				nNewSize = nMinAllowedSize;
			}
			
			if (m_groupAlignment == GROUP_VERT_ALIGN)
			{
				pNextTab->SetWindowPos (NULL, nOffset, 
					0,
					nNewSize,
					lpClientRect->bottom - lpClientRect->top, 
					SWP_NOZORDER | SWP_NOACTIVATE);
			}
			else
			{
				pNextTab->SetWindowPos (NULL, 0, 
					nOffset, 
					lpClientRect->right - lpClientRect->left,
					nNewSize, 
					SWP_NOZORDER | SWP_NOACTIVATE);
				
			}

			nOffset += nNewSize;
			AdjustMDIChildren (pNextTab);
			pNextTab->RedrawWindow();
		}
	}
}
//***********************************************************************************
void CBCGPMainClientAreaWnd::AdjustMDIChildren (CBCGPTabWnd* pTabWnd)
{
	if (!pTabWnd->IsWindowVisible () && CBCGPMDIFrameWnd::m_bDisableSetRedraw)
	{
		return;
	}

	CRect rectTabWnd;
	pTabWnd->GetWndArea (rectTabWnd);
	pTabWnd->MapWindowPoints (this, rectTabWnd);

	int nActiveTab = pTabWnd->GetActiveTab ();
	
	for (int i = 0; i < pTabWnd->GetTabsNum (); i++)
	{
		CWnd* pWnd = pTabWnd->GetTabWnd (i);
		if (pWnd->GetSafeHwnd () == 0)
		{
			continue;
		}

		DWORD dwStyle = ::GetWindowLong (pWnd->GetSafeHwnd (), GWL_STYLE);
		if ((dwStyle & WS_MINIMIZE) != 0)
		{
			pWnd->ShowWindow (SW_RESTORE);
		}

		DWORD dwFlags = SWP_NOACTIVATE;
		if (i != nActiveTab)
		{
			dwFlags |= SWP_NOZORDER | SWP_NOREDRAW;
		}
		
		if (pWnd != NULL)
		{
			pWnd->SetWindowPos (&wndTop, rectTabWnd.left, rectTabWnd.top, 
								rectTabWnd.Width (), rectTabWnd.Height (), dwFlags);

			CBCGPMDIChildWnd* pChildWnd = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pWnd);
			if (pChildWnd != NULL && pChildWnd->IsTaskbarTabsSupportEnabled() && pChildWnd->IsRegisteredWithTaskbarTabs())
			{
				pChildWnd->InvalidateIconicBitmaps();
				pChildWnd->SetTaskbarTabOrder(NULL);
			}
		}
	}
}
//***********************************************************************************
void CBCGPMainClientAreaWnd::SetTaskbarTabOrder()
{
	for (POSITION pos = m_lstTabbedGroups.GetHeadPosition(); pos != NULL;)
	{
		CBCGPTabWnd* pNextTab = DYNAMIC_DOWNCAST(CBCGPTabWnd, m_lstTabbedGroups.GetNext(pos));
		ASSERT_VALID(pNextTab);
		
		for (int i = 0; i < pNextTab->GetTabsNum(); i++)
		{
			CBCGPMDIChildWnd* pChildWnd = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pNextTab->GetTabWnd(i));
			ASSERT_VALID(pChildWnd);

			if (pChildWnd->GetSafeHwnd() == NULL)
			{
				continue;
			}
			if (pChildWnd->IsTaskbarTabsSupportEnabled() && pChildWnd->IsRegisteredWithTaskbarTabs())
			{
				pChildWnd->SetTaskbarTabOrder(NULL);
			}
		}
	}
}
//***********************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnActiveTabChanged(WPARAM wp, LPARAM lp)
{
	if (!globalData.bIsWindows7 || !m_bIsMDITabbedGroup)
	{
		return 0;
	}

	CBCGPTabWnd* pTabWnd = (CBCGPTabWnd*)lp;
	if (pTabWnd == NULL)
	{
		return 0;
	}

	ASSERT_VALID(pTabWnd);
	int iTab = (int) wp;
	CBCGPMDIChildWnd* pActiveMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pTabWnd->GetTabWnd(iTab));
	if (pActiveMDIChild == NULL)
	{
		return 0;
	}

	ASSERT_VALID(pActiveMDIChild);
	if (!pActiveMDIChild->IsRegisteredWithTaskbarTabs())
	{
		return 0;
	}

	CBCGPMDIChildWnd* pNextMDIChild = FindNextRegisteredWithTaskbarMDIChild(pActiveMDIChild);
	// if pNextMDIChild is NULL we insert at the end, because GetSafeHwnd() will return NULL
	pActiveMDIChild->SetTaskbarTabOrder(pNextMDIChild);

	return 1;
}
//***********************************************************************************
CBCGPMDIChildWnd* CBCGPMainClientAreaWnd::FindNextRegisteredWithTaskbarMDIChild(CBCGPMDIChildWnd* pOrgWnd)
{
	ASSERT_VALID(pOrgWnd);

	int iIndex = -1;
	CBCGPTabWnd* pTabCtrl = FindTabWndByChild(pOrgWnd->GetSafeHwnd(), iIndex);

	if (pTabCtrl == NULL)
		return NULL;

	ASSERT_VALID(pTabCtrl);

	// find next registered child within this tabbed group
	CBCGPMDIChildWnd* pNextMDIChild = FindNextRegisteredWithTaskbarMDIChild(pTabCtrl, iIndex + 1);
	if (pNextMDIChild == NULL)
	{
		// maybe in next tabbed group
		while ((pTabCtrl = GetNextTabWnd(pTabCtrl, TRUE)) != NULL && pNextMDIChild == NULL)
		{
			ASSERT_VALID(pTabCtrl);
			pNextMDIChild = FindNextRegisteredWithTaskbarMDIChild(pTabCtrl, 0);
		}
	}

	return pNextMDIChild;
}
//***********************************************************************************
CBCGPMDIChildWnd* CBCGPMainClientAreaWnd::FindNextRegisteredWithTaskbarMDIChild(CBCGPTabWnd* pTabCtrl, int iStartFrom)
{
	ASSERT_VALID(pTabCtrl);

	if (pTabCtrl == NULL)
	{
		return NULL;
	}

	if (iStartFrom < 0)
		iStartFrom = 0;

	for (int i = iStartFrom; i < pTabCtrl->GetTabsNum(); i++)
	{
		CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pTabCtrl->GetTabWnd(i));
		if (pMDIChild == NULL || pMDIChild->GetSafeHwnd() == NULL)
		{
			continue;
		}

		ASSERT_VALID(pMDIChild);
		if (pMDIChild->IsRegisteredWithTaskbarTabs())
		{
			return pMDIChild;
		}
	}

	return NULL;
}
//***********************************************************************************
void CBCGPMainClientAreaWnd::SetActiveTab (HWND hwnd)
{
	if (m_bDisableUpdateTabs)
	{
		return;
	}
	if (m_bIsMDITabbedGroup)
	{
		CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST (CBCGPMDIChildWnd, CWnd::FromHandle (hwnd));
		if (pMDIChild != NULL)
		{
			ASSERT_VALID (pMDIChild);
			CBCGPTabWnd* pTabWnd = pMDIChild->GetRelatedTabGroup ();
			if (pTabWnd != NULL)
			{
				ASSERT_VALID (pTabWnd);

				int iTab = pTabWnd->GetTabFromHwnd (hwnd);
				if (iTab >= 0)
				{

					CRect rectTabWnd; 
					pTabWnd->GetClientRect (rectTabWnd);

					if (rectTabWnd.IsRectEmpty ())
					{
						CFrameWnd* pMainFrame = pMDIChild->GetTopLevelFrame ();

						if (pMainFrame != NULL)
						{
							ASSERT_VALID (pMainFrame);
							pMainFrame->RecalcLayout ();
						}
					}

					CBCGPTabWnd* pPrevActiveWnd = FindActiveTabWnd ();
					if (pPrevActiveWnd != NULL)
					{
						BOOL bRedraw = pPrevActiveWnd->ClearSelectedTabs();
						pPrevActiveWnd->SetActiveInMDITabGroup (FALSE);

						if (bRedraw)
						{
							RedrawWindow (NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
						}
						else
						{
							pPrevActiveWnd->InvalidateTab (pPrevActiveWnd->GetActiveTab ());
						}
					}

					pTabWnd->SetActiveInMDITabGroup (TRUE);
					pTabWnd->SetActiveTab (iTab);
					pTabWnd->RedrawWindow();
				}
			}
		}
	}
	else
	{
		if (m_bTabIsVisible)
		{
			int iTab = m_wndTab.GetTabFromHwnd (hwnd);
			if (iTab >= 0)
			{
				m_wndTab.SetActiveTab (iTab);
			}
		}
	}
}
//************************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnUpdateTabs (WPARAM, LPARAM)
{
	UpdateTabs ();
	return 0;
}
//**************************************************************************************
void CBCGPMainClientAreaWnd::PreSubclassWindow() 
{
	CWnd::PreSubclassWindow();
	CreateTabGroup (&m_wndTab);
}
//*************************************************************************************
CBCGPTabWnd* CBCGPMainClientAreaWnd::CreateTabGroup (CBCGPTabWnd* pWndTab)
{
	BOOL bResused = FALSE;

	if (pWndTab == NULL)
	{
		if (m_mdiTabParams.m_bReuseRemovedTabGroups && m_lstRemovedTabbedGroups.GetCount () > 0 && m_bIsMDITabbedGroup)
		{
			pWndTab = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstRemovedTabbedGroups.RemoveTail ());

			if (pWndTab != NULL && ::IsWindow (pWndTab->GetSafeHwnd ()))
			{
				ASSERT_VALID (pWndTab);
				bResused = TRUE;
			}
		}
		
		if (!bResused)
		{
			CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST(CBCGPMDIFrameWnd, GetParentFrame());
			if (pMainFrame != NULL)
			{
				pWndTab = pMainFrame->CreateMDITabGroup();
			}
			else
			{
				pWndTab = new CBCGPTabWnd;
			}

			ASSERT_VALID (pWndTab);
			pWndTab->SetMDIFocused(m_bActive);
		}
	}

	if (m_mdiTabParams.m_bTabCustomTooltips)
	{
		pWndTab->EnableCustomToolTips ();
	}

	CWnd* pParent = m_bIsMDITabbedGroup ? this : (CWnd*) GetParentFrame ();

	//-------------------------
	// Create MDI tabs control:
	//-------------------------
	if (!bResused)
	{
		if (!pWndTab->Create (m_mdiTabParams.m_style, CRect (0, 0, 0, 0), 
			pParent, (UINT)-1, m_mdiTabParams.m_tabLocation, m_mdiTabParams.m_bTabCloseButton))
		{
			TRACE(_T("CBCGPMainClientAreaWnd::OnCreate: can't create tabs window\n"));
			delete pWndTab;
			return NULL;
		}

		if (m_bLastActiveTab)
		{
			pWndTab->EnableActivateLastActive();
		}
	}
	else
	{
		if (pWndTab->GetParent () != pParent)
		{
			pWndTab->SetParent (pParent);
		}

		pWndTab->ModifyTabStyle (m_mdiTabParams.m_style);
		pWndTab->SetLocation (m_mdiTabParams.m_tabLocation);
		pWndTab->EnableActiveTabCloseButton (m_mdiTabParams.m_bTabCloseButton);
	}

	ApplyParams (pWndTab);

	if (!m_bTabIsVisible)
	{
		pWndTab->ShowWindow (SW_HIDE);
	}

	//------------------
	// Create tab icons:
	//------------------
	
	if (!m_bIsMDITabbedGroup)
	{
		m_TabIcons.Create (
			globalData.m_sizeSmallIcon.cx, globalData.m_sizeSmallIcon.cy, 
			ILC_COLOR32 | ILC_MASK, 0, 1);
	}
	else 
	{
		CImageList* pImageList = NULL;
		if (m_mapTabIcons.Lookup ((UINT_PTR)pWndTab->GetSafeHwnd(), pImageList) && 
			pImageList != NULL)
		{
			pImageList->DeleteImageList ();
		}
		else
		{
			pImageList = new CImageList;
			m_mapTabIcons.SetAt ((UINT_PTR)pWndTab->GetSafeHwnd(), pImageList);
		}

		pImageList->Create (
			globalData.m_sizeSmallIcon.cx, globalData.m_sizeSmallIcon.cy, 
			ILC_COLOR32 | ILC_MASK, 0, 1);
	}

	return pWndTab;
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::UpdateTabs (BOOL bSetActiveTabVisible/* = FALSE*/)
{
	if (m_bDisableUpdateTabs)
	{
		return;
	}

	if (m_bIsMDITabbedGroup)
	{
		UpdateMDITabbedGroups (bSetActiveTabVisible);
		return;
	}

	if (m_wndTab.GetSafeHwnd () == NULL || !m_bTabIsVisible)
	{
		return;
	}

	BOOL bRecalcLayout = FALSE;
	BOOL bTabWndEmpty = m_wndTab.GetTabsNum () == 0;

	CWnd* pWndChild = GetWindow (GW_CHILD);
	while (pWndChild != NULL)
	{
		ASSERT_VALID (pWndChild);

		CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pWndChild);

		BOOL bIsShowTab = TRUE;
		if (pMDIChild != NULL)
		{
			bIsShowTab = pMDIChild->CanShowOnMDITabs ();
		}
		else if (pWndChild->IsKindOf (RUNTIME_CLASS (CBCGPTabWnd)))
		{
			pWndChild = pWndChild->GetNextWindow ();
			continue;
		}


		//--------------
		// Get tab icon:
		//--------------
		int iIcon = -1;
		if (m_mdiTabParams.m_bTabIcons)
		{
			HICON hIcon = NULL;
			if (pMDIChild != NULL)
			{
				hIcon = pMDIChild->GetFrameIcon ();
			}
			else
			{
				if ((hIcon = pWndChild->GetIcon (FALSE)) == NULL)
				{
					hIcon = (HICON)(LONG_PTR) GetClassLongPtr (*pWndChild, GCLP_HICONSM);
				}
			}

			if (hIcon != NULL)
			{
				if (!m_mapIcons.Lookup ((UINT_PTR)hIcon, iIcon))
				{
					iIcon = m_TabIcons.Add (hIcon);
					m_mapIcons.SetAt ((UINT_PTR)hIcon, iIcon);

					if (m_TabIcons.GetImageCount () == 1)
					{
						m_wndTab.SetImageList (m_TabIcons.GetSafeHandle ());
					}
				}

				if (pMDIChild != NULL)
				{
					pMDIChild->UpdateTaskbarTabIcon(hIcon);
				}
			}
		}
		else
		{
			m_wndTab.ClearImageList ();
			m_mapIcons.RemoveAll ();

			while (m_TabIcons.GetImageCount () > 0)
			{
				m_TabIcons.Remove (0);
			}

			// set tab icons for taskbar tabs
			if (pMDIChild != NULL && pMDIChild->IsTaskbarTabsSupportEnabled() && pMDIChild->IsRegisteredWithTaskbarTabs())
			{
				HICON hIcon = pMDIChild->GetFrameIcon();
				if (hIcon == NULL)
				{
					CWnd* pTopLevel = GetTopLevelFrame();

					if (pTopLevel != NULL)
					{
						hIcon = globalUtils.GetWndIcon(pTopLevel);
					}
				}

				if (hIcon != NULL)
				{
					pMDIChild->UpdateTaskbarTabIcon(hIcon);
				}
			}
		}

		//--------------------------------
		// Get tab label (window caption):
		//--------------------------------
		CString strTabLabel;
		if (pMDIChild != NULL)
		{
			strTabLabel = pMDIChild->GetFrameText ();
		}
		else
		{
			pWndChild->GetWindowText (strTabLabel);
		}

		//----------------
		// Get tab colors:
		//----------------
		COLORREF clrBkTab = (COLORREF)-1;
		COLORREF clrTextTab = (COLORREF)-1;

		if (pMDIChild != NULL)
		{
			clrBkTab = pMDIChild->GetMDITabBkColor();
			clrTextTab = pMDIChild->GetMDITabTextColor();
		}

		int iTabIndex = m_wndTab.GetTabFromHwnd (pWndChild->GetSafeHwnd ());
		if (iTabIndex >= 0)
		{
			//---------------------------------
			// Tab is already exist, update it:
			//---------------------------------
			if (pWndChild->GetStyle () & WS_VISIBLE)
			{
				CString strCurTabLabel;
				m_wndTab.GetTabLabel (iTabIndex, strCurTabLabel);

				if (strCurTabLabel != strTabLabel)
				{
					//-----------------------------
					// Text was changed, update it:
					//-----------------------------
					m_wndTab.SetTabLabel (iTabIndex, strTabLabel);
					bRecalcLayout = TRUE;
				}

				if (m_wndTab.GetTabIcon (iTabIndex) != (UINT) iIcon)
				{
					//-----------------------------
					// Icon was changed, update it:
					//-----------------------------
					m_wndTab.SetTabIcon (iTabIndex, iIcon);
					bRecalcLayout = TRUE;
				}

				if (m_wndTab.GetTabBkColor(iTabIndex) != clrBkTab)
				{
					m_wndTab.SetTabBkColor(iTabIndex, clrBkTab);
				}

				if (m_wndTab.GetTabTextColor(iTabIndex) != clrTextTab)
				{
					m_wndTab.SetTabTextColor(iTabIndex, clrTextTab);
				}
			}
			else
			{
				//----------------------------------
				// Window is hidden now, remove tab:
				//----------------------------------
				m_wndTab.RemoveTab (iTabIndex);
				bRecalcLayout = TRUE;
			}
		}
		else if ((pMDIChild == NULL || !pMDIChild->m_bToBeDestroyed) && bIsShowTab)
		{
			//----------------------
			// New item, add it now:
			//----------------------
			m_wndTab.AddTab (pWndChild, strTabLabel, iIcon);
			m_wndTab.SetActiveTab (m_wndTab.GetTabsNum () - 1);

			bRecalcLayout = TRUE;
		}

		pWndChild = pWndChild->GetNextWindow ();
    }

	CWnd* pActiveWnd = m_wndTab.GetActiveWindow();
	if (pActiveWnd->GetSafeHwnd() != NULL)
	{
		pActiveWnd->BringWindowToTop ();
	}

	if (bRecalcLayout && GetParentFrame () != NULL)
	{
		GetParentFrame ()->RecalcLayout ();
	}

	if (bSetActiveTabVisible)
	{
		m_wndTab.EnsureVisible (m_wndTab.GetActiveTab ());
	}

	if (bTabWndEmpty && m_wndTab.GetTabsNum () > 0 || m_wndTab.GetTabsNum () == 0)
	{
		GetParentFrame ()->RecalcLayout ();
		RedrawWindow (NULL, NULL, 
						RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW |
						RDW_ERASE | RDW_INTERNALPAINT);
	}
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::UpdateMDITabbedGroups (BOOL bSetActiveTabVisible)
{
	if (m_bDisableUpdateTabs)
	{
		return;
	}

	BOOL bRecalcLayout = FALSE;
	CWnd* pWndChild = GetWindow (GW_CHILD);
	HWND hwndActive = NULL;

	while (pWndChild != NULL)
	{
		ASSERT_VALID (pWndChild);

		CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pWndChild);

		if (pMDIChild == NULL)
		{
			pWndChild = pWndChild->GetNextWindow ();
			continue;
		}

		// always modify style
		pMDIChild->ModifyStyle (CBCGPMDIChildWnd::m_dwExcludeStyle | WS_MAXIMIZE | WS_SYSMENU, 0, SWP_NOZORDER);	

		BOOL bIsShowTab = pMDIChild->CanShowOnMDITabs();
		if (bIsShowTab && (pWndChild->GetStyle () & WS_VISIBLE) == 0)
		{
			bIsShowTab = FALSE;

			COleServerDoc* pDoc = DYNAMIC_DOWNCAST(COleServerDoc, pMDIChild->GetActiveDocument());
			if (pDoc != NULL)
			{
				ASSERT_VALID(pDoc);

				if (pDoc->IsEmbedded())
				{
					bIsShowTab = TRUE;
				}
			}
		}

		CString strTabLabel = pMDIChild->GetFrameText ();		

		COLORREF clrBkTab = pMDIChild->GetMDITabBkColor();
		COLORREF clrTextTab = pMDIChild->GetMDITabTextColor();

		CBCGPTabWnd* pRelatedTabWnd = pMDIChild->GetRelatedTabGroup ();

		BOOL bRemoved = FALSE;
		if (pRelatedTabWnd == NULL && !pMDIChild->m_bToBeDestroyed && bIsShowTab)
		{
			if (m_lstTabbedGroups.IsEmpty ())
			{
				pRelatedTabWnd = CreateTabGroup (NULL);
				m_lstTabbedGroups.AddTail (pRelatedTabWnd);
			}
			else
			{
				// new window to be added			
				pRelatedTabWnd = FindActiveTabWnd ();

				if (pRelatedTabWnd == NULL)
				{
					pRelatedTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetHead ());
				}
			}

			ASSERT_VALID (pRelatedTabWnd);

			pMDIChild->SetRelatedTabGroup (pRelatedTabWnd);
			pRelatedTabWnd->AddTab (pWndChild, strTabLabel);

			int iTabIndex = pRelatedTabWnd->GetTabsNum() - 1;

			if (iTabIndex == 0)	// 1-st tab
			{
				CWnd* pFocus = GetFocus();
				if (pFocus->GetSafeHwnd () != NULL && (IsChild (pFocus) || pFocus->GetSafeHwnd () == GetSafeHwnd ()))
				{
					pRelatedTabWnd->SetMDIFocused();
				}
			}

			if (clrBkTab != (COLORREF)-1)
			{
				pRelatedTabWnd->SetTabBkColor(iTabIndex, clrBkTab);
			}
			
			if (m_wndTab.GetTabTextColor(iTabIndex) != clrTextTab)
			{
				pRelatedTabWnd->SetTabTextColor(iTabIndex, clrTextTab);
			}

			if (!pRelatedTabWnd->IsWindowVisible ())
			{
				pRelatedTabWnd->ShowWindow (SW_SHOWNA);
			}
			
			hwndActive = pWndChild->GetSafeHwnd ();
			
		}
		else if (pRelatedTabWnd != NULL)
		{
			int iTabIndex = pRelatedTabWnd->GetTabFromHwnd (pWndChild->GetSafeHwnd ());
			if (iTabIndex >= 0)
			{
				//---------------------------------
				// Tab is already exist, update it:
				//---------------------------------
				if (pWndChild->GetStyle () & WS_VISIBLE)
				{
					CString strCurTabLabel;
					pRelatedTabWnd->GetTabLabel (iTabIndex, strCurTabLabel);

					if (strCurTabLabel != strTabLabel)
					{
						//-----------------------------
						// Text was changed, update it:
						//-----------------------------
						pRelatedTabWnd->SetTabLabel (iTabIndex, strTabLabel);
						bRecalcLayout = TRUE;
					}
				}
				else
				{
					//----------------------------------
					// Window is hidden now, remove tab:
					//----------------------------------
					pRelatedTabWnd->RemoveTab (iTabIndex);
					if (pRelatedTabWnd->GetTabsNum () == 0)
					{
						RemoveTabGroup (pRelatedTabWnd, FALSE);
					}

					bRecalcLayout = TRUE;
					bRemoved = TRUE;
				}
			}
		}

		CImageList* pImageList = NULL;
		m_mapTabIcons.Lookup ((UINT_PTR)pRelatedTabWnd->GetSafeHwnd(), pImageList);

		if (pImageList != NULL)
		{
			ASSERT_VALID (pImageList);

			int iIcon = -1;
			if (m_mdiTabParams.m_bTabIcons)
			{
				HICON hIcon = NULL;
				if (pMDIChild != NULL)
				{
					hIcon = pMDIChild->GetFrameIcon ();
				}

				if (hIcon != NULL)
				{
					if (!pRelatedTabWnd->IsIconAdded (hIcon, iIcon))
					{
						iIcon = pImageList->Add (hIcon);
						pRelatedTabWnd->AddIcon (hIcon, iIcon);
					}

					if (pRelatedTabWnd->GetImageList () != pImageList)
					{
						pRelatedTabWnd->SetImageList (pImageList->GetSafeHandle ());
					}
				}

				if (!bRemoved)
				{
					int iTabIndex = pRelatedTabWnd->GetTabFromHwnd (pMDIChild->GetSafeHwnd ());
					if (pRelatedTabWnd->GetTabIcon (iTabIndex) != (UINT) iIcon)
					{
						//-----------------------------
						// Icon was changed, update it:
						//-----------------------------
						pRelatedTabWnd->SetTabIcon (iTabIndex, iIcon);
					}

					pMDIChild->UpdateTaskbarTabIcon(hIcon);
				}
			}
			else
			{
				pRelatedTabWnd->ResetImageList ();
				m_mapIcons.RemoveAll ();

				while (pImageList->GetImageCount () > 0)
				{
					pImageList->Remove (0);
				}

				// set tab icons for taskbar tabs
				if (pMDIChild != NULL && pMDIChild->IsTaskbarTabsSupportEnabled() && pMDIChild->IsRegisteredWithTaskbarTabs())
				{
					HICON hIcon = pMDIChild->GetFrameIcon();
					if (hIcon == NULL)
					{
						CWnd* pTopLevel = GetTopLevelFrame();

						if (pTopLevel != NULL)
						{
							hIcon = globalUtils.GetWndIcon(pTopLevel);
						}
					}

					if (hIcon != NULL)
					{
						pMDIChild->UpdateTaskbarTabIcon(hIcon);
					}
				}

				bRecalcLayout = TRUE;
			}
		}

		pWndChild = pWndChild->GetNextWindow ();
    }

	for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextTab = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextTab);
		AdjustMDIChildren (pNextTab);
	}

	if (bRecalcLayout && GetParentFrame () != NULL)
	{
		GetParentFrame ()->RecalcLayout ();
	}
	
	if (hwndActive != NULL)
	{
		SetActiveTab (hwndActive);
		::BringWindowToTop(hwndActive);
	}

	if (bSetActiveTabVisible)
	{
		CBCGPTabWnd* pActiveWnd = FindActiveTabWnd ();
		if (pActiveWnd != NULL)
		{
			ASSERT_VALID (pActiveWnd);
			pActiveWnd->EnsureVisible (pActiveWnd->GetActiveTab ());
		}
	}
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::OnStyleChanging (int nStyleType, LPSTYLESTRUCT lpStyleStruct)
{
	if (nStyleType == GWL_EXSTYLE && !IsKeepClientEdge ())
	{
		lpStyleStruct->styleNew = lpStyleStruct->styleOld & ~WS_EX_CLIENTEDGE;
	}
	
	CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST(CBCGPMDIFrameWnd, GetParentFrame());
	if (pMainFrame->GetSafeHwnd() != NULL)
	{
		pMainFrame->OnMDIClientAreaStyleChanging(nStyleType, lpStyleStruct);
	}

	CWnd::OnStyleChanging (nStyleType, lpStyleStruct);
}
//*************************************************************************************
BOOL CBCGPMainClientAreaWnd::IsKeepClientEdge () 
{
	BOOL bKeepEdge = FALSE;
	HWND hwndActive = (HWND) SendMessage (WM_MDIGETACTIVE, 0, 0);
	if (hwndActive != NULL)
	{
		CWnd* pWnd = CWnd::FromHandle (hwndActive);
		if (pWnd != NULL && ::IsWindow (pWnd->GetSafeHwnd ()))
		{
			bKeepEdge = (pWnd->GetStyle () & WS_SYSMENU) != 0;
		}
	}

	return !m_bIsMDITabbedGroup && bKeepEdge;
}
//*************************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnGetDragBounds (WPARAM wp, LPARAM lp)
{
	if (!m_bIsMDITabbedGroup || m_lstTabbedGroups.IsEmpty ())
	{
		return 0;
	}

	CBCGPTabWnd* pTabWndToResize = (CBCGPTabWnd*) (wp);
	LPRECT lpRectBounds = (LPRECT) (lp);

	if (pTabWndToResize == NULL)
	{
		return 0;
	}

	ASSERT_VALID (pTabWndToResize);
	CBCGPTabWnd* pNextTabWnd = GetNextTabWnd (pTabWndToResize);

	if (pNextTabWnd == NULL)
	{
		return 0;
	}

	ASSERT (m_groupAlignment != GROUP_NO_ALIGN);

	CRect rectTabWndToResize;
	CRect rectNextTabWnd;

	pTabWndToResize->GetWindowRect (rectTabWndToResize);
	pNextTabWnd->GetWindowRect (rectNextTabWnd);

	rectTabWndToResize.UnionRect (rectTabWndToResize, rectNextTabWnd);

	if (m_groupAlignment == GROUP_VERT_ALIGN)
	{
		rectTabWndToResize.left += m_nResizeMargin;
		rectTabWndToResize.right -= m_nResizeMargin;
	}
	else
	{
		rectTabWndToResize.top += m_nResizeMargin;
		rectTabWndToResize.bottom -= m_nResizeMargin;
	}

	CopyRect (lpRectBounds, &rectTabWndToResize);
	return TRUE;
}
//*************************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnDragComplete (WPARAM wp, LPARAM lp)
{
	if (!m_bIsMDITabbedGroup || m_lstTabbedGroups.IsEmpty ())
	{
		return 0;
	}

	::SetCursor (AfxGetApp ()->LoadStandardCursor (IDC_ARROW));
	DrawNewGroupRect(NULL);
	m_rectNewTabGroup.SetRectEmpty ();

	CBCGPTabWnd* pTabWndToResize = (CBCGPTabWnd*) (wp);
	LPRECT lpRectResized = (LPRECT) (lp);

	ASSERT_VALID (pTabWndToResize);
	CBCGPTabWnd* pNextTabWnd = GetNextTabWnd (pTabWndToResize);

	if (pNextTabWnd == NULL)
	{
		return 0;
	}

	m_bInsideDragComplete = TRUE;

	ASSERT (m_groupAlignment != GROUP_NO_ALIGN);

	ScreenToClient (lpRectResized);
	pTabWndToResize->SetWindowPos (NULL, -1, -1, 
								   lpRectResized->right - lpRectResized->left,
								   lpRectResized->bottom - lpRectResized->top, 
								   SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);

	CRect rectNextWnd;
	pNextTabWnd->GetWindowRect (rectNextWnd);
	ScreenToClient (rectNextWnd);

	m_groupAlignment == GROUP_VERT_ALIGN ?  rectNextWnd.left = lpRectResized->right : 
											rectNextWnd.top = lpRectResized->bottom;

	pNextTabWnd->SetWindowPos (NULL, rectNextWnd.left, rectNextWnd.top, 
							   rectNextWnd.Width (), rectNextWnd.Height (), 
							   SWP_NOZORDER | SWP_NOACTIVATE);

	AdjustMDIChildren (pTabWndToResize);
	AdjustMDIChildren (pNextTabWnd);

	pTabWndToResize->RedrawWindow();
	pNextTabWnd->RedrawWindow();

	m_bInsideDragComplete = FALSE;

	return TRUE;
}
//*************************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnTabGroupMouseMove (WPARAM /*wp*/, LPARAM lp)
{
	CBCGPTabWnd* pTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, GetCapture ());
	if (pTabWnd == NULL)
	{
		return 0;
	}

	m_bDetachMDIChildrenOnly = FALSE;

	if (m_lstTabbedGroups.GetCount () == 1 && pTabWnd->GetTabsNum () == 1)
	{
		if (m_bEnableTearOffMDIChildren)
		{
			m_bDetachMDIChildrenOnly = TRUE;
		}
		else
		{
			return 0;
		}
	}

	if (m_pWndMDIChildDrag == NULL && m_bEnableTearOffMDIChildren)
	{
		CList<CBCGPMDIChildWnd*, CBCGPMDIChildWnd*>	lstMDIChild;

		CList<int, int> lstSelectedTabs;
		if (pTabWnd->GetSelectedTabs(lstSelectedTabs) > 0)
		{
			CList<HWND, HWND> lstTabWnds;
			POSITION pos = NULL;
			
			for (pos = lstSelectedTabs.GetHeadPosition(); pos != NULL;)
			{
				int nIndex = lstSelectedTabs.GetNext(pos);

				CBCGPMDIChildWnd* pMDIChildFrame = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pTabWnd->GetTabWnd(nIndex));
				if (pMDIChildFrame != NULL)
				{
					ASSERT_VALID(pMDIChildFrame);

					if (pMDIChildFrame->CanBeDetached())
					{
						lstMDIChild.AddTail(pMDIChildFrame);
					}
				}
			}
		}
		else
		{
			CBCGPMDIChildWnd* pMDIChildFrame = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pTabWnd->GetActiveWnd());
			if (pMDIChildFrame != NULL)
			{
				ASSERT_VALID(pMDIChildFrame);

				if (pMDIChildFrame->CanBeDetached())
				{
					lstMDIChild.AddTail(pMDIChildFrame);
				}
			}
		}

		if (!lstMDIChild.IsEmpty())
		{
			m_pWndMDIChildDrag = new CBCGPMDIChildDragWnd(GetSafeHwnd());
			ASSERT_VALID (m_pWndMDIChildDrag);

			m_pWndMDIChildDrag->Create(&lstMDIChild, pTabWnd);
			g_hwdTopMDIClientArea = GetSafeHwnd();
		}
	}

	CPoint point(BCG_GET_X_LPARAM(lp), BCG_GET_Y_LPARAM(lp));

	if (pTabWnd->IsPtInTabArea (point) && GetSafeHwnd() == g_hwdTopMDIClientArea)
	{
		::SetCursor (AfxGetApp ()->LoadStandardCursor (IDC_ARROW));
		DrawNewGroupRect(NULL);
		m_rectNewTabGroup.SetRectEmpty ();

		if (m_pWndMDIChildDrag->GetSafeHwnd() != NULL)
		{
			m_pWndMDIChildDrag->ShowWindow(SW_HIDE);
		}

		return 0;
	}

	CPoint pointScreen = point;
	pTabWnd->ClientToScreen (&pointScreen);

	CRect rectWnd;
	GetClientRect (rectWnd);

	ClientToScreen (rectWnd);

	if (globalData.m_hcurMoveTab == NULL)
	{
		CBCGPLocalResource locaRes;

		globalData.m_hcurMoveTab = AfxGetApp ()->LoadCursor (IDC_BCGBARRES_MOVE_TAB);
		globalData.m_hcurNoMoveTab = AfxGetApp ()->LoadCursor (IDC_BCGBARRES_NO_MOVE_TAB);
	}

	BOOL bContinueMove = !rectWnd.PtInRect (pointScreen);

	if (!bContinueMove && m_pWndMDIChildDrag->GetSafeHwnd() != NULL && m_pWndMDIChildDrag->m_hwndDropMDIFrame != NULL)
	{
		CBCGPMDIFrameWnd* pDropMDIFrame = DYNAMIC_DOWNCAST(CBCGPMDIFrameWnd, CWnd::FromHandlePermanent(m_pWndMDIChildDrag->m_hwndDropMDIFrame));
		if (pDropMDIFrame != NULL)
		{
			ASSERT_VALID(pDropMDIFrame);
			
			CRect rectDropFrame;
			pDropMDIFrame->GetWindowRect(rectDropFrame);
			
			bContinueMove = rectDropFrame.PtInRect(pointScreen);
		}
	}

	if (bContinueMove)
	{
		DrawNewGroupRect(NULL);
		m_rectNewTabGroup.SetRectEmpty ();

		if (m_pWndMDIChildDrag->GetSafeHwnd() != NULL)
		{
			m_pWndMDIChildDrag->MoveToCursor();
		}
		else
		{
			::SetCursor (globalData.m_hcurNoMoveTab);
		}

		return TRUE;
	}

	::SetCursor (globalData.m_hcurMoveTab);

	CBCGPTabWnd* pHoveredTabWnd = TabWndFromPoint (pointScreen);

	if (pHoveredTabWnd == NULL)
	{
		DrawNewGroupRect(NULL);
		m_rectNewTabGroup.SetRectEmpty ();

		if (m_pWndMDIChildDrag->GetSafeHwnd() != NULL)
		{
			m_pWndMDIChildDrag->MoveToCursor();
		}

		return 0;
	}

	if (!m_bDetachMDIChildrenOnly)
	{
		CRect rectScreenHoveredWnd;
		pHoveredTabWnd->GetWindowRect (rectScreenHoveredWnd);

		CRect rectMargin = rectScreenHoveredWnd;

		BOOL bCalcVertRect = TRUE;

		if (m_groupAlignment == GROUP_NO_ALIGN)
		{
			bCalcVertRect = rectScreenHoveredWnd.right - pointScreen.x < 
							rectScreenHoveredWnd.bottom - pointScreen.y;
		}
		else
		{
			bCalcVertRect = m_groupAlignment == GROUP_VERT_ALIGN;
		}

		if (m_groupAlignment == GROUP_VERT_ALIGN || bCalcVertRect)
		{
			rectMargin.left = rectScreenHoveredWnd.right - m_nNewGroupMargin;
			bCalcVertRect = TRUE;
		}
		else if (m_groupAlignment == GROUP_HORZ_ALIGN || !bCalcVertRect)
		{
			rectMargin.top  = rectScreenHoveredWnd.bottom - m_nNewGroupMargin; 
			bCalcVertRect = FALSE;
		}

		CRect rectNew = rectScreenHoveredWnd;

		bCalcVertRect ? rectNew.left = rectScreenHoveredWnd.right - rectScreenHoveredWnd.Width () / 2:
						rectNew.top = rectScreenHoveredWnd.bottom - rectScreenHoveredWnd.Height () / 2; 

		if (!rectMargin.PtInRect (pointScreen)) 
		{
			if (pHoveredTabWnd == pTabWnd)
			{
				rectNew.SetRectEmpty ();
			}
			else
			{
				CPoint pointClient = pointScreen;
				pHoveredTabWnd->ScreenToClient (&pointClient);
				if (pHoveredTabWnd->IsPtInTabArea (pointClient))
				{
					pHoveredTabWnd->GetWndArea (rectNew);
					pHoveredTabWnd->ClientToScreen (rectNew);
				}
				else
				{
					rectNew.SetRectEmpty ();
				}
			}
		}
		else if (pHoveredTabWnd == pTabWnd && pTabWnd->GetTabsNum () == 1)
		{
			rectNew.SetRectEmpty ();
		}

		DrawNewGroupRect(rectNew);
		m_rectNewTabGroup = rectNew;
		m_bNewVericalGroup = bCalcVertRect;
	}

	if (m_pWndMDIChildDrag->GetSafeHwnd() != NULL)
	{
		m_pWndMDIChildDrag->ShowWindow(SW_HIDE);
	}

	return TRUE;
}
//*************************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnMoveTabComplete(WPARAM wp, LPARAM lp)
{
	CRect rectNewTabGroup = m_rectNewTabGroup;
	DrawNewGroupRect(NULL);
	m_rectNewTabGroup.SetRectEmpty ();
	g_hwdTopMDIClientArea = NULL;
	::SetCursor (AfxGetApp ()->LoadStandardCursor (IDC_ARROW));

	if (m_pWndMDIChildDrag->GetSafeHwnd() != NULL)
	{
		BOOL bDetachFrame = FALSE;
		
		if (m_pWndMDIChildDrag->IsWindowVisible())
		{
			CBCGPMDIFrameWnd* pMDIFrame = DYNAMIC_DOWNCAST(CBCGPMDIFrameWnd, GetParent());
			ASSERT_VALID(pMDIFrame);

			CBCGPMDIFrameWnd* pDropMDIFrame = NULL;
			CBCGPMDIFrameWnd* pNewMDIFrame = NULL;
			
			if (m_pWndMDIChildDrag->m_hwndDropMDIFrame != NULL && ::IsWindow(m_pWndMDIChildDrag->m_hwndDropMDIFrame))
			{
				pDropMDIFrame = DYNAMIC_DOWNCAST(CBCGPMDIFrameWnd, CWnd::FromHandlePermanent(m_pWndMDIChildDrag->m_hwndDropMDIFrame));
				if (pDropMDIFrame != NULL)
				{
					ASSERT_VALID(pDropMDIFrame);
					
					if (pDropMDIFrame->IsRibbonBackstageView())
					{
						pDropMDIFrame = NULL;
					}
					else
					{
						pDropMDIFrame->m_wndClientArea.DrawNewGroupRect(NULL);
						pDropMDIFrame->LockWindowUpdate();
					}
				}
			}

			for (POSITION pos = m_pWndMDIChildDrag->m_lstMDIChild.GetHeadPosition(); pos != NULL;)
			{
				CBCGPMDIChildWnd* pMDIChildFrame = m_pWndMDIChildDrag->m_lstMDIChild.GetNext(pos);
				if (pMDIChildFrame != NULL)
				{
					ASSERT_VALID(pMDIChildFrame);

					if (pDropMDIFrame != NULL)
					{
						ASSERT_VALID(pDropMDIFrame);
						pDropMDIFrame->AttachMDIChild(pMDIChildFrame);
					}
					else
					{
						if (pNewMDIFrame == NULL)
						{
							CRect rectNewFrame;
							m_pWndMDIChildDrag->GetWindowRect(rectNewFrame);

							rectNewFrame.top -= GetSystemMetrics(SM_CYCAPTION);
							rectNewFrame.InflateRect(GetSystemMetrics(SM_CXSIZEFRAME), GetSystemMetrics(SM_CYSIZEFRAME));
						
							pNewMDIFrame = pMDIFrame->DetachMDIChild(pMDIChildFrame, rectNewFrame);
							ASSERT_VALID(pNewMDIFrame);

							pNewMDIFrame->LockWindowUpdate();
						}
						else
						{
							ASSERT_VALID(pNewMDIFrame);
							pNewMDIFrame->AttachMDIChild(pMDIChildFrame);
						}
					}
					
					bDetachFrame = TRUE;
				}
			}

			if (pDropMDIFrame != NULL)
			{
				pDropMDIFrame->UnlockWindowUpdate();
				pDropMDIFrame->RedrawWindow();
			}
			else if (pNewMDIFrame != NULL)
			{
				pNewMDIFrame->UnlockWindowUpdate();
				pNewMDIFrame->RedrawWindow();
			}
		}
		
		m_pWndMDIChildDrag->DestroyWindow();
		m_pWndMDIChildDrag = NULL;
		
		if (bDetachFrame)
		{
			return 0;
		}
	}

	if (m_bDetachMDIChildrenOnly)
	{
		return 0;
	}

	CBCGPTabWnd* pTabWnd = (CBCGPTabWnd*)wp;
	if (pTabWnd == NULL)
	{
		return 0;
	}

	ASSERT_VALID (pTabWnd);

	CPoint point(BCG_GET_X_LPARAM(lp), BCG_GET_Y_LPARAM(lp));

	CPoint pointScreen = point;
	pTabWnd->ClientToScreen (&pointScreen);

	CBCGPTabWnd* pHoveredTabWnd = TabWndFromPoint (pointScreen);

	if (pHoveredTabWnd == NULL)
	{
		return 0;
	}

	ASSERT_VALID (pHoveredTabWnd);

	BOOL bMenuResult = TRUE;
	if (rectNewTabGroup.IsRectEmpty ())
	{
		CBCGPMDIFrameWnd* pMDIFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParent ());
		ASSERT_VALID (pMDIFrame);

		CPoint point;
		GetCursorPos (&point);

		DWORD dwAllowedItems = GetMDITabsContextMenuAllowedItems ();

		if (dwAllowedItems != 0)
		{
			bMenuResult = pMDIFrame->OnShowMDITabContextMenu (point, dwAllowedItems, TRUE);
		}
	}

	CRect rectHoveredWnd;
	pHoveredTabWnd->GetWndArea (rectHoveredWnd);
	pHoveredTabWnd->ClientToScreen (rectHoveredWnd);

	if (!bMenuResult)
	{
		if (pTabWnd != pHoveredTabWnd)
		{
			MoveWindowToTabGroup (pTabWnd, pHoveredTabWnd);
		}
		else if (pTabWnd->GetTabsNum () > 1)
		{
			BOOL bVertGroup = FALSE;
			if (m_groupAlignment == GROUP_HORZ_ALIGN)
			{
				rectHoveredWnd.top = rectHoveredWnd.bottom - rectHoveredWnd.Height () / 2;
			}
			else
			{
				rectHoveredWnd.left = rectHoveredWnd.right - rectHoveredWnd.Width () / 2;
				bVertGroup = TRUE;
			}

			CBCGPTabWnd* pNewTabWnd = CreateNewTabGroup (pHoveredTabWnd, 
					rectHoveredWnd, bVertGroup); 
			MoveWindowToTabGroup (pTabWnd, pNewTabWnd);
		}
	}
	else if (!rectNewTabGroup.IsRectEmpty ())
	{
		if (rectNewTabGroup == rectHoveredWnd)
		{
			MoveWindowToTabGroup (pTabWnd, pHoveredTabWnd);
		}
		else
		{
			CBCGPTabWnd* pNewTabWnd = CreateNewTabGroup (pHoveredTabWnd, 
					rectNewTabGroup, m_bNewVericalGroup); 
			MoveWindowToTabGroup (pTabWnd, pNewTabWnd);
		}
	}

	
	return 0;
}
//*************************************************************************************
BOOL CBCGPMainClientAreaWnd::MoveWindowToTabGroup (CBCGPTabWnd* pTabWndFrom, CBCGPTabWnd* pTabWndTo, int nIdxFrom)
{
	ASSERT_VALID (pTabWndFrom);
	ASSERT_VALID (pTabWndTo);

	HWND hwndFrom = pTabWndFrom->GetSafeHwnd ();
	HWND hwndTo	  = pTabWndTo->GetSafeHwnd ();

	int nIdx = nIdxFrom;
	if (nIdx == -1)
	{
		if (m_mdiTabParams.m_bTabMultipleSelection)
		{
			CList<int, int> lstSelectedTabs;
			if (pTabWndFrom->GetSelectedTabs(lstSelectedTabs) > 0)
			{
				CList<HWND, HWND> lstTabWnds;
				POSITION pos = NULL;

				for (pos = lstSelectedTabs.GetHeadPosition(); pos != NULL;)
				{
					int nIndex = lstSelectedTabs.GetNext(pos);
					HWND hWnd = pTabWndFrom->GetTabWnd(nIndex)->GetSafeHwnd();

					if (hWnd != NULL)
					{
						lstTabWnds.AddTail(hWnd);
					}
				}

				SetRedraw(FALSE);

				for (pos = lstTabWnds.GetHeadPosition(); pos != NULL;)
				{
					HWND hwndTab = lstTabWnds.GetNext(pos);
					int nIndex = pTabWndFrom->GetTabFromHwnd(hwndTab);

					if (nIndex < 0 || !MoveWindowToTabGroup(pTabWndFrom, pTabWndTo, nIndex))
					{
						SetRedraw();
						UpdateTabs();

						return FALSE;
					}
				}

				SetRedraw();
				UpdateTabs();

				return TRUE;
			}
		}

		nIdx = pTabWndFrom->GetActiveTab ();
	}

	if (nIdx == -1)
	{
		return FALSE;
	}

	CBCGPMDIChildWnd* pWnd = DYNAMIC_DOWNCAST (CBCGPMDIChildWnd, pTabWndFrom->GetTabWnd (nIdx));

	if (pWnd == NULL)
	{
		return FALSE;
	}

	ASSERT_VALID (pWnd);

	// Keep Tab Colors when moving tab around
	COLORREF clrTabBkColor = pTabWndFrom->GetTabBkColor(nIdx);
	COLORREF clrTabTextColor = pTabWndFrom->GetTabTextColor(nIdx);

	CString strTabLabel = pWnd->GetFrameText ();		

	pTabWndFrom->RemoveTab (nIdx, TRUE);
	pWnd->SetRelatedTabGroup (pTabWndTo);
	pTabWndTo->AddTab (pWnd, strTabLabel);
	
	// Restore Tab Colors:
	pTabWndTo->SetTabBkColor(pTabWndTo->GetTabsNum() - 1, clrTabBkColor);
	pTabWndTo->SetTabTextColor(pTabWndTo->GetTabsNum() - 1, clrTabTextColor);

	if (pTabWndFrom->GetTabsNum () == 0)
	{
		RemoveTabGroup (pTabWndFrom);
		UpdateMDITabbedGroups(TRUE);
		pTabWndTo->RecalcLayout ();
	}
	else
	{
		AdjustMDIChildren (pTabWndFrom);
	}

	AdjustMDIChildren (pTabWndTo);
	SetActiveTab (pWnd->GetSafeHwnd ());

	CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParentFrame ());
	ASSERT_VALID (pMainFrame);

	if (pMainFrame != NULL)
	{
		pMainFrame->SendMessage (BCGM_ON_MOVETOTABGROUP, (WPARAM) hwndFrom, (LPARAM) hwndTo);
	}

	return TRUE;
}
//*************************************************************************************
CBCGPTabWnd* CBCGPMainClientAreaWnd::CreateNewTabGroup (CBCGPTabWnd* pTabWndAfter, CRect rectGroup, 
												BOOL bVertical)
{
	ASSERT_VALID (pTabWndAfter);
	POSITION pos = m_lstTabbedGroups.Find (pTabWndAfter);

	if (pos == NULL)
	{
		return NULL;
	}

	CBCGPTabWnd* pNewTabWnd = CreateTabGroup (NULL);
	pTabWndAfter->SetResizeMode (bVertical ? CBCGPTabWnd::RESIZE_VERT : CBCGPTabWnd::RESIZE_HORIZ);

	m_lstTabbedGroups.InsertAfter (pos, pNewTabWnd);
	if (pNewTabWnd != m_lstTabbedGroups.GetTail ())
	{
		pNewTabWnd->SetResizeMode (bVertical ? CBCGPTabWnd::RESIZE_VERT : CBCGPTabWnd::RESIZE_HORIZ);
	}

	m_groupAlignment = bVertical ? GROUP_VERT_ALIGN : GROUP_HORZ_ALIGN;

	CRect rectWndAfter;
	pTabWndAfter->GetWindowRect (rectWndAfter);

	ScreenToClient (rectGroup);
	ScreenToClient (rectWndAfter);
	
	if (bVertical)		
	{
		rectWndAfter.right -= rectGroup.Width ();
		rectGroup.top = rectWndAfter.top;
		rectGroup.bottom = rectWndAfter.bottom;
	}
	else
	{
		rectWndAfter.bottom -= rectGroup.Height ();
		rectGroup.left = rectWndAfter.left;
		rectGroup.right = rectWndAfter.right;
	}

	pTabWndAfter->SetWindowPos (NULL, -1, -1, rectWndAfter.Width (), rectWndAfter.Height (),
									SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
	pNewTabWnd->SetWindowPos (NULL, rectGroup.left, rectGroup.top, 
									rectGroup.Width (), rectGroup.Height (),
									SWP_NOZORDER |  SWP_NOACTIVATE);

	AdjustMDIChildren (pTabWndAfter);

	pNewTabWnd->SetMDIFocused(m_bActive);
	return pNewTabWnd;
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::RemoveTabGroup (CBCGPTabWnd* pTabWnd, BOOL /*bRecalcLayout*/)
{
	ASSERT_VALID (pTabWnd);
	ASSERT (pTabWnd->GetTabsNum () == 0);

	POSITION pos = m_lstTabbedGroups.Find (pTabWnd);
	if (pos == NULL)
	{
		TRACE0 ("Attempt to remove non-existing tab group");
		return;
	}

	CBCGPTabWnd* pSiblingTabWndToResize = NULL;
	POSITION posNextPrev = pos;
	BOOL bNext = FALSE;

	if (m_lstTabbedGroups.GetHeadPosition () == pos)
	{
		m_lstTabbedGroups.GetNext (posNextPrev);
		bNext = TRUE;
	}
	else
	{
		m_lstTabbedGroups.GetPrev (posNextPrev);
	}

	if (posNextPrev != NULL)
	{
		pSiblingTabWndToResize = DYNAMIC_DOWNCAST (CBCGPTabWnd, 
										m_lstTabbedGroups.GetAt (posNextPrev));
	}

	
	m_lstTabbedGroups.RemoveAt (pos);
	pTabWnd->ShowWindow (SW_HIDE);

	if (m_lstTabbedGroups.GetCount () > 0)
	{
		CBCGPTabWnd* pLastTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetTail ());
		ASSERT_VALID (pLastTabWnd);
		pLastTabWnd->SetResizeMode (CBCGPTabWnd::RESIZE_NO);
	}

	if (m_lstTabbedGroups.GetCount () <= 1)
	{
		m_groupAlignment = GROUP_NO_ALIGN;
	}

	if (pSiblingTabWndToResize != NULL)
	{
		CRect rectTabWndToRemove;
		pTabWnd->GetWindowRect (rectTabWndToRemove);
		CRect rectSiblingWnd;
		pSiblingTabWndToResize->GetWindowRect (rectSiblingWnd);
		
		rectSiblingWnd.UnionRect (rectSiblingWnd, rectTabWndToRemove);
		ScreenToClient (rectSiblingWnd);
		pSiblingTabWndToResize->SetWindowPos (NULL, rectSiblingWnd.left, rectSiblingWnd.top, 
							rectSiblingWnd.Width (), rectSiblingWnd.Height (), 
							SWP_NOZORDER);
		
		AdjustMDIChildren (pSiblingTabWndToResize);
	}

	CImageList* pImageList = NULL;
	if (m_mapTabIcons.Lookup ((UINT_PTR)pTabWnd->GetSafeHwnd(), pImageList) && 
		pImageList != NULL)
	{
		delete pImageList;
		m_mapTabIcons.RemoveKey ((UINT_PTR)pTabWnd->GetSafeHwnd());
	}

	pTabWnd->ResetImageList();

	m_lstRemovedTabbedGroups.AddTail (pTabWnd);
	pTabWnd->ShowWindow(SW_HIDE);

	return;
}
//*************************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnCancelTabMove (WPARAM, LPARAM)
{
	if (m_pWndMDIChildDrag->GetSafeHwnd() != NULL)
	{
		m_pWndMDIChildDrag->DestroyWindow();
		m_pWndMDIChildDrag = NULL;
	}

	DrawNewGroupRect(NULL);
	m_rectNewTabGroup.SetRectEmpty ();
	g_hwdTopMDIClientArea = NULL;

	return 0;
}
//*************************************************************************************
DWORD CBCGPMainClientAreaWnd::GetMDITabsContextMenuAllowedItems ()
{
	CBCGPTabWnd* pActiveTabWnd = FindActiveTabWndByActiveChild ();
	if (pActiveTabWnd == NULL)
	{
		return 0;
	}

	DWORD dwAllowedItems = 0;
	int nTabCount = pActiveTabWnd->GetTabsNum ();

	if (nTabCount > 1)
	{
		if (m_lstTabbedGroups.GetCount () > 1)
		{
			dwAllowedItems = (m_groupAlignment == GROUP_VERT_ALIGN) ? 
										 BCGP_MDI_CREATE_VERT_GROUP : 
										 BCGP_MDI_CREATE_HORZ_GROUP;
		}
		else
		{
			dwAllowedItems = BCGP_MDI_CREATE_VERT_GROUP | BCGP_MDI_CREATE_HORZ_GROUP;
		}
	}

	if (pActiveTabWnd != m_lstTabbedGroups.GetHead ())
	{
		dwAllowedItems |= BCGP_MDI_CAN_MOVE_PREV;
	}

	if (pActiveTabWnd != m_lstTabbedGroups.GetTail ())
	{
		dwAllowedItems |= BCGP_MDI_CAN_MOVE_NEXT;
	}

	CBCGPMDIChildWnd* pMDIChildFrame = DYNAMIC_DOWNCAST (
		CBCGPMDIChildWnd, pActiveTabWnd->GetActiveWnd ());

	CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParentFrame ());
	if (pMDIChildFrame != NULL && pMDIChildFrame->IsTabbedControlBar () && pMainFrame != NULL && 
		!pMainFrame->IsFullScreen ())
	{
		dwAllowedItems |= BCGP_MDI_CAN_BE_DOCKED;
	}

	return dwAllowedItems;
}
//*************************************************************************************
CBCGPTabWnd* CBCGPMainClientAreaWnd::FindTabWndByChild(HWND hWndChild, int& iIndex)
{
	if (hWndChild == NULL)
		return NULL;

	for (POSITION pos = m_lstTabbedGroups.GetHeadPosition(); pos != NULL;)
	{
		CBCGPTabWnd* pNextTabWnd = DYNAMIC_DOWNCAST(CBCGPTabWnd, m_lstTabbedGroups.GetNext(pos));
		ASSERT_VALID(pNextTabWnd);
		
		iIndex = pNextTabWnd->GetTabFromHwnd(hWndChild);

		if (iIndex >= 0)
		{
			return pNextTabWnd;
		}
	}

	return NULL;
}
//*************************************************************************************
CBCGPTabWnd* CBCGPMainClientAreaWnd::FindActiveTabWndByActiveChild ()
{
	HWND hwndActive = (HWND) SendMessage (WM_MDIGETACTIVE, 0, 0);
	if (hwndActive == NULL)
	{
		return NULL;
	}

	for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextTabWnd);
		if (pNextTabWnd->GetTabFromHwnd (hwndActive) >= 0)
		{
			return pNextTabWnd;
		}
	}
	return NULL;
}
//*************************************************************************************
CBCGPTabWnd* CBCGPMainClientAreaWnd::FindActiveTabWnd ()
{
	for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextTabWnd);
		if (pNextTabWnd->IsActiveInMDITabGroup ())
		{
			return pNextTabWnd;
		}
	}
	return NULL;
}
//*************************************************************************************
CBCGPTabWnd* CBCGPMainClientAreaWnd::GetFirstTabWnd ()
{
	if (m_lstTabbedGroups.IsEmpty ())
	{
		return NULL;
	}

	return DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetHead ());
}
//*************************************************************************************
BOOL CBCGPMainClientAreaWnd::IsMemberOfMDITabGroup (CWnd* pWnd)
{
	if (!IsMDITabbedGroup ())
	{
		return FALSE;
	}

	return (m_lstTabbedGroups.Find (pWnd) != NULL);
}
//*************************************************************************************
CBCGPTabWnd* CBCGPMainClientAreaWnd::GetNextTabWnd (CBCGPTabWnd* pOrgTabWnd, BOOL bWithoutAsserts)
{
	POSITION pos = m_lstTabbedGroups.Find (pOrgTabWnd);

	if (pos == NULL)
	{
		if (!bWithoutAsserts)
		{
			ASSERT(FALSE);
			TRACE0("Trying to resize a member of tabbed group which is not in the list of groups.\n");
		}
		return NULL;
	}
	
	m_lstTabbedGroups.GetNext (pos);
	if (pos == NULL)
	{
		if (!bWithoutAsserts)
		{
			ASSERT(FALSE);
			TRACE0("Trying to resize a last member of tabbed group, which should not be resizable.\n");
		}
		return NULL;
	}

	CBCGPTabWnd* pNextTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetAt (pos));

	if (pNextTabWnd == NULL)
	{
		if (!bWithoutAsserts)
		{
			ASSERT(FALSE);
			TRACE0("Next member of tabbed group is NULL, or not a tab window.\n");
		}
		return NULL;
	}

	return pNextTabWnd;
}
//*************************************************************************************
CBCGPTabWnd* CBCGPMainClientAreaWnd::TabWndFromPoint (CPoint ptScreen)
{
	for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextTab = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextTab);
		CRect rectWnd;
		pNextTab->GetWindowRect (rectWnd);
		if (rectWnd.PtInRect (ptScreen))
		{
			return pNextTab;
		}
	}
	return NULL;
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::DrawNewGroupRect(LPCRECT lpRectNew)
{
	if (lpRectNew != NULL)
	{
		CBCGPMDIFrameWnd* pFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParent ());
		ASSERT_VALID(pFrame);

		pFrame->BringWindowToTop();

		if (g_hwdTopMDIClientArea != GetSafeHwnd())
		{
			CBCGPMainClientAreaWnd* pOldTopMDIClient = DYNAMIC_DOWNCAST(CBCGPMainClientAreaWnd, CWnd::FromHandlePermanent(g_hwdTopMDIClientArea));
			if (pOldTopMDIClient != NULL)
			{
				ASSERT_VALID(pOldTopMDIClient);
				pOldTopMDIClient->DrawNewGroupRect(NULL);
			}

			g_hwdTopMDIClientArea = GetSafeHwnd();
		}

		CRect rectNew = lpRectNew;

		if (m_pDragRectWnd == NULL)
		{
			m_pDragRectWnd = new CBCGPDragRectWnd;
			m_pDragRectWnd->Create(rectNew, this);
		}
		else
		{
			m_pDragRectWnd->SetWindowPos(NULL, rectNew.left, rectNew.top, rectNew.Width(), rectNew.Height(),
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}
	else if (m_pDragRectWnd != NULL)
	{
		m_pDragRectWnd->DestroyWindow();
		m_pDragRectWnd = NULL;
	}
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::MDITabMoveToNextGroup (BOOL bNext) 
{
	CBCGPTabWnd* pActiveWnd = FindActiveTabWndByActiveChild ();
	if (pActiveWnd == NULL)
	{
		return;
	}

	ASSERT_VALID (pActiveWnd);

	POSITION pos = m_lstTabbedGroups.Find (pActiveWnd);
	bNext ? m_lstTabbedGroups.GetNext (pos) : m_lstTabbedGroups.GetPrev (pos);

	if (pos == NULL)
	{
		return;
	}

	CBCGPTabWnd* pNextTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetAt (pos));
	ASSERT_VALID (pNextTabWnd);
	
	MoveWindowToTabGroup (pActiveWnd, pNextTabWnd);
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::MDITabNewGroup (BOOL bVert) 
{
	CBCGPTabWnd* pActiveWnd = FindActiveTabWndByActiveChild ();
	if (pActiveWnd == NULL)
	{
		return;
	}
	ASSERT_VALID (pActiveWnd);

	CRect rect;
	pActiveWnd->GetWindowRect (rect);

	if (bVert)
	{
		rect.left += rect.Width () / 2;
	}
	else
	{
		rect.top += rect.Height () / 2;
	}

	CBCGPTabWnd* pNewTabWnd = CreateNewTabGroup (pActiveWnd, rect, bVert); 
	MoveWindowToTabGroup (pActiveWnd, pNewTabWnd);
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::CloseAllWindows (CBCGPTabWnd* pTabWnd)
{
	if (pTabWnd != NULL)
	{
		ASSERT_VALID (pTabWnd);

		for (int i = pTabWnd->GetTabsNum () - 1; i >= 0; i--)
		{
			CBCGPMDIChildWnd* pNextWnd = DYNAMIC_DOWNCAST (CBCGPMDIChildWnd, pTabWnd->GetTabWnd (i));
			if (pNextWnd != NULL)
			{
				ASSERT_VALID (pNextWnd);
				pNextWnd->SendMessage (WM_CLOSE, (WPARAM) 0, (LPARAM) 0);
			}
		}
	}
	else
	{
		CObList lstWindows;
		CWnd* pWndChild = GetWindow (GW_CHILD);

		while (pWndChild != NULL)
		{
			ASSERT_VALID (pWndChild);

			CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pWndChild);
			if (pMDIChild != NULL)
			{
				ASSERT_VALID (pMDIChild);
				lstWindows.AddTail (pMDIChild);
			}

			pWndChild = pWndChild->GetNextWindow ();
		}

		for (POSITION pos = lstWindows.GetHeadPosition (); pos != NULL;)
		{
			CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST (CBCGPMDIChildWnd, 
															lstWindows.GetNext (pos));
			ASSERT_VALID (pMDIChild);
			pMDIChild->SendMessage (WM_CLOSE, (WPARAM) 0, (LPARAM) 0);
		}

	}
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::SerializeTabGroup (CArchive& ar, CBCGPTabWnd* pTabWnd, BOOL bSetRelation)
{
	ASSERT_VALID (pTabWnd);
	if (ar.IsStoring ())
	{
		int nTabsNum = pTabWnd->GetTabsNum ();
		if (pTabWnd->IsNewTabEnabled() && nTabsNum > 0 && pTabWnd->IsTabNew(nTabsNum - 1))
		{
			nTabsNum--;
		}

		ar << nTabsNum;

		int nActiveTab = pTabWnd->GetActiveTab ();
		ar << nActiveTab;
		ar << pTabWnd->IsActiveInMDITabGroup ();

		int i = 0;

		for (i = 0; i < nTabsNum; i++)
		{
			CBCGPMDIChildWnd* pNextWnd = DYNAMIC_DOWNCAST (CBCGPMDIChildWnd, pTabWnd->GetTabWnd (i));
			ASSERT_VALID (pNextWnd);

			CObject* pObject = NULL;
			CString strDocumentName = pNextWnd->GetDocumentName (&pObject);
			ar << strDocumentName;

			BOOL bObjPresent = (pObject != NULL);
			ar << bObjPresent;
			if (bObjPresent)
			{
				ar << pObject;
				m_lstSerializedToBeDeleted.AddTail(pObject);
			}

			CString strLabel; 
			pTabWnd->GetTabLabel (i, strLabel);
			ar << strLabel;

			ar << pTabWnd->GetResizeMode ();
			ar << pTabWnd->GetTabBkColor (i);

			int nBarID = -1;
			
			if (pNextWnd->IsTabbedControlBar ())
			{
				CBCGPDockingControlBar* pBar = pNextWnd->GetTabbedControlBar ();
				if (pBar != NULL && pBar->GetSafeHwnd () != NULL)
				{
					nBarID = pBar->GetDlgCtrlID ();
				}
			}

			ar << nBarID;
		}

		ar << pTabWnd->IsAutoColor ();

		const CArray<COLORREF, COLORREF>& arColors = pTabWnd->GetAutoColors (); 
		ar << (int) arColors.GetSize ();

		for (i = 0; i < arColors.GetSize (); i++)
		{
			ar << arColors [i];
		}

		ar << pTabWnd->IsTabDocumentsMenu ();
		ar << pTabWnd->IsTabSwapEnabled ();
		ar << pTabWnd->GetTabBorderSize ();

		CRect rectWindow;
		pTabWnd->GetWindowRect (rectWindow);
		ar << rectWindow;
	}
	else
	{
		int nTabsNum = 0;
		ar >> nTabsNum;

		int nActiveTab = -1;
		ar >> nActiveTab;

		BOOL bIsActiveInMDITabGroup = FALSE;
		ar >> bIsActiveInMDITabGroup;

		if (bIsActiveInMDITabGroup)
		{
			CBCGPTabWnd* pPrevActiveWnd = FindActiveTabWnd ();
			if (pPrevActiveWnd != NULL)
			{
				pPrevActiveWnd->SetActiveInMDITabGroup (FALSE);
				pPrevActiveWnd->InvalidateTab (pPrevActiveWnd->GetActiveTab ());
			}
		}
		pTabWnd->SetActiveInMDITabGroup (bIsActiveInMDITabGroup);

		CBCGPMDIFrameWnd* pFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParent ());
		ASSERT_VALID (pFrame);

		int i = 0;

		for (i = 0; i < nTabsNum; i++)
		{
			CString strDocumentName;
			ar >> strDocumentName;

			BOOL bObjectPresent = FALSE;
			CObject* pObj = NULL;

			ar >> bObjectPresent;
			if (bObjectPresent)
			{
				ar >> pObj;
			}

			CString strLabel; 
			ar >> strLabel;

			int nValue;
			ar >> nValue;
			pTabWnd->SetResizeMode ((CBCGPTabWnd::ResizeMode) nValue);

			COLORREF clrTab = (COLORREF) -1;
			ar >> clrTab;

			int nBarID = -1;
			if (g_pWorkspace != NULL && g_pWorkspace->GetDataVersion () >= 0x90200)
			{
				ar >> nBarID; 
			}

			CBCGPMDIChildWnd* pNextWnd = NULL;

			if (!strDocumentName.IsEmpty ())
			{
				if (m_lstLoadedTabDocuments.Find (strDocumentName) == NULL)
				{
					pNextWnd = pFrame->CreateDocumentWindow (strDocumentName, pObj);
					m_lstLoadedTabDocuments.AddTail (strDocumentName);
				}
				else
				{
					pNextWnd = pFrame->CreateNewWindow (strDocumentName, pObj);
				}
			}
			else if (nBarID != -1)
			{
				CBCGPDockingControlBar* pBar = DYNAMIC_DOWNCAST (CBCGPDockingControlBar, pFrame->GetControlBar (nBarID));
				if (pBar != NULL)
				{
					CBCGPBaseTabbedBar* pTabbedBar = pBar->GetParentTabbedBar ();
					if (pTabbedBar != NULL)
					{
						pBar->StoreRecentTabRelatedInfo ();
					}
					pNextWnd = pFrame->ControlBarToTabbedDocument (pBar);
					if (pTabbedBar != NULL)
					{
						pTabbedBar->RemoveControlBar (pBar);
					}
				}
			}

			if (pNextWnd != NULL)
			{
				ASSERT_VALID (pNextWnd);
				pTabWnd->AddTab (pNextWnd, strLabel);
				pTabWnd->SetTabBkColor (i, clrTab);
				if (bSetRelation)
				{
					pNextWnd->SetRelatedTabGroup (pTabWnd);
				}
				
			}
			
			if (pObj != NULL)
			{
				delete pObj;
			}
		}

		BOOL bIsAutoColor = FALSE;
		ar >> bIsAutoColor;

		int nAutoColorSize = 0;
		ar >> nAutoColorSize;

		CArray<COLORREF, COLORREF> arColors;
		
		for (i = 0; i < nAutoColorSize; i++)
		{
			COLORREF clr = (COLORREF) -1;
			ar >> clr;
			arColors.SetAtGrow (i, clr);
		}

		pTabWnd->EnableAutoColor (bIsAutoColor);
		pTabWnd->SetAutoColors (arColors);
		m_mdiTabParams.m_bAutoColor = bIsAutoColor;

		BOOL bValue = FALSE;

		ar >> bValue; 
		pTabWnd->EnableTabDocumentsMenu (bValue);
		m_mdiTabParams.m_bDocumentMenu = bValue;

		ar >> bValue;
		pTabWnd->EnableTabSwap (bValue);
		m_mdiTabParams.m_bEnableTabSwap = bValue;

		int nTabBorderSize = 1;
		ar >> nTabBorderSize; 
		pTabWnd->SetTabBorderSize (nTabBorderSize);
		m_mdiTabParams.m_nTabBorderSize = nTabBorderSize;

		CRect rectWindow;
		ar >> rectWindow;

		pTabWnd->GetParent ()->ScreenToClient (rectWindow);

		pTabWnd->SetWindowPos (NULL, rectWindow.left, rectWindow.right, 
									 rectWindow.Width (), rectWindow.Height (), 
									 SWP_NOZORDER | SWP_NOACTIVATE);

		if (pTabWnd->GetTabsNum () > 0)
		{
			if (nActiveTab > pTabWnd->GetTabsNum () - 1)
			{
				nActiveTab = pTabWnd->GetTabsNum () - 1;
			}

			pTabWnd->SetActiveTab (nActiveTab);
		}
	}
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::SerializeOpenChildren (CArchive& ar)
{
	if (ar.IsStoring ())
	{
		CObList lstWindows;

		CWnd* pWndChild = GetWindow (GW_CHILD);
		while (pWndChild != NULL)
		{
			ASSERT_VALID (pWndChild);

			CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pWndChild);
			if (pMDIChild != NULL)
			{
				ASSERT_VALID (pWndChild);

				CObject* pObj = NULL;
				CString str = pMDIChild->GetDocumentName (&pObj);
				if (pObj != NULL)
				{
					delete pObj;
				}
				if (!str.IsEmpty ())
				{
					lstWindows.AddHead (pMDIChild);
				}
			}

			pWndChild = pWndChild->GetNextWindow ();
		}

		HWND hwndActive = (HWND) SendMessage (WM_MDIGETACTIVE);

		ar << (int) lstWindows.GetCount ();

		for (POSITION pos = lstWindows.GetHeadPosition (); pos != NULL;)
		{
			CBCGPMDIChildWnd* pMDIChild = DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, 
															lstWindows.GetNext (pos));
			
			ASSERT_VALID (pMDIChild);

			CObject* pObj = NULL;
			CString str = pMDIChild->GetDocumentName (&pObj);
			ar << str;

			BOOL bObjPresent = (pObj != NULL);
			ar << bObjPresent;
			if (bObjPresent)
			{
				ar << pObj;
				m_lstSerializedToBeDeleted.AddTail(pObj);
			}

			WINDOWPLACEMENT wp;
			pMDIChild->GetWindowPlacement (&wp);

			ar << wp.flags;
			ar << wp.length;
			ar << wp.ptMaxPosition;
			ar << wp.ptMinPosition;
			ar << wp.rcNormalPosition;
			ar << wp.showCmd;

			BOOL bActive = (pMDIChild->GetSafeHwnd () == hwndActive);
			ar << bActive;

			int nBarID = -1;
			
			if (pMDIChild->IsTabbedControlBar ())
			{
				CBCGPDockingControlBar* pBar = pMDIChild->GetTabbedControlBar ();
				if (pBar != NULL && pBar->GetSafeHwnd () != NULL)
				{
					nBarID = pBar->GetDlgCtrlID ();
				}
			}

			ar << nBarID;
		}
	}
	else
	{
		CBCGPMDIFrameWnd* pFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParent ());
		ASSERT_VALID (pFrame);


		int nCount = 0;
		ar >> nCount;

		HWND hwndActive = NULL;
		BOOL bMaximize = FALSE;
		for (int i = 0; i < nCount; i++)
		{
			CString strDocName;
			ar >> strDocName;

			BOOL bObjPresent = FALSE;
			CObject* pObj = NULL;

			ar >> bObjPresent;
			if (bObjPresent)
			{
				ar >> pObj;
			}

			WINDOWPLACEMENT wp;
			ar >> wp.flags;
			ar >> wp.length;
			ar >> wp.ptMaxPosition;
			ar >> wp.ptMinPosition;
			ar >> wp.rcNormalPosition;
			ar >> wp.showCmd;

			BOOL bActive = FALSE;
			ar >> bActive;

			int nBarID = -1;
			if (g_pWorkspace != NULL && g_pWorkspace->GetDataVersion () >= 0x90200)
			{
				ar >> nBarID; 
			}


			if (bMaximize)
			{
				wp.showCmd = SW_SHOWMAXIMIZED;
			}

			CBCGPMDIChildWnd* pNextWnd = NULL;



			if (!strDocName.IsEmpty () && nBarID == -1)
			{
				pNextWnd = pFrame->CreateNewWindow (strDocName, pObj);
			}
			else if (nBarID != -1)
			{

				CBCGPDockingControlBar* pBar = DYNAMIC_DOWNCAST (CBCGPDockingControlBar, pFrame->GetControlBar (nBarID));
				if (pBar != NULL)
				{
					CBCGPBaseTabbedBar* pTabbedBar = pBar->GetParentTabbedBar ();
					if (pTabbedBar != NULL)
					{
						pBar->StoreRecentTabRelatedInfo ();
					}
					pNextWnd = pFrame->ControlBarToTabbedDocument (pBar);
					if (pTabbedBar != NULL)
					{
						pTabbedBar->RemoveControlBar (pBar);
					}
				}
			}

			if (pObj != NULL)
			{
				delete pObj;
			}
			if (pNextWnd != NULL)
			{
				ASSERT_VALID (pNextWnd);

				pNextWnd->SetWindowPlacement (&wp);	
				if (wp.showCmd == SW_SHOWMAXIMIZED)
				{
					ShowWindow (wp.showCmd);
					bMaximize = TRUE;
				}

				if (bActive)
				{
					hwndActive = pNextWnd->GetSafeHwnd ();
				}
			}
		}

		if (hwndActive != NULL)
		{
			SendMessage (WM_MDIACTIVATE, (WPARAM) hwndActive);
		}
	}
	
}
//*************************************************************************************
BOOL CBCGPMainClientAreaWnd::SaveState (LPCTSTR lpszProfileName, UINT nFrameID, BOOL bSaveFramePosition)
{
	BOOL bResult = FALSE;
	CString strProfileName = ::BCGPGetRegPath (strMDIClientAreaProfile, lpszProfileName);

	CString strSection;
	strSection.Format (m_strRegSectionFmt, strProfileName, nFrameID);
	
	try
	{
		CMemFile file;
		{
			CArchive ar (&file, CArchive::store);

			Serialize (ar);
			ar.Flush ();
		}

		UINT uiDataSize = (UINT) file.GetLength ();
		LPBYTE lpbData = file.Detach ();

		if (lpbData != NULL)
		{
			CBCGPRegistrySP regSP;
			CBCGPRegistry& reg = regSP.Create (FALSE, FALSE);

			if (reg.CreateKey (strSection))
			{
				bResult = reg.Write (REG_ENTRY_MDITABS_STATE, lpbData, uiDataSize);

				if (bResult && bSaveFramePosition)
				{
					WINDOWPLACEMENT wp;
					wp.length = sizeof (WINDOWPLACEMENT);
					
					if (GetParent()->GetWindowPlacement (&wp))
					{
						//---------------------------
						// Make sure we don't pop up 
						// minimized the next time
						//---------------------------
						if (wp.showCmd != SW_SHOWMAXIMIZED)
						{
							wp.showCmd = SW_SHOWNORMAL;
						}
						
						RECT rectDesktop;
						SystemParametersInfo(SPI_GETWORKAREA,0,(PVOID)&rectDesktop,0);
						OffsetRect(&wp.rcNormalPosition, rectDesktop.left, rectDesktop.top);
						
						reg.Write(REG_ENTRY_MDI_FRAME_RECT, &wp.rcNormalPosition);
						reg.Write(REG_ENTRY_MDI_FRAME_FLAGS, (int)wp.flags);
						reg.Write(REG_ENTRY_MDI_FRAME_SHOW_CMD, (int)wp.showCmd);
					}
				}
			}

			free (lpbData);
		}
	}
	catch (CMemoryException* pEx)
	{
		pEx->Delete ();
		TRACE(_T ("Memory exception in CBCGPMainClientAreaWnd::SaveState ()!\n"));
	}
	catch (CArchiveException* pEx)
	{
		pEx->Delete ();
		TRACE(_T ("CArchiveException exception in CBCGPMainClientAreaWnd::SaveState ()!\n"));
	}
	catch (...)
	{
		TRACE(_T ("Unknown exception in CBCGPMainClientAreaWnd::SaveState ()!\n"));
	}


	return bResult;
}
//*************************************************************************************
BOOL CBCGPMainClientAreaWnd::LoadState (LPCTSTR lpszProfileName, UINT nFrameID, BOOL bRestoreFramePosition)
{
	BOOL bResult = FALSE;

	CString strProfileName = ::BCGPGetRegPath (strMDIClientAreaProfile, lpszProfileName);

	CString strSection;
	strSection.Format (m_strRegSectionFmt, strProfileName, nFrameID);

	LPBYTE	lpbData = NULL;
	UINT	uiDataSize;

	CBCGPRegistrySP regSP;
	CBCGPRegistry& reg = regSP.Create (FALSE, TRUE);

	if (!reg.Open (strSection))
	{
		return FALSE;
	}

	if (!reg.Read (REG_ENTRY_MDITABS_STATE, &lpbData, &uiDataSize))
	{
		return FALSE;
	}

	if (bRestoreFramePosition)
	{
		CWnd* pFrameWnd = GetParent();

		CRect rectNormal;
		int nFlags = 0;
		int nShowCmd = SW_SHOWNORMAL;
		
		if (reg.Read (REG_ENTRY_MDI_FRAME_RECT, rectNormal) &&
			reg.Read (REG_ENTRY_MDI_FRAME_FLAGS, nFlags) &&
			reg.Read (REG_ENTRY_MDI_FRAME_SHOW_CMD, nShowCmd))
		{
			WINDOWPLACEMENT wp;
			wp.length = sizeof (WINDOWPLACEMENT);
			
			if (pFrameWnd->GetWindowPlacement (&wp))
			{
				wp.rcNormalPosition = rectNormal;
				wp.showCmd = nShowCmd;
				
				RECT rectDesktop;
				SystemParametersInfo(SPI_GETWORKAREA,0,(PVOID)&rectDesktop,0);
				OffsetRect(&wp.rcNormalPosition, -rectDesktop.left, -rectDesktop.top);
				
				pFrameWnd->SetWindowPlacement (&wp);
			}
		}
	}

	try
	{
		CMemFile file (lpbData, uiDataSize);
		CArchive ar (&file, CArchive::load);

		Serialize (ar);
		bResult = TRUE;
	}
	catch (CMemoryException* pEx)
	{
		pEx->Delete ();
		TRACE(_T ("Memory exception in CBCGPMainClientAreaWnd::LoadState!\n"));
	}
	catch (CArchiveException* pEx)
	{
		pEx->Delete ();
		TRACE(_T ("CArchiveException exception in CBCGPMainClientAreaWnd::LoadState ()!\n"));
	}
	catch (...)
	{
		TRACE(_T ("Unknown exception in CBCGPMainClientAreaWnd::LoadState ()!\n"));
	}

	if (lpbData != NULL)
	{
		delete [] lpbData;
	}

	if (!bResult)
	{
		m_bDisableUpdateTabs = FALSE;
		CloseAllWindows (NULL);
	}

	return bResult;
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::Serialize (CArchive& ar)
{
	m_mdiTabParams.Serialize (ar);

	if (ar.IsStoring ())
	{
		ar << m_bTabIsEnabled;
		ar << m_bIsMDITabbedGroup;

		ar << m_bTabIsVisible;
		ar << m_groupAlignment;
		ar << m_nResizeMargin;
		ar << m_nNewGroupMargin;

		if (m_bTabIsEnabled)
		{
			SerializeTabGroup (ar, &m_wndTab);
		}
		else if (m_bIsMDITabbedGroup)
		{
			int nCountTabbedGroups = (int) m_lstTabbedGroups.GetCount ();
			ar << nCountTabbedGroups;

			if (nCountTabbedGroups > 0)
			{
				for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
				{
					CBCGPTabWnd* pNextWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
					SerializeTabGroup (ar, pNextWnd);
				}
			}
		}
		else
		{
			SerializeOpenChildren (ar);
		}

		while (!m_lstSerializedToBeDeleted.IsEmpty())
		{
			delete m_lstSerializedToBeDeleted.RemoveHead();
		}
	}
	else if (ar.IsLoading ())
	{
		CloseAllWindows (NULL);
		m_lstLoadedTabDocuments.RemoveAll ();

		m_bDisableUpdateTabs = TRUE;

		ar >> m_bTabIsEnabled;
		ar >> m_bIsMDITabbedGroup;
		ar >> m_bTabIsVisible;

		int nValue;		
		ar >> nValue; 
		m_groupAlignment = (GROUP_ALIGNMENT) nValue;

		ar >> m_nResizeMargin;
		ar >> m_nNewGroupMargin;

		if (m_bTabIsEnabled)
		{
			SerializeTabGroup (ar, &m_wndTab);
			EnableMDITabs (TRUE, m_mdiTabParams);
		}
		else if (m_bIsMDITabbedGroup)
		{
			int nCountTabbedGroups = 0;
			ar >> nCountTabbedGroups;

			for (int i = 0; i < nCountTabbedGroups; i++)
			{
				CBCGPTabWnd* pNewTabWnd = CreateTabGroup (NULL);
				ASSERT_VALID (pNewTabWnd);
				SerializeTabGroup (ar, pNewTabWnd, TRUE);

				if (pNewTabWnd->GetTabsNum () == 0)
				{
					pNewTabWnd->DestroyWindow ();
					delete pNewTabWnd;
				}
				else
				{
					m_lstTabbedGroups.AddTail (pNewTabWnd);
				}
			}

			// sanity check for resize mode - the last group might have been removed
			// because the document could not be opened
			if (m_lstTabbedGroups.GetCount () > 0)
			{
				CBCGPTabWnd* pLastTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetTail ());
				ASSERT_VALID (pLastTabWnd);
				pLastTabWnd->SetResizeMode (CBCGPTabWnd::RESIZE_NO);
			}
		
			EnableMDITabbedGroups (TRUE, m_mdiTabParams);
		}
		else
		{
			SerializeOpenChildren (ar);
		}

		m_bDisableUpdateTabs = FALSE;

		if (m_bIsMDITabbedGroup)
		{
			UpdateMDITabbedGroups (TRUE);
			for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
			{
				CBCGPTabWnd* pNextWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
				ASSERT_VALID (pNextWnd);
				pNextWnd->RecalcLayout ();

				if (pNextWnd->IsActiveInMDITabGroup ())
				{
					CWnd* pWnd = pNextWnd->GetTabWnd (pNextWnd->GetActiveTab ());
					PostMessage (WM_MDIACTIVATE, (WPARAM) pWnd->GetSafeHwnd ());
				}
			}
		}
		else if (m_bTabIsEnabled)
		{
			UpdateTabs (TRUE);
			m_wndTab.RecalcLayout ();
		}

		((CFrameWnd*) GetParent ())->RecalcLayout ();
	}
}
//*************************************************************************************
CWnd* CBCGPMainClientAreaWnd::GetNextMDITab(BOOL bPrev)
{
	if (!m_bIsMDITabbedGroup && !m_bTabIsEnabled)
	{
		return NULL;
	}
	
	CBCGPTabWnd* pActiveTabWnd = NULL;
	int nActiveTab = -1;
	
	if (m_bIsMDITabbedGroup)
	{
		pActiveTabWnd = FindActiveTabWnd ();
	}
	else
	{
		pActiveTabWnd = &m_wndTab;
	}
	
	if (pActiveTabWnd->GetSafeHwnd() == NULL)
	{
		return NULL;
	}
	
	ASSERT_VALID (pActiveTabWnd);
	
	POSITION posActive = m_bIsMDITabbedGroup ? m_lstTabbedGroups.Find (pActiveTabWnd) : NULL;
	int nGroupCount = m_bIsMDITabbedGroup ? (int) m_lstTabbedGroups.GetCount () : 0;
	
	if (m_bIsMDITabbedGroup)
	{
		ASSERT (posActive != NULL);
	}
	
	nActiveTab = pActiveTabWnd->GetActiveTab ();
	
	bPrev ? nActiveTab-- : nActiveTab++;
	
	if (nActiveTab < 0)
	{
		if (nGroupCount > 0)
		{
			m_lstTabbedGroups.GetPrev (posActive);
			if (posActive != NULL)
			{
				pActiveTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetAt (posActive));
			}
			else
			{
				pActiveTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetTail ());
			}
		}
		
		ASSERT (pActiveTabWnd != NULL);
		
		nActiveTab = pActiveTabWnd->GetTabsNum () - 1;
	}
	
	if (nActiveTab >= pActiveTabWnd->GetTabsNum ())
	{
		if (nGroupCount > 0)
		{
			pActiveTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (posActive));
			if (posActive != NULL)
			{
				pActiveTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetAt (posActive));
			}
			else
			{
				pActiveTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetHead ());
			}
		}
		
		ASSERT (pActiveTabWnd != NULL);
		
		nActiveTab = 0;
	}
	
	
	return pActiveTabWnd->GetTabWnd (nActiveTab);
}
//*************************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnMDINext(WPARAM wp, LPARAM lp)
{
	if (!m_bIsMDITabbedGroup && !m_bTabIsEnabled)
	{
		return Default ();
	}

	CWnd* pWnd = GetNextMDITab(lp != 0);
	if (pWnd->GetSafeHwnd() == NULL)
	{
		return 0L;
	}

	ASSERT_VALID(pWnd);

	if (pWnd->GetSafeHwnd () != (HWND) wp)
	{
		SetActiveTab (pWnd->GetSafeHwnd ());
	}	
	return 0L;
}
//*************************************************************************************
LRESULT CBCGPMainClientAreaWnd::OnIdleUpdateCmdUI(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	if (!m_bIsMDITabbedGroup && m_wndTab.GetSafeHwnd() == NULL)
	{
		return Default();
	}

	CWnd* pFocus = GetFocus();
	BOOL bActiveOld = m_bActive;

	if (CBCGPMDIFrameWnd::m_hwndLastActiveDetachedMDIFrame != NULL)
	{
		m_bActive = (GetParent()->GetSafeHwnd() == CBCGPMDIFrameWnd::m_hwndLastActiveDetachedMDIFrame);
	}
	else
	{
		m_bActive = (pFocus->GetSafeHwnd () != NULL && (IsChild (pFocus) || pFocus->GetSafeHwnd () == GetSafeHwnd ()));
	}

	if (m_bActive != bActiveOld)
	{
		for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
		{
			CBCGPTabWnd* pNextTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
			ASSERT_VALID (pNextTabWnd);

			pNextTabWnd->SetMDIFocused(m_bActive);
		}

		if (m_wndTab.GetSafeHwnd() != NULL)
		{
			m_wndTab.SetMDIFocused(m_bActive);
		}
	}

	return Default();
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::EnableMDITabsLastActiveActivation(BOOL bLastActiveTab)
{
	if (m_bLastActiveTab == bLastActiveTab)
	{
		return;
	}

	m_bLastActiveTab = bLastActiveTab;

	m_wndTab.EnableActivateLastActive(bLastActiveTab);

	for (POSITION pos = m_lstTabbedGroups.GetHeadPosition (); pos != NULL;)
	{
		CBCGPTabWnd* pNextTabWnd = DYNAMIC_DOWNCAST (CBCGPTabWnd, m_lstTabbedGroups.GetNext (pos));
		ASSERT_VALID (pNextTabWnd);

		pNextTabWnd->EnableActivateLastActive(bLastActiveTab);
	}
}
//*************************************************************************************
void CBCGPMainClientAreaWnd::OnDragForeignMDIChild(CBCGPMDIChildWnd* pMDIChildWnd)
{
	CBCGPMDIFrameWnd* pMainFrame = DYNAMIC_DOWNCAST (CBCGPMDIFrameWnd, GetParentFrame ());
	if (pMainFrame != NULL && ::IsWindow (pMainFrame->GetSafeHwnd ()) && pMainFrame->IsRibbonBackstageView())
	{
		DrawNewGroupRect(NULL);
		return;
	}

	if (pMDIChildWnd == NULL)
	{
		DrawNewGroupRect(NULL);
		return;
	}

	if (globalData.m_hcurMoveTab == NULL)
	{
		CBCGPLocalResource locaRes;

		globalData.m_hcurMoveTab = AfxGetApp ()->LoadCursor (IDC_BCGBARRES_MOVE_TAB);
		globalData.m_hcurNoMoveTab = AfxGetApp ()->LoadCursor (IDC_BCGBARRES_NO_MOVE_TAB);
	}

	CRect rectScreenHoveredWnd;
	GetClientRect(rectScreenHoveredWnd);
	ClientToScreen(&rectScreenHoveredWnd);

	DrawNewGroupRect(rectScreenHoveredWnd);
	m_rectNewTabGroup = rectScreenHoveredWnd;

	::SetCursor (globalData.m_hcurMoveTab);
}
//*************************************************************************************
BOOL CBCGPMainClientAreaWnd::IsDragMDIChild() const
{
	return g_MDIChildDragWnd != NULL;
}
//************************************************************************
BOOL CBCGPMainClientAreaWnd::IsInternalScrollBarThemed() const
{
	return (globalData.m_nThemedScrollBars & BCGP_THEMED_SCROLLBAR_MDICLIENT) != 0;
}
//************************************************************************
void CBCGPMainClientAreaWnd::OnNcPaint()
{
	if ((GetStyle() & WS_BORDER) || (GetExStyle() & WS_EX_CLIENTEDGE))
	{
		CWindowDC dc (this);

		CRect rectWindow;
		GetWindowRect(&rectWindow);

		if (rectWindow.Width() > 0 && rectWindow.Height() > 0)
		{
			CPoint ptOffset(-rectWindow.TopLeft());

			CRect rectClient;
			GetClientRect(&rectClient);
			ClientToScreen(&rectClient);
			rectClient.OffsetRect(ptOffset);

			dc.ExcludeClipRect(rectClient);

			rectWindow.OffsetRect(ptOffset);

			CBCGPVisualManager::GetInstance()->OnDrawControlBorder (&dc, rectWindow, this, CBCGPToolBarImages::m_bIsDrawOnGlass);

			dc.SelectClipRgn(NULL);
		}
	}
	else
	{
		Default();
	}
}
//*************************************************************************************
BOOL CBCGPMainClientAreaWnd::OnNcActivate(BOOL /*bActive*/)
{
	BOOL bRet = (BOOL)Default();
	
	SendMessage(WM_NCPAINT);
	return bRet;
}

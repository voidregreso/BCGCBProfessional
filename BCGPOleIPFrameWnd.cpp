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

// BCGPOleIPFrameWnd.cpp : implementation file
//

#include "stdafx.h"
#include "BCGCBPro.h"
#include "BCGPOleIPFrameWnd.h"
#include "BCGPMenuBar.h"
#include "BCGPMiniFrameWnd.h"
#include "BCGPPopupMenu.h"
#include "BCGPUserToolsManager.h"
#include "BCGPPrintPreviewView.h"
#include "BCGPOleCntrFrameWnd.h"
#include "BCGPIntelliSenseWnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBCGPOleIPFrameWnd

IMPLEMENT_DYNCREATE(CBCGPOleIPFrameWnd, COleIPFrameWnd)

#pragma warning (disable : 4355)

CBCGPOleIPFrameWnd::CBCGPOleIPFrameWnd() :
	m_Impl (this),
	m_bContextHelp (FALSE),
	m_hwndLastTopLevelFrame (NULL)
{
	m_dockManager.Create (this);
}

#pragma warning (default : 4355)

CBCGPOleIPFrameWnd::~CBCGPOleIPFrameWnd()
{
}


BEGIN_MESSAGE_MAP(CBCGPOleIPFrameWnd, COleIPFrameWnd)
	//{{AFX_MSG_MAP(CBCGPOleIPFrameWnd)
	ON_WM_MENUCHAR()
	ON_WM_ACTIVATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
	ON_REGISTERED_MESSAGE(BCGM_CREATETOOLBAR, OnToolbarCreateNew)
	ON_REGISTERED_MESSAGE(BCGM_DELETETOOLBAR, OnToolbarDelete)
	ON_UPDATE_COMMAND_UI(ID_VIEW_STATUS_BAR, OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_STATUS_BAR, OnBarCheck)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR, OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_TOOLBAR, OnBarCheck)
	ON_UPDATE_COMMAND_UI(ID_VIEW_REBAR, OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_REBAR, OnBarCheck)
	ON_REGISTERED_MESSAGE(BCGM_TOOLBARMENU, OnToolbarContextMenu)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CBCGPOleIPFrameWnd message handlers

LRESULT CBCGPOleIPFrameWnd::OnMenuChar(UINT nChar, UINT nFlags, CMenu* pMenu) 
{
	if (m_Impl.OnMenuChar (nChar))
	{
		return MAKELPARAM (MNC_EXECUTE, -1);
	}
		
	return CFrameWnd::OnMenuChar(nChar, nFlags, pMenu);
}
//*******************************************************************************************
afx_msg LRESULT CBCGPOleIPFrameWnd::OnSetMenu (WPARAM wp, LPARAM lp)
{
	OnSetMenu ((HMENU) wp);
	return DefWindowProc (WM_MDISETMENU, NULL, lp);
}
//*******************************************************************************************
BOOL CBCGPOleIPFrameWnd::OnSetMenu (HMENU hmenu)
{
	if (m_Impl.m_pMenuBar != NULL)
	{
		m_Impl.m_pMenuBar->CreateFromMenu 
			(hmenu == NULL ? m_Impl.m_hDefaultMenu : hmenu);
		return TRUE;
	}

	return FALSE;
}
//*******************************************************************************************
BOOL CBCGPOleIPFrameWnd::PreTranslateMessage(MSG* pMsg) 
{
	switch (pMsg->message)
	{
	case WM_KEYDOWN:
		if (!CBCGPFrameImpl::IsHelpKey (pMsg) && 
			m_Impl.ProcessKeyboard ((int) pMsg->wParam))
		{
			return TRUE;
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
		{
			CPoint pt (BCG_GET_X_LPARAM(pMsg->lParam), BCG_GET_Y_LPARAM(pMsg->lParam));
			CWnd* pWnd = CWnd::FromHandle(pMsg->hwnd);

			if (pWnd != NULL && ::IsWindow (pMsg->hwnd))
			{
				pWnd->ClientToScreen (&pt);
			}

			if (m_Impl.ProcessMouseClick (pMsg->message, pt, pMsg->hwnd))
			{
				return TRUE;
			}

			if (!::IsWindow (pMsg->hwnd))
			{
				return TRUE;
			}
		}
		break;

	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
		if (m_Impl.ProcessMouseClick (pMsg->message,
			CPoint (BCG_GET_X_LPARAM(pMsg->lParam), BCG_GET_Y_LPARAM(pMsg->lParam)),
			pMsg->hwnd))
		{
			return TRUE;
		}
		break;

	case WM_MOUSEMOVE:
		{
			CPoint pt (BCG_GET_X_LPARAM(pMsg->lParam), BCG_GET_Y_LPARAM(pMsg->lParam));
			CWnd* pWnd = CWnd::FromHandle(pMsg->hwnd);

			if (pWnd != NULL)
			{
				pWnd->ClientToScreen (&pt);
			}

			if (m_Impl.ProcessMouseMove (pt))
			{
				return TRUE;
			}
		}
	}

	return CFrameWnd::PreTranslateMessage(pMsg);
}
//*******************************************************************************************
BOOL CBCGPOleIPFrameWnd::ShowPopupMenu (CBCGPPopupMenu* pMenuPopup)
{
	if (!m_Impl.OnShowPopupMenu (pMenuPopup, this))
	{
		return FALSE;
	}

	if (pMenuPopup != NULL && pMenuPopup->m_bShown)
	{
		return TRUE;
	}

	return OnShowPopupMenu (pMenuPopup);
}
//*******************************************************************************************
void CBCGPOleIPFrameWnd::OnClosePopupMenu (CBCGPPopupMenu* pMenuPopup)
{
	if (CBCGPPopupMenu::m_pActivePopupMenu == pMenuPopup)
	{
		CBCGPPopupMenu::m_pActivePopupMenu = NULL;
	}

	m_dockManager.OnClosePopupMenu ();
}
//*******************************************************************************************
BOOL CBCGPOleIPFrameWnd::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	if (HIWORD (wParam) == 1 && lParam == 0)
	{
		UINT uiCmd = LOWORD (wParam);

		CBCGPToolBar::AddCommandUsage (uiCmd);

#ifndef BCGP_EXCLUDE_EDIT_CTRL
		if (CBCGPIntelliSenseWnd::m_hwndActive != NULL && ::IsWindow(CBCGPIntelliSenseWnd::m_hwndActive))
		{
			::SendMessage(CBCGPIntelliSenseWnd::m_hwndActive, WM_CLOSE, 0, 0);
		}
#endif
		//---------------------------
		// Simmulate ESC keystroke...
		//---------------------------
		if (m_Impl.ProcessKeyboard (VK_ESCAPE))
		{
			return TRUE;
		}

		if (g_pUserToolsManager != NULL &&
			g_pUserToolsManager->InvokeTool (uiCmd))
		{
			return TRUE;
		}
	}

	if (!CBCGPToolBar::IsCustomizeMode ())
	{
		return CFrameWnd::OnCommand(wParam, lParam);
	}

	return FALSE;
}
//******************************************************************
BOOL CBCGPOleIPFrameWnd::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext) 
{
	m_Impl.m_nIDDefaultResource = nIDResource;
	m_Impl.LoadLargeIconsState ();

	return CFrameWnd::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext);
}
//***************************************************************************************
#if _MSC_VER >= 1300
void CBCGPOleIPFrameWnd::HtmlHelp(DWORD_PTR dwData, UINT nCmd)
{
	if (dwData > 0 || !m_bContextHelp)
	{
		CFrameWnd::HtmlHelp(dwData, nCmd);
	}
	else
	{
		OnContextHelp ();
	}
}
#endif
//***************************************************************************************
void CBCGPOleIPFrameWnd::WinHelp(DWORD_PTR dwData, UINT nCmd) 
{
	if (dwData > 0 || !m_bContextHelp)
	{
		CFrameWnd::WinHelp(dwData, nCmd);
	}
	else
	{
		OnContextHelp ();
	}
}
//***************************************************************************************
void CBCGPOleIPFrameWnd::OnContextHelp ()
{
	m_bContextHelp = TRUE;

	if (!m_bHelpMode && CanEnterHelpMode())
	{
		CBCGPToolBar::SetHelpMode ();
	}

	CFrameWnd::OnContextHelp ();

	if (!m_bHelpMode)
	{
		CBCGPToolBar::SetHelpMode (FALSE);
	}

	m_bContextHelp = FALSE;
}
//****************************************************************************************
LRESULT CBCGPOleIPFrameWnd::OnToolbarCreateNew(WPARAM,LPARAM lp)
{
	ASSERT (lp != NULL);
	return (LRESULT) m_Impl.CreateNewToolBar ((LPCTSTR) lp);
}
//***************************************************************************************
LRESULT CBCGPOleIPFrameWnd::OnToolbarDelete(WPARAM,LPARAM lp)
{
	CBCGPToolBar* pToolbar = (CBCGPToolBar*) lp;
	ASSERT_VALID (pToolbar);

	return (LRESULT) m_Impl.DeleteToolBar (pToolbar);
}
//***************************************************************************************
void CBCGPOleIPFrameWnd::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized) 
{
	COleIPFrameWnd::OnActivate(nState, pWndOther, bMinimized);
	
	switch (nState)
	{
	case WA_CLICKACTIVE:
		UpdateWindow ();
		break;

	case WA_INACTIVE:
		if (!CBCGPToolBar::IsCustomizeMode ())
		{
			m_Impl.DeactivateMenu ();
		}

		if (CBCGPPopupMenu::GetActiveMenu () != NULL)
		{
			CBCGPPopupMenu::GetActiveMenu ()->SendMessage (WM_CLOSE);
		}

		break;
	}

	if (nState == WA_INACTIVE)
	{
		if (g_pBCGPTopLevelFrame == this)
		{
			CFrameWnd* pTopLevelFrame = DYNAMIC_DOWNCAST (CFrameWnd,
				CWnd::FromHandlePermanent (m_hwndLastTopLevelFrame));

			BCGCBProSetTopLevelFrame (pTopLevelFrame);
		}
	}
	else
	{
		m_hwndLastTopLevelFrame = g_pBCGPTopLevelFrame->GetSafeHwnd ();
		g_pBCGPTopLevelFrame = this;
	}
}
//***************************************************************************************
void CBCGPOleIPFrameWnd::OnClose() 
{
	m_Impl.OnCloseFrame();
	COleIPFrameWnd::OnClose();
}
//***************************************************************************************
void CBCGPOleIPFrameWnd::OnDestroy() 
{
	if (CBCGPPopupMenu::GetActiveMenu () != NULL)
	{
		CBCGPPopupMenu::GetActiveMenu ()->SendMessage (WM_CLOSE);
	}

	if (g_pBCGPTopLevelFrame == this)
	{
		CFrameWnd* pTopLevelFrame = DYNAMIC_DOWNCAST (CFrameWnd,
			CWnd::FromHandlePermanent (m_hwndLastTopLevelFrame));

		g_pBCGPTopLevelFrame = pTopLevelFrame;
	}

	m_Impl.DeactivateMenu ();

    if (m_hAccelTable != NULL)
    {
        ::DestroyAcceleratorTable (m_hAccelTable);
        m_hAccelTable = NULL;
    }

	m_dockManager.m_bEnableAdjustLayout = FALSE;

	CList<HWND, HWND> lstChildren;
	CWnd* pNextWnd = GetTopWindow ();

	while (pNextWnd != NULL)
	{
		lstChildren.AddTail (pNextWnd->m_hWnd);
		pNextWnd = pNextWnd->GetNextWindow ();
	}

	for (POSITION pos = lstChildren.GetHeadPosition (); pos != NULL;)
	{
		HWND hwndNext = lstChildren.GetNext (pos);
		if (IsWindow (hwndNext) && ::GetParent (hwndNext) == m_hWnd)
		{
			::DestroyWindow (hwndNext);
		}
	}
	COleIPFrameWnd::OnDestroy();
}

//*****************************************************************************
//******************* dockmanager layer ***************************************
//*****************************************************************************
void CBCGPOleIPFrameWnd::AddDockBar ()
{
	ASSERT_VALID (this);
}
//*****************************************************************************
BOOL CBCGPOleIPFrameWnd::AddControlBar (CBCGPBaseControlBar* pControlBar, BOOL bTail)
{
	ASSERT_VALID (this);
	return m_dockManager.AddControlBar (pControlBar, bTail);
}
//*****************************************************************************
BOOL CBCGPOleIPFrameWnd::InsertControlBar (CBCGPBaseControlBar* pControlBar, 
									  CBCGPBaseControlBar* pTarget, BOOL bAfter)
{
	ASSERT_VALID (this);
	return m_dockManager.InsertControlBar (pControlBar, pTarget, bAfter);
}
//*****************************************************************************
void CBCGPOleIPFrameWnd::RemoveControlBarFromDockManager (CBCGPBaseControlBar* pControlBar, 
										   BOOL bDestroy, BOOL bAdjustLayout, 
										   BOOL bAutoHide, CBCGPBaseControlBar* pBarReplacement)
{
	ASSERT_VALID (this);
	m_dockManager.RemoveControlBarFromDockManager (pControlBar, bDestroy, bAdjustLayout, bAutoHide, pBarReplacement);
}
//*****************************************************************************
void CBCGPOleIPFrameWnd::DockControlBar (CBCGPBaseControlBar* pBar, UINT nDockBarID, 
									LPCRECT lpRect)
{
	ASSERT_VALID (this);
	m_dockManager.DockControlBar (pBar, nDockBarID, lpRect);
}
//*****************************************************************************
CBCGPBaseControlBar* CBCGPOleIPFrameWnd::ControlBarFromPoint (CPoint point, 
							int nSensitivity, bool bExactBar, 
							CRuntimeClass* pRTCBarType) const
{
	ASSERT_VALID (this);
	return m_dockManager.ControlBarFromPoint (point, nSensitivity, bExactBar, 
												pRTCBarType);
}
//*****************************************************************************
CBCGPBaseControlBar* CBCGPOleIPFrameWnd::ControlBarFromPoint (CPoint point, 
								int nSensitivity, DWORD& dwAlignment, 
								CRuntimeClass* pRTCBarType) const
{
	ASSERT_VALID (this);
	return m_dockManager.ControlBarFromPoint (point, nSensitivity, dwAlignment, 
												pRTCBarType);
}
//*****************************************************************************
BOOL CBCGPOleIPFrameWnd::IsPointNearDockBar (CPoint point, DWORD& dwBarAlignment, 
											 BOOL& bOuterEdge) const
{
	ASSERT_VALID (this);
	return m_dockManager.IsPointNearDockBar (point, dwBarAlignment, bOuterEdge);
}
//*****************************************************************************
void CBCGPOleIPFrameWnd::AdjustDockingLayout (HDWP hdwp)
{
	ASSERT_VALID (this);
	CWnd* pChildWnd = GetWindow (GW_CHILD);
	while (pChildWnd != NULL)
	{
		ASSERT_VALID (pChildWnd);
		if (!pChildWnd->IsKindOf (RUNTIME_CLASS (CBCGPBaseControlBar)))
		{
			break;
		}
		pChildWnd = GetWindow (GW_HWNDNEXT);
	}

	m_dockManager.AdjustDockingLayout (hdwp);
}
//*****************************************************************************
BOOL CBCGPOleIPFrameWnd::OnMoveMiniFrame	(CWnd* pFrame)
{
	ASSERT_VALID (this);
	return m_dockManager.OnMoveMiniFrame (pFrame);
}
//****************************************************************************************
BOOL CBCGPOleIPFrameWnd::EnableDocking (DWORD dwDockStyle)
{
	return m_dockManager.EnableDocking (dwDockStyle);
}
//****************************************************************************************
BOOL CBCGPOleIPFrameWnd::EnableAutoHideBars (DWORD dwDockStyle, BOOL bActivateOnMouseClick)
{
	return m_dockManager.EnableAutoHideBars (dwDockStyle, bActivateOnMouseClick);
}
//****************************************************************************************
BOOL CBCGPOleIPFrameWnd::PreCreateWindow(CREATESTRUCT& cs) 
{
	m_Impl.SetDockManager (&m_dockManager);	
	return COleIPFrameWnd::PreCreateWindow(cs);
}
//****************************************************************************************
CBCGPBaseControlBar* CBCGPOleIPFrameWnd::GetControlBar (UINT nID)
{
	ASSERT_VALID (this);

	CBCGPBaseControlBar* pBar = m_dockManager.FindBarByID (nID, TRUE);
	return pBar;
}
//****************************************************************************************
void CBCGPOleIPFrameWnd::ShowControlBar (CBCGPBaseControlBar* pBar, BOOL bShow, BOOL bDelay, BOOL bActivate)
{
	ASSERT_VALID (this);
	ASSERT_VALID (pBar);

	pBar->ShowControlBar (bShow, bDelay, bActivate);
}
//*************************************************************************************
void CBCGPOleIPFrameWnd::OnUpdateControlBarMenu(CCmdUI* pCmdUI)
{
	CBCGPBaseControlBar* pBar = GetControlBar(pCmdUI->m_nID);
	if (pBar != NULL)
	{
		pCmdUI->SetCheck((pBar->GetStyle() & WS_VISIBLE) != 0);
		return;
	}

	pCmdUI->ContinueRouting();
}
//*************************************************************************************
BOOL CBCGPOleIPFrameWnd::OnBarCheck(UINT nID)
{
	ASSERT_VALID (this);

	CBCGPBaseControlBar* pBar = GetControlBar(nID);
	if (pBar != NULL)
	{
		ShowControlBar(pBar, (pBar->GetStyle() & WS_VISIBLE) == 0, FALSE, FALSE);
		return TRUE;
	}

	return FALSE;
}
//*************************************************************************************
BOOL CBCGPOleIPFrameWnd::DockControlBarLeftOf (CBCGPControlBar* pBar, CBCGPControlBar* pLeftOf)
{
	return m_dockManager.DockControlBarLeftOf (pBar, pLeftOf);
}
//*************************************************************************************
void CBCGPOleIPFrameWnd::OnSetPreviewMode(BOOL bPreview, CPrintPreviewState* pState) 
{
	ASSERT_VALID (this);

	m_dockManager.SetPrintPreviewMode (bPreview, pState);
	DWORD dwSavedState = pState->dwStates;
	COleIPFrameWnd::OnSetPreviewMode(bPreview, pState);
	pState->dwStates = dwSavedState;
	RecalcLayout ();
}
//*************************************************************************************
void CBCGPOleIPFrameWnd::RecalcLayout (BOOL bNotify)
{
	COleIPFrameWnd::RecalcLayout (bNotify);

	if (m_bInRecalcLayout)
		return;

	m_bInRecalcLayout = TRUE;

	m_dockManager.AdjustDockingLayout ();
	m_dockManager.RecalcLayout (bNotify);

	CView* pView = GetActiveView ();
	if (pView != NULL && pView->IsKindOf (RUNTIME_CLASS (CBCGPPrintPreviewView)) && 
		m_dockManager.IsPrintPreviewValid ())
	{
		CRect rectClient = m_dockManager.GetClientAreaBounds ();
		pView->SetWindowPos (NULL, rectClient.left, rectClient.top, 
								rectClient.Width (), rectClient.Height (),
								SWP_NOZORDER  | SWP_NOACTIVATE);
	}

	m_bInRecalcLayout = FALSE;
}
//********************************************************************************
LRESULT CBCGPOleIPFrameWnd::OnIdleUpdateCmdUI(WPARAM, LPARAM)
{
	COleIPFrameWnd::OnIdleUpdateCmdUI();
	m_dockManager.SendMessageToMiniFrames (WM_IDLEUPDATECMDUI);

	CBCGPOleCntrFrameWnd* pFrame = DYNAMIC_DOWNCAST (CBCGPOleCntrFrameWnd, m_pMainFrame);
	if (pFrame!= NULL)
	{
		pFrame->OnIdleUpdateCmdUI();
	}

	return 0L;
}
//********************************************************************************
BOOL CBCGPOleIPFrameWnd::OnCreateControlBars(CFrameWnd* pWndFrame, CFrameWnd* pWndDoc)
{
	// Remove this if you use pWndDoc
	UNREFERENCED_PARAMETER(pWndDoc);

	CBCGPOleCntrFrameWnd* pNewFrame = DYNAMIC_DOWNCAST (CBCGPOleCntrFrameWnd,
		pWndFrame);

	if (pNewFrame == NULL)
	{
		ASSERT (m_pMainFrame == pWndFrame);
		pNewFrame = new CBCGPOleCntrFrameWnd (this);
		ASSERT_VALID (pNewFrame);

		HWND hwndFrame = m_pMainFrame->Detach ();
		delete m_pMainFrame;
		m_pMainFrame = pNewFrame;

		m_pMainFrame->Attach (hwndFrame);
		pNewFrame->CreateDockManager ();
	}

	return TRUE;
}
//********************************************************************************
BOOL CBCGPOleIPFrameWnd::OnShowControlBars (BOOL bShow)
{
	ASSERT_VALID (this);
	BOOL bResult = m_dockManager.ShowControlBars (bShow);
	AdjustDockingLayout ();
	
	return bResult;
}


CBCGPOleCntrFrameWnd* CBCGPOleIPFrameWnd::GetContainerFrameWindow()
{
		
		CBCGPOleCntrFrameWnd* pNewFrame = DYNAMIC_DOWNCAST (CBCGPOleCntrFrameWnd, m_pMainFrame);
		ASSERT_VALID (pNewFrame);
		return pNewFrame;
}

LRESULT CBCGPOleIPFrameWnd::OnToolbarContextMenu(WPARAM,LPARAM)
{
	return 1l;
}

BOOL CBCGPOleIPFrameWnd::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	if (COleIPFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
	{
		return TRUE;
	}

	return m_dockManager.ProcessControlBarContextMenuCommand (nID, nCode, pExtra, pHandlerInfo);
}

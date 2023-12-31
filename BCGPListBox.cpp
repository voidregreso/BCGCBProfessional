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
//
// BCGPListBox.cpp : implementation file
//

#include "stdafx.h"
#include "BCGPListBox.h"
#include "BCGPDlgImpl.h"
#include "BCGPVisualManager.h"
#include "BCGPPropertySheet.h"
#include "BCGPStatic.h"
#include "TrackMouse.h"
#include "BCGPLocalResource.h"
#include "bcgprores.h"
#include "BCGPGlobalUtils.h"

#ifndef _BCGSUITE_
	#include "BCGPMultiDocTemplate.h"
	#include "BCGPTooltipManager.h"
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

struct BCGP_LB_ITEM_DATA
{
public:
	int			m_nCheck;
	DWORD_PTR	m_dwData;
	CString		m_strDescription;
	BOOL		m_bIsPinned;
	BOOL		m_bIsEnabled;
	int			m_nImageIndex;
	HICON		m_hIcon;
	CString		m_strToolTip;
	CString		m_strToolTipDescription;
	COLORREF	m_clrBar;
	BOOL		m_nCheckEnabled;
	
	BCGP_LB_ITEM_DATA()
		: m_nCheck(BST_UNCHECKED)
		, m_dwData(0)
		, m_bIsPinned(FALSE)
		, m_bIsEnabled(TRUE)
		, m_nImageIndex(-1)
		, m_hIcon(NULL)
		, m_clrBar((COLORREF)-1)
		, m_nCheckEnabled(TRUE)
	{
	}

	~BCGP_LB_ITEM_DATA()
	{
		if (m_hIcon != NULL)
		{
			::DestroyIcon(m_hIcon);
		}
	}
};

IMPLEMENT_DYNAMIC(CBCGPListBox, CListBox)

#define PIN_AREA_WIDTH	(CBCGPVisualManager::GetInstance ()->GetPinSize(TRUE).cx + globalUtils.ScaleByDPI(10))
#define BCGP_DEFAULT_TABS_STOP	32

UINT BCGM_ON_CLICK_LISTBOX_PIN = ::RegisterWindowMessage (_T("BCGM_ON_CLICK_LISTBOX_PIN"));
UINT BCGM_ON_CHANGE_BACKSTAGE_PROP_HIGHLIGHTING = ::RegisterWindowMessage (_T("BCGM_ON_CHANGE_BACKSTAGE_PROP_HIGHLIGHTING"));

/////////////////////////////////////////////////////////////////////////////
// CBCGPListBox

CBCGPListBox::CBCGPListBox()
{
	m_bVisualManagerStyle = FALSE;
	m_bOnGlass = FALSE;
	m_nHighlightedItem = -1;
	m_bHighlightedWasChanged = FALSE;
	m_bItemHighlighting = TRUE;
	m_bTracked = FALSE;
	m_hImageList = NULL;
	m_sizeImage = CSize(0, 0);
	m_bBackstageMode = FALSE;
	m_bPropertySheetNavigator = FALSE;
	m_bPins = FALSE;
	m_bIsPinHighlighted = FALSE;
	m_bIsCheckHighlighted = FALSE;
	m_bHasCheckBoxes = FALSE;
	m_bHasDescriptions = FALSE;
	m_bHasColorBars = FALSE;
	m_nDescrRows = 0;
	m_nClickedItem = -1;
	m_hFont	= NULL;
	m_nTextHeight = -1;
	m_bInAddingCaption = FALSE;
	m_pToolTip = NULL;
	m_bRebuildTooltips = FALSE;
	m_nLastScrollPos = -1;
	m_nItemExtraHeight = 0;
	m_nVMExtraHeight = 0;
	m_nIconCount = 0;

	m_arTabStops.Add(BCGP_DEFAULT_TABS_STOP);
}

CBCGPListBox::~CBCGPListBox()
{
	CBCGPTooltipManager::DeleteToolTip(m_pToolTip);
}

BEGIN_MESSAGE_MAP(CBCGPListBox, CListBox)
	//{{AFX_MSG_MAP(CBCGPListBox)
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_VSCROLL()
	ON_WM_LBUTTONDOWN()
	ON_WM_KEYDOWN()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONUP()
	ON_WM_CANCELMODE()
	ON_WM_HSCROLL()
	ON_WM_NCPAINT()
	ON_WM_CREATE()
	ON_MESSAGE(LB_ADDSTRING, OnLBAddString)
	ON_MESSAGE(LB_GETITEMDATA, OnLBGetItemData)
	ON_MESSAGE(LB_INSERTSTRING, OnLBInsertString)
	ON_MESSAGE(LB_SETITEMDATA, OnLBSetItemData)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
	ON_REGISTERED_MESSAGE(BCGM_ONSETCONTROLVMMODE, OnBCGSetControlVMMode)
	ON_REGISTERED_MESSAGE(BCGM_ONSETCONTROLAERO, OnBCGSetControlAero)
	ON_REGISTERED_MESSAGE(BCGM_ONSETCONTROLBACKSTAGEMODE, OnBCGSetControlBackStageMode)
	ON_REGISTERED_MESSAGE(BCGM_UPDATETOOLTIPS, OnBCGUpdateToolTips)
	ON_REGISTERED_MESSAGE(BCGM_CHANGEVISUALMANAGER, OnBCGChangeVisualManager)
	ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
	ON_CONTROL_REFLECT_EX(LBN_SELCHANGE, OnSelchange)
	ON_MESSAGE(WM_PRINTCLIENT, OnPrintClient)
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_MESSAGE(LB_SETTABSTOPS, OnLBSetTabstops)
	ON_MESSAGE(LB_SETCURSEL, OnLBSetCurSel)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXT, 0, 0xFFFF, OnTTNeedTipText)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBCGPListBox message handlers

const BCGP_LB_ITEM_DATA* CBCGPListBox::_GetItemData(int nItem) const
{
	const BCGP_LB_ITEM_DATA* pState = NULL;

	if (GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED))
	{
		LRESULT lResult = ((CBCGPListBox*)this)->DefWindowProc(LB_GETITEMDATA, nItem, 0);
		if (lResult != LB_ERR)
		{
			pState = (const BCGP_LB_ITEM_DATA*)lResult;
		}
	}

	return pState;
}
//**************************************************************************
BCGP_LB_ITEM_DATA* CBCGPListBox::_GetAllocItemData(int nItem)
{
	BCGP_LB_ITEM_DATA* pState = NULL;
	
	if (GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED))
	{
		LRESULT lResult = DefWindowProc(LB_GETITEMDATA, nItem, 0);
		if (lResult != LB_ERR)
		{
			pState = (BCGP_LB_ITEM_DATA*)lResult;
			if (pState == NULL)
			{
				pState = new BCGP_LB_ITEM_DATA;
				VERIFY(DefWindowProc(LB_SETITEMDATA, nItem, (LPARAM)pState) != LB_ERR);
			}
		}
	}

	return pState;
}
//**************************************************************************
LRESULT CBCGPListBox::OnBCGSetControlVMMode (WPARAM wp, LPARAM)
{
	m_bVisualManagerStyle = (BOOL) wp;
	return 0;
}
//**************************************************************************
LRESULT CBCGPListBox::OnBCGSetControlAero (WPARAM wp, LPARAM)
{
	m_bOnGlass = (BOOL) wp;
	return 0;
}
//**************************************************************************
CRect CBCGPListBox::GetCheckBoxRect(const CRect& rectItem) const
{
	int nHorzMargin = globalUtils.ScaleByDPI(4);
	const int nTextRowHeight = m_nTextHeight == -1 ? globalData.GetTextHeight() : m_nTextHeight;

	CSize sizeCheckBox = m_bVisualManagerStyle ? 
		CBCGPVisualManager::GetInstance ()->GetCheckRadioDefaultSize() :
		CBCGPVisualManager::GetInstance ()->CBCGPVisualManager::GetCheckRadioDefaultSize();
	
	CRect rectCheck = rectItem;
	rectCheck.right = rectCheck.left + sizeCheckBox.cx + nHorzMargin;
	
	if (m_nDescrRows > 0)
	{
		rectCheck.DeflateRect(0, nTextRowHeight / 2);
		rectCheck.bottom = rectCheck.top + sizeCheckBox.cy;
	}
	
	int dx = max(0, (rectCheck.Width() - sizeCheckBox.cx) / 2);
	int dy = max(0, (rectCheck.Height() - sizeCheckBox.cy) / 2);
	
	rectCheck.DeflateRect(dx, dy);
	return rectCheck;
}
//**************************************************************************
int CBCGPListBox::GetColorBarWidth() const
{
	return m_bHasColorBars ? globalUtils.ScaleByDPI(6) : 0;
}
//**************************************************************************
int CBCGPListBox::HitTest(CPoint pt, BOOL* pbPin, BOOL* pbCheck)
{
	if (pbPin != NULL)
	{
		*pbPin = FALSE;
	}

	if (pbCheck != NULL)
	{
		*pbCheck = FALSE;
	}

	if ((GetStyle() & LBS_NOSEL) == LBS_NOSEL)
	{
		return -1;
	}

	for (int i = 0; i < GetCount (); i++)
	{
		CRect rectItem;
		GetItemRect (i, rectItem);

		if (rectItem.PtInRect (pt))
		{
			if (IsCaptionItem(i) || IsSeparatorItem(i))
			{
				return -1;
			}

			BOOL bIsEnabled = IsEnabled(i);
			
			if (pbPin != NULL && m_bPins && bIsEnabled)
			{
				*pbPin = (pt.x > rectItem.right - PIN_AREA_WIDTH);
			}

			if (pbCheck != NULL && m_bHasCheckBoxes && bIsEnabled && IsCheckEnabled(i))
			{
				if (pt.x < rectItem.left + rectItem.Height())
				{
					rectItem.left += GetColorBarWidth();

					CRect rectCheck = GetCheckBoxRect(rectItem);
					rectCheck.DeflateRect(1, 1);

					*pbCheck =  rectCheck.PtInRect(pt);
				}
			}

			return i;
		}
	}

	return -1;
}
//**************************************************************************
void CBCGPListBox::OnMouseMove(UINT nFlags, CPoint point)
{
	CListBox::OnMouseMove (nFlags, point);

	if ((GetStyle() & LBS_NOSEL) == LBS_NOSEL)
	{
		return;
	}

	ASSERT (IsWindowEnabled ());

	BOOL bIsPinHighlighted = FALSE;
	BOOL bIsCheckHighlighted = FALSE;

	BOOL bItemHighlighting = (m_bItemHighlighting && m_bVisualManagerStyle) || (nFlags & MK_LBUTTON);

	int nHighlightedItem = HitTest(point, &bIsPinHighlighted, &bIsCheckHighlighted);
	BOOL bIsMouseEnter = FALSE;

	if (!m_bTracked)
	{
		m_bTracked = TRUE;
		
		TRACKMOUSEEVENT trackmouseevent;
		trackmouseevent.cbSize = sizeof(trackmouseevent);
		trackmouseevent.dwFlags = TME_LEAVE;
		trackmouseevent.hwndTrack = GetSafeHwnd();
		trackmouseevent.dwHoverTime = HOVER_DEFAULT;
		BCGPTrackMouse (&trackmouseevent);

		bIsMouseEnter = TRUE;
	}

	if (nHighlightedItem != m_nHighlightedItem || m_bIsPinHighlighted != bIsPinHighlighted || m_bIsCheckHighlighted != bIsCheckHighlighted)
	{
		CRect rectItem;

		if (bItemHighlighting && m_nHighlightedItem >= 0 && nHighlightedItem != m_nHighlightedItem)
		{
			GetItemRect (m_nHighlightedItem, rectItem);
			InvalidateRect (rectItem);
		}

		m_nHighlightedItem = nHighlightedItem;
		m_bHighlightedWasChanged = TRUE;

		m_bIsPinHighlighted = bIsPinHighlighted;
		m_bIsCheckHighlighted = bIsCheckHighlighted;

		if (bItemHighlighting && m_nHighlightedItem >= 0)
		{
			GetItemRect (m_nHighlightedItem, rectItem);
			InvalidateRect (rectItem);
		}

		if (bItemHighlighting)
		{
			UpdateWindow();
		}
	}

	if (IsBackstagePageSelector() && bIsMouseEnter)
	{
		CWnd* pWndTopLevel = GetTopLevelParent();
		if (pWndTopLevel->GetSafeHwnd() != NULL)
		{
			pWndTopLevel->SendMessage(BCGM_ON_CHANGE_BACKSTAGE_PROP_HIGHLIGHTING);
		}
	}
}
//***********************************************************************************************	
LRESULT CBCGPListBox::OnMouseLeave(WPARAM,LPARAM)
{
	m_bTracked = FALSE;
	m_bIsPinHighlighted = FALSE;
	m_bIsCheckHighlighted = FALSE;

	if (m_nHighlightedItem >= 0)
	{
		CRect rectItem;
		GetItemRect (m_nHighlightedItem, rectItem);

		m_nHighlightedItem = -1;

		if (m_bItemHighlighting && m_bVisualManagerStyle)
		{
			RedrawWindow (rectItem);
		}
	}

	return 0;
}
//***********************************************************************************************	
void CBCGPListBox::DrawItem(LPDRAWITEMSTRUCT /*lpDIS*/)
{
}
//***********************************************************************************************	
void CBCGPListBox::MeasureItem(LPMEASUREITEMSTRUCT lpMIS) 
{
	lpMIS->itemHeight = GetItemMinHeight();

	if ((GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS)) == (LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS))
	{
		POSITION pos = m_lstCaptionIndexes.Find(lpMIS->itemID);
		if (pos != NULL || m_bInAddingCaption)
		{
			const int nSeparatorHeight = 10;

			CString strText;
			GetText(lpMIS->itemID, strText);

			if (strText.IsEmpty())
			{
				lpMIS->itemHeight = nSeparatorHeight;
			}
			else
			{
				lpMIS->itemHeight += nSeparatorHeight;
			}
		}
	}
}
//***********************************************************************************************	
CSize CBCGPListBox::GetItemImageSize(int nIndex) const
{
	ASSERT_VALID(this);

	HICON hIcon = GetItemIcon(nIndex);
	if (hIcon != NULL)
	{
		return globalUtils.GetIconSize(hIcon);
	}

	CSize sizeImage = m_sizeImage;
	
	if (sizeImage == CSize(0, 0) && m_nIconCount > 0)
	{
		sizeImage = globalData.m_sizeSmallIcon;
	}

	return sizeImage;
}
//***********************************************************************************************	
COLORREF CBCGPListBox::GetCaptionTextColor(int nIndex)
{
	ASSERT_VALID(this);
	return CBCGPVisualManager::GetInstance()->GetListBoxCaptionTextColor(this, nIndex);
}
//***********************************************************************************************	
void CBCGPListBox::OnDrawItemColorBar(CDC* pDC, CRect rect, int /*nIndex*/, COLORREF color)
{
	ASSERT_VALID(this);
	ASSERT_VALID(pDC);

	CBrush br(color);
	pDC->FillRect(rect, &br);
}
//***********************************************************************************************	
void CBCGPListBox::OnDrawItemContent(CDC* pDC, CRect rect, int nIndex)
{
	ASSERT_VALID(this);
	ASSERT_VALID(pDC);

	const BOOL bIsCaption = IsCaptionItem(nIndex);

	CFont* pOldFont = NULL;

	int nHorzMargin = globalUtils.ScaleByDPI(4);

	const int nTextRowHeight = m_nTextHeight == -1 ? globalData.GetTextHeight() : m_nTextHeight;
	const int xStart = rect.left;

	if (m_bHasColorBars)
	{
		const int nColorBarWidth = GetColorBarWidth();
		const COLORREF color = GetItemBarColor(nIndex);

		if (color != (COLORREF)-1)
		{
			const int nPadding = globalUtils.ScaleByDPI(1);

			CRect rectColorBar = rect;
			rectColorBar.right = rectColorBar.left + nColorBarWidth;
			rectColorBar.DeflateRect(nPadding, nPadding);

			OnDrawItemColorBar(pDC, rectColorBar, nIndex, color);
		}
		
		rect.left += nColorBarWidth;
	}

	if (!bIsCaption)
	{
		if (m_bHasCheckBoxes)
		{
			CRect rectCheck = GetCheckBoxRect(rect);
			rect.left = rectCheck.right;

			BOOL bIsHighlighted = m_bIsCheckHighlighted && nIndex == m_nHighlightedItem;
			BOOL bIsEnabled = IsWindowEnabled() && IsEnabled(nIndex) && IsCheckEnabled(nIndex);

			if (m_bVisualManagerStyle)
			{
				CBCGPVisualManager::GetInstance ()->OnDrawCheckBoxEx
					(pDC, rectCheck, GetCheck(nIndex), bIsHighlighted, FALSE, bIsEnabled);
			}
			else
			{
				if (!CBCGPVisualManager::GetInstance()->CBCGPVisualManager::DrawCheckBox(pDC, rectCheck, bIsHighlighted, GetCheck(nIndex), bIsEnabled, FALSE))
				{
					CBCGPVisualManager::GetInstance()->CBCGPVisualManager::OnDrawCheckBoxEx(pDC, rectCheck, GetCheck(nIndex), bIsHighlighted, FALSE, bIsEnabled);
				}
			}
		}

		HICON hIcon = GetItemIcon(nIndex);
		CSize sizeImage = GetItemImageSize(nIndex);
		int nIcon = (m_hImageList != NULL || m_ImageList.GetCount() > 0) ? GetItemImage(nIndex) : -1;

		if (m_nIconCount > 0 || nIcon >= 0)
		{
			CRect rectTop = rect;

			if (m_bHasDescriptions && m_nDescrRows > 0 && sizeImage.cy < rectTop.Height() / 2)
			{
				rectTop.DeflateRect(0, nTextRowHeight / 3);
				rectTop.bottom = rectTop.top + rect.Height() / (m_nDescrRows + 1);
			}

			CRect rectImage = rectTop;
			rectImage.top += max(0, (rectTop.Height () - sizeImage.cy) / 2);
			rectImage.bottom = rectImage.top + sizeImage.cy;

			rectImage.left += nHorzMargin;
			rectImage.right = rectImage.left + sizeImage.cx;

			if (hIcon != NULL)
			{
				::DrawIconEx(pDC->GetSafeHdc (), 
					rectImage.left,
					rectImage.top,
					hIcon, sizeImage.cx, sizeImage.cy, 0, NULL,
					DI_NORMAL);
			}
			else if (m_hImageList != NULL)
			{
				CImageList* pImageList = CImageList::FromHandle(m_hImageList);
				pImageList->Draw (pDC, nIcon, rectImage.TopLeft (), ILD_TRANSPARENT);
			}
			else if (m_ImageList.GetCount() > 0)
			{
				m_ImageList.DrawEx(pDC, rectImage, nIcon);
			}

			rect.left += sizeImage.cx + max(2 * nHorzMargin, sizeImage.cx / 3);
			rect.right -= 2 * nHorzMargin;
		}
		else
		{
			rect.DeflateRect(nHorzMargin, 0);
		}
	}
#ifndef _BCGSUITE_
	else
	{
		pOldFont = pDC->SelectObject(&globalData.fontCaption);
	}
#endif

	CString strText;
	if (GetStyle() & LBS_HASSTRINGS)
	{
		GetText(nIndex, strText);
	}

	UINT uiDTFlags = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;

	if (bIsCaption)
	{
		int nTextHeight = 0;
		if (!strText.IsEmpty())
		{
			COLORREF clrText = GetCaptionTextColor(nIndex);
			COLORREF clrTextOld = (COLORREF)-1;

			if (clrText != (COLORREF)-1)
			{
				clrTextOld = pDC->SetTextColor(clrText);
			}

			nTextHeight = OnDrawItemName(pDC, xStart, rect, nIndex, strText, uiDTFlags);

			if (clrTextOld != (COLORREF)-1)
			{
				pDC->SetTextColor(clrTextOld);
			}
		}

		if (CBCGPVisualManager::GetInstance ()->IsUnderlineListBoxCaption(this))
		{
			CBCGPStatic ctrl;
			ctrl.m_bBackstageMode = m_bBackstageMode;

			CRect rectSeparator = rect;
			rectSeparator.top = rect.CenterPoint().y + nTextHeight / 2;
			rectSeparator.bottom = rectSeparator.top + 1;
			rectSeparator.right -= nHorzMargin + 1;

#ifndef _BCGSUITE_
			if (!globalData.IsHighContastMode () && CBCGPVisualManager::GetInstance ()->IsOwnerDrawDlgSeparator(&ctrl))
			{
				CBCGPVisualManager::GetInstance ()->OnDrawDlgSeparator(pDC, &ctrl, rectSeparator, TRUE);
			}
			else
#endif
			{
				CPen pen (PS_SOLID, 1, globalData.clrBtnShadow);
				CPen* pOldPen = (CPen*)pDC->SelectObject (&pen);

				pDC->MoveTo(rectSeparator.left, rectSeparator.top);
				pDC->LineTo(rectSeparator.right, rectSeparator.top);

				pDC->SelectObject(pOldPen);
			}
		}

		pDC->SelectObject(pOldFont);
	}
	else if (!m_bHasDescriptions || m_nDescrRows == 0)
	{
		OnDrawItemName(pDC, xStart, rect, nIndex, strText, uiDTFlags);
	}
	else
	{
		if (m_hFont != NULL)
		{
			pOldFont = pDC->SelectObject (CFont::FromHandle(m_hFont));
		}
		else
		{
			pOldFont = pDC->SelectObject(&globalData.fontBold);
		}

		rect.DeflateRect(0, nTextRowHeight / 3);

		CRect rectTop = rect;
		rectTop.bottom = rectTop.top + rectTop.Height() / (m_nDescrRows + 1);

		OnDrawItemName(pDC, xStart, rectTop, nIndex, strText, uiDTFlags);

		pDC->SelectObject(pOldFont);

		LPCTSTR lpszDescr = GetItemDescription(nIndex);
		if (lpszDescr != NULL)
		{
			CString strDescr(lpszDescr);
			
			if (!strDescr.IsEmpty())
			{
				CRect rectBottom = rect;
				rectBottom.top = rectTop.bottom;

				uiDTFlags = DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX | DT_WORDBREAK;
				OnDrawItemDescription(pDC, rectBottom, nIndex, strDescr, uiDTFlags);
			}
		}
	}
}
//***************************************************************************************
void CBCGPListBox::OnDrawItemDescription(CDC* pDC, CRect rect, int /*nIndex*/, const CString& strDescr, UINT uiDTFlags)
{
	ASSERT_VALID(this);
	ASSERT_VALID(pDC);

	pDC->DrawText(strDescr, rect, uiDTFlags);
}
//***************************************************************************************
int CBCGPListBox::DU2DP(int nDU)
{
	CRect rectDummy(0, 0, nDU, nDU);
	if (GetParent()->GetSafeHwnd() != NULL && MapDialogRect(GetParent()->GetSafeHwnd(), &rectDummy))
	{
		return rectDummy.right;
	}
	else
	{
		return MulDiv(nDU, LOWORD(::GetDialogBaseUnits()), 4);
	}
}
//***************************************************************************************
int CBCGPListBox::OnDrawItemName(CDC* pDC, int /*xStart*/, CRect rect, int nIndex, const CString& strName, UINT nDrawFlags)
{
	ASSERT_VALID(pDC);

	const int nTabStops = (int)m_arTabStops.GetSize();

	if ((GetStyle() & LBS_USETABSTOPS) == 0 || nTabStops == 0)
	{
		return pDC->DrawText (strName, rect, nDrawFlags);
	}

	int nHorzMargin = globalUtils.ScaleByDPI(4);
	int cy = pDC->GetTextExtent(strName).cy;
	int nOffset = 0;

	if (m_bHasCheckBoxes && IsCaptionItem(nIndex))
	{
		nOffset += CBCGPVisualManager::GetInstance ()->GetCheckRadioDefaultSize().cx;
	}

	if (m_hImageList != NULL || m_ImageList.GetCount() > 0 || GetItemIcon(nIndex) != NULL)
	{
		CSize sizeImage = GetItemImageSize(nIndex);

		if (GetItemImage(nIndex) < 0 || IsCaptionItem(nIndex) || m_nIconCount > 0)
		{
			nOffset += sizeImage.cx + max(2 * nHorzMargin, sizeImage.cx / 3);
		}
		else
		{
			nOffset += nHorzMargin;
		}
	}

	pDC->TabbedTextOut(rect.left, rect.top + max(0, (rect.Height() - cy) / 2), strName, nTabStops, m_arTabStops.GetData(), rect.left + nOffset);
	return cy;
}
//***************************************************************************************
BOOL CBCGPListBox::SetImageList (HIMAGELIST hImageList, int nVertMargin)
{
	ASSERT (hImageList != NULL);

	m_hImageList = NULL;
	m_ImageList.Clear();
	m_sizeImage = CSize(0, 0);

	CImageList* pImageList = CImageList::FromHandle (hImageList);
	if (pImageList == NULL)
	{
		ASSERT (FALSE);
		return FALSE;
	}

	IMAGEINFO info;
	pImageList->GetImageInfo (0, &info);

	CRect rectImage = info.rcImage;
	m_sizeImage = rectImage.Size ();

	m_hImageList = hImageList;

	SetItemHeight(-1, max(GetItemHeight(-1), m_sizeImage.cy + 2 * nVertMargin));
	return TRUE;
}
//***************************************************************************************
BOOL CBCGPListBox::SetImageList(UINT nImageListResID, int cxIcon, int nVertMargin, BOOL bAutoScale)
{
	m_hImageList = NULL;
	m_ImageList.Clear();
	m_sizeImage = CSize(0, 0);

	if (nImageListResID == 0)
	{
		return TRUE;
	}

	if (!m_ImageList.Load(nImageListResID))
	{
		ASSERT (FALSE);
		return FALSE;
	}

#ifndef _BCGSUITE_
	m_ImageList.SetSingleImage(FALSE);
#else
	m_ImageList.SetSingleImage();
#endif

	int cyIcon = m_ImageList.GetImageSize().cy;
	m_sizeImage = CSize(cxIcon, cyIcon);

	m_ImageList.SetImageSize(m_sizeImage, TRUE);

	if (bAutoScale)
	{
		m_sizeImage = globalUtils.ScaleByDPI(m_ImageList);
	}

	SetItemHeight(-1, max(GetItemHeight(-1), m_sizeImage.cy + 2 * nVertMargin));
	return TRUE;
}
//***********************************************************************************************************
void CBCGPListBox::SetItemImage(int nIndex, int nImageIndex)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		pState->m_nImageIndex = nImageIndex;
	}
}
//***********************************************************************************************************
int CBCGPListBox::GetItemImage(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_nImageIndex;
	}

	return -1;
}
//***********************************************************************************************************
void CBCGPListBox::SetItemIcon(int nIndex, HICON hIcon)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		if (pState->m_hIcon != NULL)
		{
			::DestroyIcon(pState->m_hIcon);
			pState->m_hIcon = NULL;

			m_nIconCount--;
		}

		if (hIcon != NULL)
		{
			pState->m_hIcon = ::CopyIcon(hIcon);
			m_nIconCount++;
		}
	}
}
//***********************************************************************************************************
HICON CBCGPListBox::GetItemIcon(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_hIcon;
	}

	return NULL;
}
//***********************************************************************************************************
void CBCGPListBox::SetItemDescription(int nIndex, const CString& strDescription)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		pState->m_strDescription = strDescription;

		if (!strDescription.IsEmpty())
		{
			m_bHasDescriptions = TRUE;
		}
	}
}
//***********************************************************************************************************
LPCTSTR CBCGPListBox::GetItemDescription(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_strDescription;
	}
	
	return NULL;
}
//***********************************************************************************************************
void CBCGPListBox::SetItemColorBar(int nIndex, COLORREF color)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		pState->m_clrBar = color;
		m_bHasColorBars = TRUE;
	}
}
//***********************************************************************************************************
COLORREF CBCGPListBox::GetItemBarColor(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_clrBar;
	}
	
	return (COLORREF)-1;
}
//***********************************************************************************************************
void CBCGPListBox::SetItemToolTip(int nIndex, const CString& strToolTip, LPCTSTR lpszDescription/* = NULL*/)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		pState->m_strToolTip = strToolTip;
		pState->m_strToolTipDescription = lpszDescription == NULL ? _T("") : lpszDescription;

		RebuildToolTips();
	}
}
//***********************************************************************************************************
LPCTSTR CBCGPListBox::GetItemToolTip(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_strToolTip.IsEmpty() ? NULL : (LPCTSTR)pState->m_strToolTip;
	}
	
	return NULL;
}
//***********************************************************************************************************
LPCTSTR CBCGPListBox::GetItemToolTipDescription(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_strToolTipDescription.IsEmpty() ? NULL : (LPCTSTR)pState->m_strToolTipDescription;
	}
	
	return NULL;
}
//***********************************************************************************************************
BOOL CBCGPListBox::OnEraseBkgnd(CDC* /*pDC*/)
{
	if (!(GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED)))
	{
		return (BOOL)Default();
	}

	if (GetHorizontalExtent() > 0 && GetScrollPos(SB_HORZ) > 0)
	{
		return (BOOL)Default();
	}

	return TRUE;
}
//***********************************************************************************************************
void CBCGPListBox::OnFillBackground(CDC* pDC, CRect rectClient, COLORREF& clrTextDefault)
{
#ifndef _BCGSUITE_
	if (m_bVisualManagerStyle)
	{
		clrTextDefault = CBCGPVisualManager::GetInstance ()->OnFillListBox(pDC, this, rectClient);
	}
	else
#endif
	{
		UNREFERENCED_PARAMETER(clrTextDefault);
		pDC->FillRect(rectClient, &globalData.brWindow);
	}
}
//***********************************************************************************************************
COLORREF CBCGPListBox::OnFillItem(CDC* pDC, int nIndex, CRect rect, BOOL bIsHighlihted, BOOL bIsSelected, BOOL& bFocusRectDrawn)
{
	if (m_bVisualManagerStyle)
	{
		return CBCGPVisualManager::GetInstance ()->OnFillListBoxItem(pDC, this, nIndex, rect, bIsHighlihted, bIsSelected);
	}

	pDC->FillRect(rect, &globalData.brHilite);
		
	if (bIsHighlihted)
	{
		pDC->DrawFocusRect (rect);
		bFocusRectDrawn = TRUE;
	}
		
	return globalData.clrTextHilite;
}
//***********************************************************************************************************
void CBCGPListBox::OnDraw(CDC* pDC) 
{
	ASSERT_VALID(pDC);

	BOOL bItemHighlighting = (m_bItemHighlighting && m_bVisualManagerStyle);

	if (m_bRebuildTooltips || (m_nLastScrollPos >= 0 && m_nLastScrollPos != GetScrollPos(SB_VERT)))
	{
		m_bRebuildTooltips = FALSE;
		m_nLastScrollPos = GetScrollPos(SB_VERT);

		if (m_pToolTip->GetSafeHwnd () != NULL)
		{
			RebuildToolTips();
		}
	}

	if (m_hFont != NULL && ::GetObjectType (m_hFont) != OBJ_FONT)
	{
		m_hFont = NULL;
		m_nTextHeight = -1;
	}
	
	CRect rectClient;
	GetClientRect(rectClient);

	int cxScroll = GetScrollPos(SB_HORZ);
	BOOL bNoSel = ((GetStyle() & LBS_NOSEL) == LBS_NOSEL);

	COLORREF clrTextDefault = globalData.clrWindowText;

	OnFillBackground(pDC, rectClient, clrTextDefault);

#ifndef _BCGSUITE_
	if (m_bBackstageMode)
	{
		if (m_bPropertySheetNavigator)
		{
			CBCGPPropertySheet* pPropSheet = DYNAMIC_DOWNCAST(CBCGPPropertySheet, GetParent());
			if (pPropSheet != NULL)
			{
				ASSERT_VALID(pPropSheet);
				pPropSheet->OnDrawListBoxBackground(pDC, this);
			}

			CRect rectSeparator = rectClient;
			rectSeparator.left = rectSeparator.right - 3;
			rectSeparator.right -= 2;
			CBCGPStatic ctrl;
			ctrl.m_bBackstageMode = TRUE;

			CBCGPVisualManager::GetInstance ()->OnDrawDlgSeparator(pDC, &ctrl, rectSeparator, FALSE);
		}
	}
#endif
	int nStart = GetTopIndex ();
	int nCount = GetCount ();

	if (nStart != LB_ERR && nCount > 0)
	{
		pDC->SetBkMode (TRANSPARENT);

		CFont* pOldFont = NULL;
		
		if (m_hFont != NULL)
		{
			pOldFont = pDC->SelectObject (CFont::FromHandle(m_hFont));
		}
		else
		{
			pOldFont = pDC->SelectObject (&globalData.fontRegular);
		}

		ASSERT_VALID (pOldFont);

		CArray<int,int> arSelection;

		int nSelCount = GetSelCount();
		if (nSelCount != LB_ERR && !bNoSel)
		{
			if (nSelCount > 0)
			{
				arSelection.SetSize (nSelCount);
				GetSelItems (nSelCount, arSelection.GetData());	
			}
		}
		else
		{
			int nSel = GetCurSel();
			if (nSel != LB_ERR)
			{
				nSelCount = 1;
				arSelection.Add (nSel);
			}
		}

		nSelCount = (int)arSelection.GetSize ();

		const BOOL bIsFocused = (CWnd::GetFocus() == this);
		
		BOOL bFocusRectDrawn = FALSE;

		for (int nIndex = nStart; nIndex < nCount; nIndex++)
		{
			CRect rect;
			GetItemRect(nIndex, rect);

			if (rect.bottom < rectClient.top || rectClient.bottom < rect.top)
			{
				break;
			}

			int cxExtent = GetHorizontalExtent();
			if (cxExtent > rect.Width())
			{
				rect.right = rect.left + cxExtent;
			}

			rect.OffsetRect(-cxScroll, 0);
			rect.DeflateRect(2, 0);

			CRect rectPin(0, 0, 0, 0);
			if (m_bPins && !IsSeparatorItem(nIndex) && !IsCaptionItem(nIndex))
			{
				rectPin = rect;
				rect.right -= PIN_AREA_WIDTH;
				rectPin.left = rect.right;
			}

			BOOL bIsSelected = FALSE;
			for (int nSelIndex = 0; nSelIndex < nSelCount; nSelIndex++)
			{
				if (nIndex == arSelection[nSelIndex])
				{
					bIsSelected = TRUE;
					break;
				}
			}

			BOOL bIsHighlihted = (nIndex == m_nHighlightedItem) || (bIsSelected && bIsFocused);

			COLORREF clrText = (COLORREF)-1;

			BOOL bIsCaptionItem = IsCaptionItem(nIndex);

			if (((bIsHighlihted && !m_bIsPinHighlighted && bItemHighlighting) || bIsSelected) && !bIsCaptionItem && !bNoSel)
			{
				clrText = OnFillItem(pDC, nIndex, rect, bIsHighlihted && bItemHighlighting, bIsSelected, bFocusRectDrawn);
			}

			BOOL bIsItemDisabled = !IsWindowEnabled() || !IsEnabled(nIndex);
			if (bIsItemDisabled)
			{
#ifndef _BCGSUITE_
				clrText = CBCGPVisualManager::GetInstance()->GetToolbarDisabledTextColor();
#else
				clrText = globalData.clrGrayedText;
#endif
			}

			if (clrText == (COLORREF)-1)
			{
				pDC->SetTextColor (clrTextDefault);
			}
			else
			{
				pDC->SetTextColor (clrText);
			}

			OnDrawItemContent(pDC, rect, nIndex);

			if (bIsFocused && !bFocusRectDrawn && !bIsCaptionItem && !bNoSel)
			{
				BOOL bDraw = FALSE;

				if ((GetStyle() & LBS_MULTIPLESEL) != 0)
				{
					bDraw = (nIndex == GetCurSel());
				}
				else if (GetStyle() & LBS_EXTENDEDSEL)
				{
					bDraw = (!bItemHighlighting && (nIndex == m_nHighlightedItem) && ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)) || (nSelCount <= 0);
				}
				else
				{
					bDraw = nSelCount <= 0;
				}

				if (bDraw)
				{
					pDC->DrawFocusRect (rect);
					bFocusRectDrawn = TRUE;
				}
			}

			if (!rectPin.IsRectEmpty())
			{
				BOOL bIsPinHighlighted = FALSE;
				COLORREF clrTextPin = clrTextDefault;

				if (nIndex == m_nHighlightedItem && m_bIsPinHighlighted)
				{
					bIsPinHighlighted = TRUE;
					clrTextPin = OnFillItem(pDC, nIndex, rectPin, TRUE, FALSE, bFocusRectDrawn);
				}

				BOOL bIsDark = TRUE;

				if (clrTextPin != (COLORREF)-1)
				{
					if (GetRValue (clrTextPin) > 192 &&
						GetGValue (clrTextPin) > 192 &&
						GetBValue (clrTextPin) > 192)
					{
						bIsDark = FALSE;
					}
				}

				BOOL bIsPinned = IsItemPinned(nIndex);

				CSize sizePin = CBCGPVisualManager::GetInstance ()->GetPinSize(bIsPinned);
				CRect rectPinImage(
					CPoint(
						rectPin.CenterPoint().x - sizePin.cx / 2,
						rectPin.CenterPoint().y - sizePin.cy / 2),
					sizePin);

				CBCGPVisualManager::GetInstance()->OnDrawPin(pDC, rectPinImage, bIsPinned,
					bIsDark, bIsPinHighlighted, FALSE, bIsItemDisabled);
			}
		}

		pDC->SelectObject (pOldFont);
	}
}
//***********************************************************************************************************
void CBCGPListBox::OnPaint() 
{
	if (!(GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED)))
	{
		Default();
		return;
	}

	CPaintDC dcPaint(this); // device context for painting

	CRect rectClient;
	GetClientRect(rectClient);

	CRgn rgn;
	rgn.CreateRectRgnIndirect (rectClient);
	dcPaint.SelectClipRgn (&rgn);

	{
		CBCGPMemDC memDC(dcPaint, this);
		OnDraw(&memDC.GetDC());
	}

	dcPaint.SelectClipRgn (NULL);
}
//************************************************************************************************************
BOOL CBCGPListBox::OnSelchange() 
{
	RedrawWindow();
	return FALSE;
}
//************************************************************************************************************
void CBCGPListBox::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	if (m_pToolTip->GetSafeHwnd () != NULL)
	{
		m_pToolTip->Pop();
	}

	CListBox::OnVScroll(nSBCode, nPos, pScrollBar);
	RedrawWindow();
}
//**************************************************************************
LRESULT CBCGPListBox::OnBCGSetControlBackStageMode (WPARAM, LPARAM)
{
	m_bBackstageMode = TRUE;
	return 0;
}
//**************************************************************************
LRESULT CBCGPListBox::OnBCGChangeVisualManager(WPARAM, LPARAM)
{
	if (m_bVisualManagerStyle && (GetStyle() & (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS)) == (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS))
	{
		int nVMExtraHeightPrev = m_nVMExtraHeight;
		m_nVMExtraHeight = CBCGPVisualManager::GetInstance()->GetListBoxItemExtraHeight(this);

		if (m_nVMExtraHeight != nVMExtraHeightPrev)
		{
			int nHeight = GetItemHeight(0);
			SetItemHeight(0, nHeight + m_nVMExtraHeight - nVMExtraHeightPrev);
		}
	}

	return 0;
}
//**************************************************************************
void CBCGPListBox::AddCaption(LPCTSTR lpszCaption)
{
	m_bInAddingCaption = TRUE;

	int nIndex = AddString(lpszCaption == NULL ? _T("") : lpszCaption);

	m_lstCaptionIndexes.AddTail(nIndex);

	m_bInAddingCaption = FALSE;
}
//**************************************************************************
void CBCGPListBox::AddSeparator()
{
	AddCaption(NULL);
}
//**************************************************************************
void CBCGPListBox::CleanUp()
{
	ResetContent();

	m_sizeImage = CSize(0, 0);
	m_hImageList = NULL;
	m_nLastScrollPos = -1;
	m_bHasColorBars = FALSE;
	m_nIconCount = 0;

	m_lstCaptionIndexes.RemoveAll();

	CBCGPTooltipManager::DeleteToolTip(m_pToolTip);
}
//**************************************************************************
BOOL CBCGPListBox::IsCaptionItem(int nIndex) const
{
	return m_lstCaptionIndexes.Find(nIndex) != 0;
}
//**************************************************************************
BOOL CBCGPListBox::IsSeparatorItem(int nIndex) const
{
	return IsCaptionItem(nIndex);
}
//**************************************************************************
void CBCGPListBox::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if ((GetStyle() & LBS_NOSEL) == LBS_NOSEL)
	{
		CListBox::OnLButtonDown(nFlags, point);
		return;
	}

	if (!m_lstCaptionIndexes.IsEmpty() && HitTest(point) == -1)
	{
		return;
	}
	
	CListBox::OnLButtonDown(nFlags, point);

	m_nClickedItem = HitTest(point);

	BOOL bItemHighlighting = (m_bItemHighlighting && m_bVisualManagerStyle);
	if (!bItemHighlighting)
	{
		RedrawWindow();
	}
}
//**************************************************************************
void CBCGPListBox::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if ((GetStyle() & LBS_NOSEL) == LBS_NOSEL)
	{
		CListBox::OnLButtonUp(nFlags, point);
		return;
	}

	HWND hwndThis = GetSafeHwnd();

	CListBox::OnLButtonUp(nFlags, point);

	if (!::IsWindow(hwndThis))
	{
		return;
	}

	if (m_nClickedItem >= 0)
	{
		RedrawWindow();
	}

	if (IsSeparatorItem(m_nClickedItem))
	{
		return;
	}

	int nClickedItem = m_nClickedItem;
	m_nClickedItem = -1;

	BOOL bPin = FALSE;

	if (nClickedItem >= 0 && nClickedItem == HitTest(point, &bPin))
	{
		if (bPin)
		{
			OnClickPin(nClickedItem);
		}
		else
		{
			OnClickItem(nClickedItem);
		}
	}
}
//**************************************************************************
void CBCGPListBox::OnClickPin(int nClickedItem)
{
	if (GetOwner ()->GetSafeHwnd() != NULL)
	{
		GetOwner ()->SendMessage(BCGM_ON_CLICK_LISTBOX_PIN, GetDlgCtrlID (), LPARAM (nClickedItem));
	}
}
//**************************************************************************
void CBCGPListBox::OnCancelMode() 
{
	CListBox::OnCancelMode();
	m_nClickedItem = -1;
}
//**************************************************************************
void CBCGPListBox::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	const int nPrevSel = GetCurSel();

	CListBox::OnKeyDown(nChar, nRepCnt, nFlags);

	if ((GetStyle() & LBS_NOSEL) == LBS_NOSEL)
	{
		return;
	}

	if (m_lstCaptionIndexes.IsEmpty())
	{
		return;
	}

	const int nCurSel = GetCurSel();
	if (!IsCaptionItem(nCurSel) || nCurSel == nPrevSel)
	{
		return;
	}

	BOOL bFindNext = FALSE;

	switch (nChar)
	{
	case VK_HOME:
	case VK_DOWN:
	case VK_NEXT:
		bFindNext = TRUE;
		break;
	}

	int nNewSel = -1;

	if (bFindNext)
	{
		for(int i = nCurSel + 1; i < GetCount(); i++)
		{
			if (!IsCaptionItem(i))
			{
				nNewSel = i;
				break;
			}
		}
	}
	else
	{
		for(int i = nCurSel - 1; i >= 0; i--)
		{
			if (!IsCaptionItem(i))
			{
				nNewSel = i;
				break;
			}
		}
	}

	SetCurSel(nNewSel != -1 ? nNewSel : nPrevSel);

	RedrawWindow();
}
//**************************************************************************
BOOL CBCGPListBox::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	if (CListBox::OnMouseWheel(nFlags, zDelta, pt))
	{
		RedrawWindow();
		return TRUE;
	}

	return FALSE;
}
//**************************************************************************
void CBCGPListBox::EnableItemDescription(BOOL bEnable, int nRows)
{
	m_nDescrRows = bEnable ? nRows : 0;

	if (GetSafeHwnd() != NULL && (GetStyle() & (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS)) == (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS))
	{
		SetItemHeight(0, GetItemMinHeight());
	}
}
//**************************************************************************
void CBCGPListBox::EnableItemHighlighting(BOOL bEnable)
{
	m_bItemHighlighting = bEnable;
}
//**************************************************************************
void CBCGPListBox::SetItemExtraHeight(int nExtraHeight)
{
	m_nItemExtraHeight = nExtraHeight;

	if (GetSafeHwnd() != NULL && (GetStyle() & (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS)) == (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS))
	{
		SetItemHeight(0, GetItemMinHeight());
	}
}
//**************************************************************************
BOOL CBCGPListBox::PreTranslateMessage(MSG* pMsg) 
{
   	switch (pMsg->message)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_NCLBUTTONDOWN:
	case WM_NCRBUTTONDOWN:
	case WM_NCMBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCRBUTTONUP:
	case WM_NCMBUTTONUP:
	case WM_MOUSEMOVE:
		if (m_pToolTip->GetSafeHwnd () != NULL)
		{
			m_pToolTip->RelayEvent(pMsg);
		}
		break;
	}

	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && GetFocus()->GetSafeHwnd() == GetSafeHwnd())
	{
		if (OnReturnKey())
		{
			return TRUE;
		}
	}
	
	return CListBox::PreTranslateMessage(pMsg);
}
//**************************************************************************
void CBCGPListBox::SetPropertySheetNavigator(BOOL bSet)
{
	m_bPropertySheetNavigator = bSet;
}
//**************************************************************************
void CBCGPListBox::EnablePins(BOOL bEnable)
{
	m_bPins = bEnable;

	if (GetSafeHwnd() != NULL)
	{
		RedrawWindow();
	}
}
//**********************************************************************************************
void CBCGPListBox::SetItemPinned(int nIndex, BOOL bSet, BOOL bRedraw)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		pState->m_bIsPinned = bSet;
	}

	if (bRedraw && GetSafeHwnd() != NULL)
	{
		CRect rectItem;
		GetItemRect(nIndex, rectItem);

		RedrawWindow (rectItem);
	}
}
//**********************************************************************************************
BOOL CBCGPListBox::IsItemPinned(int nIndex)
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_bIsPinned;
	}
	
	return FALSE;
}
//**********************************************************************************************
void CBCGPListBox::ResetPins()
{
	for (int i = 0; i < GetCount(); i++)
	{
		SetItemPinned(i, FALSE, FALSE);
	}
}
//**************************************************************************
void CBCGPListBox::Enable(int nIndex, BOOL bEnabled/* = TRUE*/, BOOL bRedraw/* = TRUE*/)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		pState->m_bIsEnabled = bEnabled;
	}

	if (bRedraw && GetSafeHwnd() != NULL)
	{
		CRect rectItem;
		GetItemRect(nIndex, rectItem);

		RedrawWindow (rectItem);
	}
}
//**************************************************************************
BOOL CBCGPListBox::IsEnabled(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_bIsEnabled;
	}

	return TRUE;
}
//**********************************************************************************************
void CBCGPListBox::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	CListBox::OnHScroll(nSBCode, nPos, pScrollBar);
	RedrawWindow();
}
//**********************************************************************************************
void CBCGPListBox::OnNcPaint() 
{
	CListBox::OnNcPaint();

	const BOOL bHasBorder = (GetExStyle () & WS_EX_CLIENTEDGE) || (GetStyle () & WS_BORDER);

	if (bHasBorder && (m_bVisualManagerStyle || m_bOnGlass))
	{
		CBCGPDrawOnGlass dog (m_bOnGlass);
		CBCGPVisualManager::GetInstance ()->OnDrawControlBorder (this);
	}
}
//*********************************************************************************************
LRESULT CBCGPListBox::OnPrintClient(WPARAM wp, LPARAM lp)
{
	if (!(GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED)))
	{
		return Default();
	}

	if (lp & PRF_CLIENT)
	{
		CDC* pDC = CDC::FromHandle((HDC) wp);
		ASSERT_VALID(pDC);

		OnDraw(pDC);
	}

	return 0;
}
//*********************************************************************************************
int CBCGPListBox::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CListBox::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if ((GetStyle() & (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS)) == (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS))
	{
		SetItemHeight(0, GetItemMinHeight());
	}

	return 0;
}
//*********************************************************************************************
void CBCGPListBox::PreSubclassWindow() 
{
	CListBox::PreSubclassWindow();

	if ((GetStyle() & (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS)) == (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS))
	{
		SetItemHeight(0, GetItemMinHeight());
	}
}
//*********************************************************************************************
int CBCGPListBox::GetItemMinHeight()
{
	if (m_nTextHeight <= 0)
	{
		CClientDC dc(this);
		
		CFont* pOldFont = dc.SelectObject(&globalData.fontRegular);
		ASSERT(pOldFont != NULL);
		
		TEXTMETRIC tm;
		dc.GetTextMetrics (&tm);
		
		m_nTextHeight = tm.tmHeight;
		
		dc.SelectObject (pOldFont);
	}

	int nVertMargin = m_nDescrRows == 0 ? 0 : globalUtils.ScaleByDPI(4);
	int nCheckBoxHeight = !m_bHasCheckBoxes ? 0 : (CBCGPVisualManager::GetInstance ()->GetCheckRadioDefaultSize().cy + 4);
	int nImageHeight = m_sizeImage.cy == 0 ? 0 : m_sizeImage.cy + globalUtils.ScaleByDPI(4);
	int nTextHeight = 0;

	if (m_bPins)
	{
		int nPinHeight = max(CBCGPVisualManager::GetInstance ()->GetPinSize(TRUE).cy, CBCGPVisualManager::GetInstance ()->GetPinSize(FALSE).cy);
		nImageHeight = max(nImageHeight, nPinHeight);
	}

	const int nTextRowHeight = m_nTextHeight;
	
	if (IsPropertySheetNavigator())
	{
		nTextHeight = nTextRowHeight * 9 / 5;
	}
	else
	{
		if (m_nDescrRows == 0 || m_bInAddingCaption)
		{
			if ((GetStyle() & (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS)) == (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS))
			{
				nTextHeight = nTextRowHeight + nVertMargin;
			}
			else
			{
				nTextHeight = nTextRowHeight + nVertMargin + 2;
			}
		}
		else
		{
			nTextHeight = (nTextRowHeight + nVertMargin) * (m_nDescrRows + 1);
		}
	}
	
	int nExtraHeight = m_nItemExtraHeight;
	if (m_bVisualManagerStyle)
	{
		m_nVMExtraHeight = CBCGPVisualManager::GetInstance()->GetListBoxItemExtraHeight(this);
		nExtraHeight += m_nVMExtraHeight;
	}

	return max(nCheckBoxHeight, max(nImageHeight, nTextHeight)) + m_nItemExtraHeight + nExtraHeight;
}
//*********************************************************************************************
void CBCGPListBox::DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct)
{
	DELETEITEMSTRUCT deleteItem;
	memcpy(&deleteItem, lpDeleteItemStruct, sizeof(DELETEITEMSTRUCT));
	
	if (deleteItem.itemData == 0)
	{
		LRESULT lResult = DefWindowProc(LB_GETITEMDATA, deleteItem.itemID, 0);
		if (lResult != LB_ERR)
		{
			deleteItem.itemData = (UINT)lResult;
		}
	}
	
	if ((GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED)) != 0 &&
		deleteItem.itemData != 0 && deleteItem.itemData != LB_ERR)
	{
		BCGP_LB_ITEM_DATA* pState = (BCGP_LB_ITEM_DATA*)deleteItem.itemData;
		if (pState->m_hIcon != NULL)
		{
			m_nIconCount--;
		}

		deleteItem.itemData = pState->m_dwData;
		delete pState;
	}
	
	CListBox::DeleteItem(&deleteItem);
}
//*********************************************************************************************
LRESULT CBCGPListBox::OnLBAddString(WPARAM wParam, LPARAM lParam)
{
	if (!(GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED)))
	{
		return DefWindowProc(LB_ADDSTRING, wParam, lParam);
	}

	BCGP_LB_ITEM_DATA* pState = NULL;
	
	if (!(GetStyle() & LBS_HASSTRINGS))
	{
		pState = new BCGP_LB_ITEM_DATA;
		pState->m_dwData = lParam;
		lParam = (LPARAM)pState;
	}
	
	LRESULT lResult = DefWindowProc(LB_ADDSTRING, wParam, lParam);
	
	if (lResult == LB_ERR && pState != NULL)
	{
		delete pState;
	}
	
	m_bRebuildTooltips = TRUE;
	return lResult;
}
//*********************************************************************************************
LRESULT CBCGPListBox::OnLBInsertString(WPARAM wParam, LPARAM lParam)
{
	if (!(GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED)))
	{
		return DefWindowProc(LB_INSERTSTRING, wParam, lParam);
	}
	
	BCGP_LB_ITEM_DATA* pState = NULL;
	
	if (!(GetStyle() & LBS_HASSTRINGS))
	{
		pState = new BCGP_LB_ITEM_DATA;
		pState->m_dwData = lParam;
		lParam = (LPARAM)pState;
	}
	
	LRESULT lResult = DefWindowProc(LB_INSERTSTRING, wParam, lParam);
	
	if (lResult == LB_ERR && pState != NULL)
	{
		delete pState;
	}

	m_bRebuildTooltips = TRUE;
	return lResult;
}
//*********************************************************************************************
LRESULT CBCGPListBox::OnLBGetItemData(WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = DefWindowProc(LB_GETITEMDATA, wParam, lParam);

	if (!(GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED)))
	{
		return lResult;
	}

	if (lResult != LB_ERR)
	{
		BCGP_LB_ITEM_DATA* pState = (BCGP_LB_ITEM_DATA*)lResult;
		
		if (pState == NULL)
		{
			return 0;
		}
		
		lResult = pState->m_dwData;
	}
	
	return lResult;
}
//*********************************************************************************************
LRESULT CBCGPListBox::OnLBSetItemData(WPARAM wParam, LPARAM lParam)
{
	if (!(GetStyle() & (LBS_OWNERDRAWVARIABLE | LBS_OWNERDRAWFIXED)))
	{
		return Default();
	}

	LRESULT lResult = DefWindowProc(LB_GETITEMDATA, wParam, 0);

	if (lResult != LB_ERR)
	{
		BCGP_LB_ITEM_DATA* pState = (BCGP_LB_ITEM_DATA*)lResult;
		if (pState == NULL)
		{
			pState = new BCGP_LB_ITEM_DATA;
		}
		
		pState->m_dwData = lParam;
		lResult = DefWindowProc(LB_SETITEMDATA, wParam, (LPARAM)pState);
		
		if (lResult == LB_ERR)
		{
			delete pState;
		}
	}
	
	return lResult;
}
//*****************************************************************************
LRESULT CBCGPListBox::OnSetFont(WPARAM wParam, LPARAM)
{
	m_hFont = (HFONT) wParam;

	if (m_hFont == 0)
	{
		m_nTextHeight = -1;
	}
	else
	{
		CClientDC dc(this);
		
		CFont* pOldFont = dc.SelectObject(CFont::FromHandle(m_hFont));
		ASSERT(pOldFont != NULL);
		
		TEXTMETRIC tm;
		dc.GetTextMetrics (&tm);
		
		m_nTextHeight = tm.tmHeight;
		
		dc.SelectObject(pOldFont);
	}

	if (GetSafeHwnd() != NULL && (GetStyle() & (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS)) == (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS))
	{
		SetItemHeight(0, GetItemMinHeight());
	}

	return Default();
}
//*****************************************************************************
LRESULT CBCGPListBox::OnLBSetTabstops(WPARAM wp, LPARAM lp)
{
	m_arTabStops.RemoveAll();

	int nTabStops = (int)wp;

	if (nTabStops <= 0)
	{
		m_arTabStops.Add(BCGP_DEFAULT_TABS_STOP);
	}
	else
	{
		LPINT arTabStops = (LPINT)lp;

		for (int i = 0; i < nTabStops; i++)
		{
			m_arTabStops.Add(arTabStops[i]);
		}
	}

	return Default();
}
//*****************************************************************************
LRESULT CBCGPListBox::OnLBSetCurSel(WPARAM/* wp*/, LPARAM /*lp*/)
{
	LRESULT lResult = Default();

	RedrawWindow();
	return lResult;
}
//***********************************************************************************************************
BOOL CBCGPListBox::OnTTNeedTipText (UINT /*id*/, NMHDR* pNMH, LRESULT* /*pResult*/)
{
	static CString strTipText;

	int nItem = ((int)pNMH->idFrom) - 1;
    if (nItem < 0)
	{
        return FALSE;
	}
	
	LPCTSTR lpszTT = GetItemToolTip(nItem);
	if (lpszTT == NULL)
	{
		return FALSE;
	}

	strTipText = lpszTT;

	LPNMTTDISPINFO	pTTDispInfo	= (LPNMTTDISPINFO) pNMH;
	ASSERT((pTTDispInfo->uFlags & TTF_IDISHWND) == 0);

	LPCTSTR lpszDescr = GetItemToolTipDescription(nItem);
	
	if (lpszDescr != NULL)
	{
		CBCGPToolTipCtrl* pToolTip = DYNAMIC_DOWNCAST(CBCGPToolTipCtrl, m_pToolTip);
		if (pToolTip != NULL)
		{
			ASSERT_VALID (pToolTip);
			pToolTip->SetDescription(lpszDescr);
		}
	}
	
	pTTDispInfo->lpszText = const_cast<LPTSTR> ((LPCTSTR)strTipText);

	return TRUE;
}
//**************************************************************************
LRESULT CBCGPListBox::OnBCGUpdateToolTips(WPARAM wp, LPARAM)
{
	UINT nTypes = (UINT) wp;

	if (m_pToolTip->GetSafeHwnd () == NULL)
	{
		return 0;
	}

	if (nTypes & BCGP_TOOLTIP_TYPE_GRID)
	{
		RebuildToolTips();
	}

	return 0;
}
//**************************************************************************
void CBCGPListBox::OnSize(UINT nType, int cx, int cy) 
{
	CListBox::OnSize(nType, cx, cy);
	m_bRebuildTooltips = TRUE;
}
//**************************************************************************
void CBCGPListBox::RebuildToolTips()
{
	CBCGPTooltipManager::DeleteToolTip(m_pToolTip);
	CBCGPTooltipManager::CreateToolTip(m_pToolTip, this, BCGP_TOOLTIP_TYPE_GRID);

	CRect rectClient;
	GetClientRect(rectClient);

	for (int i = 0; i < GetCount(); i++)
	{
		LPCTSTR lpszTT = GetItemToolTip(i);
		if (lpszTT == NULL)
		{
			continue;
		}

		CRect rectItem;
		GetItemRect(i, rectItem);

		if (rectItem.bottom < rectClient.top || rectItem.top > rectClient.bottom)
		{
			continue;
		}

		if (m_bPins && !IsSeparatorItem(i) && !IsCaptionItem(i))
		{
			rectItem.right -= PIN_AREA_WIDTH;
		}

		m_pToolTip->AddTool(this, LPSTR_TEXTCALLBACK, rectItem, i + 1);
	}
}
//************************************************************************
BOOL CBCGPListBox::IsInternalScrollBarThemed() const
{
#ifndef _BCGSUITE_
	return (globalData.m_nThemedScrollBars & BCGP_THEMED_SCROLLBAR_LISTBOX) != 0 && m_bVisualManagerStyle;
#else
	return FALSE;
#endif
}

//////////////////////////////////////////////////////////////////////////
// CBCGPCheckListBox

IMPLEMENT_DYNAMIC(CBCGPCheckListBox, CBCGPListBox)

CBCGPCheckListBox::CBCGPCheckListBox()
{
	m_nCheckStyle = BS_AUTOCHECKBOX;
	m_bHasCheckBoxes = TRUE;
}

CBCGPCheckListBox::~CBCGPCheckListBox()
{
}

BEGIN_MESSAGE_MAP(CBCGPCheckListBox, CBCGPListBox)
	//{{AFX_MSG_MAP(CBCGPCheckListBox)
	ON_WM_LBUTTONDOWN()
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CBCGPCheckListBox::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_SPACE)
	{
		int nIndex = GetCaretIndex();
		
		CWnd* pParent = GetParent();
		ASSERT_VALID(pParent);
		
		if (nIndex != LB_ERR)
		{
			int nModulo = (m_nCheckStyle == BS_AUTO3STATE) ? 3 : 2;

			if (m_nCheckStyle != BS_CHECKBOX && m_nCheckStyle != BS_3STATE)
			{
				if ((GetStyle() & LBS_MULTIPLESEL) != 0)
				{
					if (IsEnabled(nIndex) && IsCheckEnabled(nIndex))
					{
						BOOL bSelected = GetSel(nIndex);
						if (bSelected)
						{
							int nCheck = GetCheck(nIndex);
							nCheck = (nCheck == nModulo) ? nCheck - 1 : nCheck;
							SetCheck(nIndex, (nCheck + 1) % nModulo);
							
							// Inform of check
							pParent->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), CLBN_CHKCHANGE), (LPARAM)m_hWnd);
						}

						SetSel(nIndex, !bSelected);
					}

					return;
				}
				else
				{
					// If there is a selection, the space bar toggles that check,
					// all other keys are the same as a standard listbox.
					
					if (IsEnabled(nIndex) && IsCheckEnabled(nIndex))
					{
						int nCheck = GetCheck(nIndex);
						nCheck = (nCheck == nModulo) ? nCheck - 1 : nCheck;
						
						int nNewCheck = (nCheck + 1) % nModulo;
						SetCheck(nIndex, nNewCheck);
						
						if (GetStyle() & LBS_EXTENDEDSEL)
						{
							// The listbox is a multi-select listbox, and the user
							// clicked on a selected check, so change the check on all
							// of the selected items.
							SetSelectionCheck(nNewCheck);
						}
						else
						{
							int nCurSel = GetCurSel();
							if (nCurSel < 0)
							{
								SetCurSel(nIndex);
							}
						}
						
						// Inform of check
						pParent->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), CLBN_CHKCHANGE), (LPARAM)m_hWnd);
					}
					
					return;
				}
			}
		}
	}
	
	CBCGPListBox::OnKeyDown(nChar, nRepCnt, nFlags);
}
//**********************************************************************************************
void CBCGPCheckListBox::SetCheckStyle(UINT nStyle)
{
	ASSERT(nStyle == 0 || nStyle == BS_CHECKBOX ||
		nStyle == BS_AUTOCHECKBOX || nStyle == BS_AUTO3STATE ||
		nStyle == BS_3STATE);

	m_nCheckStyle = nStyle;
	m_bHasCheckBoxes = (m_nCheckStyle != 0);

	if (GetSafeHwnd() != NULL)
	{
		RedrawWindow();
	}
}
//**********************************************************************************************
void CBCGPCheckListBox::SetCheck(int nIndex, int nCheck, BOOL bRedraw)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		pState->m_nCheck = nCheck;
	}

	if (bRedraw && GetSafeHwnd() != NULL)
	{
		CRect rectItem;
		GetItemRect(nIndex, rectItem);

		RedrawWindow (rectItem);
	}
}
//**********************************************************************************************
int CBCGPCheckListBox::GetCheck(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_nCheck;
	}

	return BST_UNCHECKED;
}
//**********************************************************************************************
void CBCGPCheckListBox::EnableCheck(int nIndex, BOOL bEnabled, BOOL bRedraw)
{
	BCGP_LB_ITEM_DATA* pState = _GetAllocItemData(nIndex);
	if (pState != NULL)
	{
		pState->m_nCheckEnabled = bEnabled;
	}

	if (bRedraw && GetSafeHwnd() != NULL)
	{
		CRect rectItem;
		GetItemRect(nIndex, rectItem);

		RedrawWindow (rectItem);
	}
}
//**********************************************************************************************
BOOL CBCGPCheckListBox::IsCheckEnabled(int nIndex) const
{
	const BCGP_LB_ITEM_DATA* pState = _GetItemData(nIndex);
	if (pState != NULL)
	{
		return pState->m_nCheckEnabled;
	}

	return TRUE;
}
//**********************************************************************************************
void CBCGPCheckListBox::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();
	
	// determine where the click is
	BOOL bInCheck;
	int nIndex = HitTest(point, NULL, &bInCheck);
	
	// if the item is disabled, then eat the click
	if (!IsEnabled(nIndex))
	{
		return;
	}
	
	if (m_nCheckStyle != BS_CHECKBOX && m_nCheckStyle != BS_3STATE && IsCheckEnabled(nIndex))
	{
		// toggle the check mark automatically if the check mark was hit
		if (bInCheck)
		{
			CWnd* pParent = GetParent();
			ASSERT_VALID(pParent);
			
			int nModulo = (m_nCheckStyle == BS_AUTO3STATE) ? 3 : 2;
			int nCheck = GetCheck(nIndex);
			nCheck = (nCheck == nModulo) ? nCheck - 1 : nCheck;

			int nNewCheck = (nCheck + 1) % nModulo;
			SetCheck(nIndex, nNewCheck);
			
			if ((GetStyle() & (LBS_EXTENDEDSEL | LBS_MULTIPLESEL)) && GetSel(nIndex))
			{
				// The listbox is a multi-select listbox, and the user clicked on
				// a selected check, so change the check on all of the selected
				// items.
				SetSelectionCheck(nNewCheck);
			}
			else
			{
				CBCGPListBox::OnLButtonDown(nFlags, point);
			}
			
			// Inform parent of check
			pParent->SendMessage(WM_COMMAND, MAKEWPARAM( GetDlgCtrlID(), CLBN_CHKCHANGE ), (LPARAM)m_hWnd);
			return;
		}
	}
	
	// do default listbox selection logic
	CBCGPListBox::OnLButtonDown( nFlags, point);
}
//**********************************************************************************************
void CBCGPCheckListBox::SetSelectionCheck(int nCheck)
{
	int nSelectedItems = GetSelCount();
	if (nSelectedItems > 0)
	{
		CArray<int,int> rgiSelectedItems;
		rgiSelectedItems.SetSize(nSelectedItems);
		int *piSelectedItems = rgiSelectedItems.GetData(); 

		GetSelItems(nSelectedItems, piSelectedItems);
		
		for (int iSelectedItem = 0; iSelectedItem < nSelectedItems; iSelectedItem++)
		{
			if (IsEnabled (piSelectedItems[iSelectedItem]))
			{
				SetCheck(piSelectedItems[iSelectedItem], nCheck);
			}
		}
	}
}
//**********************************************************************************************
int CBCGPCheckListBox::GetCheckCount() const
{
	int nCheckCount = 0;

	for (int i = 0; i < GetCount (); i++)
	{
		if (GetCheck(i) != 0)
		{
			nCheckCount++;
		}
	}

	return nCheckCount;
}

#ifndef _BCGSUITE_

/////////////////////////////////////////////////////////////////////////////
//CBCGPMDITemplatesListBox window

CBCGPMDITemplatesListBox::CBCGPMDITemplatesListBox()
{
}
//**********************************************************************************************
CDocTemplate* CBCGPMDITemplatesListBox::GetSelectedTemplate() const
{
	if (GetSafeHwnd() == NULL)
	{
		return NULL;
	}
	
	int nSel = GetCurSel();
	if (nSel < 0)
	{
		return NULL;
	}
	
	CDocTemplate* pTemplate = (CDocTemplate*)GetItemData(nSel);
	ASSERT_VALID(pTemplate);
	
	return pTemplate;
}
//**********************************************************************************************
void CBCGPMDITemplatesListBox::FillList()
{
	if (GetSafeHwnd() == NULL)
	{
		return;
	}
	
	ResetContent();
	CleanUp();
	
	SetItemHeight(-1, ::GetSystemMetrics (SM_CYICON) + globalUtils.ScaleByDPI(8));
	
	for (POSITION pos = AfxGetApp()->GetFirstDocTemplatePosition(); pos != NULL;)
	{
		CBCGPMultiDocTemplate* pTemplate = (CBCGPMultiDocTemplate*)DYNAMIC_DOWNCAST(CMultiDocTemplate, AfxGetApp()->GetNextDocTemplate(pos));
		if (pTemplate != NULL)
		{
			ASSERT_VALID(pTemplate);
			
			CString strTypeName;
			if (pTemplate->GetDocString(strTypeName, CDocTemplate::fileNewName) && !strTypeName.IsEmpty())
			{
				int nIndex = AddString(strTypeName);
				
				SetItemData(nIndex, (DWORD_PTR)pTemplate);
				
				HICON hIcon = AfxGetApp()->LoadIcon(pTemplate->GetResId ());
				if (hIcon == NULL)
				{
					hIcon = ::LoadIcon(NULL, IDI_APPLICATION);
				}
				
				if (hIcon != NULL)
				{
					SetItemIcon(nIndex, hIcon);
				}
			}
		}
	}
	
	if (GetCount() > 0)
	{
		SetCurSel(0);
	}
}
#endif


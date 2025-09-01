/**----------------------------------------------------------------------
//@file
//			医用画像用 ワークステーション
//@brief	計測クラス本体
//			[実装部]
//
//
//-------------------------------------------------------------*/
#include "stdafx.h"
#include "AmeMeasurePlugInGUI.h"

#include <algorithm>
#include <AmeDrawLine.h>
#include <AmeBitSeedPaint.h>
#include <AmeVRRenderingInformation.h>
#include <AmePath.h>
#include "AmeImageAttr.h"
#include "AmeViewerAttr.h"
#include "AmeImageViewer.h"
#include "AmeImageViewerFrame.h"
#include "AmePlugInManager.h"
#include "AmeTaskManager.h"
#include "AmeTimeTable.h"
#include "AmeViewerUtil.h"
#include "AmeImageUtil.h"
#include "AmeImageViewerUtil.h"
#include "AmeWidgetUtil.h"
#include "AmeInterfaceUtil.h"
#include "AmeMainController.h"
#include "AmeRegistryFileBase.h"
#include "AmeNotifyParameter.h"
#include "AmeWorkstationErrorCodes.h"
#include "AmeMeasureDrawObject.h"
#include "AmeMeasurePointArrayBase.h"
#include "AmeMeasureLine.h"
#include "AmeMeasureProjectedLine.h"
#include "AmeMeasureTTTG.h"
#include "AmeMeasurePolyLine.h"
#include "AmeMeasureCurve.h"
#include "AmeMeasureAngle.h"
#include "AmeMeasureTwoLineAngle.h"
#include "AmeMeasureProjAngle.h"
#include "AmeMeasureRegionBase.h"
#include "AmeMeasureVolumeBase.h"
#include "AmeMeasureBox.h"
#include "AmeMeasureSphere.h"
#include "AmeMeasureAll.h"
#include "AmeMeasureSizeDialog.h"
#include "AmeMeasureResultDialog.h"
#include "AmeMeasurePlotCanvas.h"
#include "AmeMeasureErrorCodes.h"
#include "AmeRescaleCalculator.h"
#include "AmeSUVCalculator.h"
#include "AmeManagedGroupBox.h"
#include "AmeViewerCornerContainer.h"
#include "AmeShortcutBar.h"
#include "AmeTaskManagerBasicGUI.h"
#include "AmeTaskManagerConsoleGUI.h"
#include "AmeManagedList.h"
#include "AmeMeasureFrameList.h"

#include "AmeObliqueLiveWire.h"
#include "AmeBaseAnalysisPlugInDesktopGUI.h"
#include "AmeDrawUtil.h"
#include "AmeMeasureAngleDialog.h"
#include "AmeGUIProgressManager.h"


extern AmeApplicationAttr* AppAttr;


// 作成可能かビューアかどうかチェック
static bool IsMeasureCreatable( AmeMeasureType type, AmeImageViewer* viewer, bool& bValidScale )
{
	// 計測可能な画像かどうかチェック
	bool bMeasuringScale = false;
	bool bMeasuringValue = false;
	bValidScale = true;
	if ( viewer->GetViewerAttr() != nullptr )
	{
		if ( viewer->GetViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
		{
			bMeasuringValue = true;
		}
		if ( viewer->GetViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT ) )
		{
			bMeasuringScale = true;
		}
		if ( !viewer->GetViewerAttr()->GetImageAttr()->IsValidVoxelScale() )
		{
			bMeasuringScale = false;
			bValidScale = false;
		}
	}
	if ( viewer->GetOverlayViewerAttr() != nullptr )
	{
		if ( viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
		{
			bMeasuringValue = true;
		}
		if ( viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT ) )
		{
			bMeasuringScale = true;
		}
		if ( !viewer->GetOverlayViewerAttr()->GetImageAttr()->IsValidVoxelScale() )
		{
			bMeasuringScale = false;
			bValidScale = false;
		}
	}

	if ( !bMeasuringScale && !bMeasuringValue )
	{
		return false;
	}

	// 種類ごとのビューア可否チェック
	switch ( type )
	{
		case AME_MEASURE_LINE:
			if ( !bMeasuringScale )
			{
				return false;
			}
			// ストレッチ・投影CPR以外はすべてOK
			if ( viewer->GetViewerType() == AME_VIEWER_CPR )
			{
				switch ( viewer->GetViewerAttr()->m_CPRImageType )
				{
					case AME_CPR_STRETCH_H:
					case AME_CPR_STRETCH_V:
					case AME_CPR_PROJECTION:
						return false;
				}
			}
			break;
		case AME_MEASURE_PROJ_LINE:
			/*
			if(!bMeasuringScale){
				return false;
			}
			*/
			// CPRとMPRとVEでは使えない
			if ( viewer->GetViewerType() == AME_VIEWER_CPR || IsPlaneViewer( viewer ) ||
				!((viewer->GetViewerType() == AME_VIEWER_VOLUME || viewer->GetViewerType() == AME_VIEWER_MULTI_3D || viewer->GetViewerType() == AME_VIEWER_SURFACE) && viewer->GetViewerAttr()->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE) )
			{
				return false;
			}
			break;
		case AME_MEASURE_TTTG:
		{
			if ( !bMeasuringScale )
			{
				return false;
			}
			// Only MPR
			if ( !IsPlaneViewer( viewer ) )
			{
				return false;
			}
		}
		break;
		case AME_MEASURE_POLYLINE:
		case AME_MEASURE_CURVE:
			if ( !bMeasuringScale )
			{
				return false;
			}
			// CPRでは使えない
			if ( viewer->GetViewerType() == AME_VIEWER_CPR )
			{
				return false;
			}
			break;
		case AME_MEASURE_ANGLE:
			// CPRでは使えない
			if ( viewer->GetViewerType() == AME_VIEWER_CPR )
			{
				return false;
			}
			else
			{
				// 魚眼ビューでは使えない
				if ( viewer->GetViewerAttr() != nullptr && viewer->GetViewerAttr()->GetRenderingInfo() != nullptr )
				{
					if ( viewer->GetViewerAttr()->GetRenderingInfo()->GetRenderingMode() == AME_VR_RENDERING_VE
						&& viewer->GetViewerAttr()->GetRenderingInfo()->IsFishEye() )
					{

						return false;
					}
				}
			}
			break;
		case AME_MEASURE_PROJ_ANGLE:
			// CPRとMPRとVEでは使えない
			if ( viewer->GetViewerType() == AME_VIEWER_CPR || IsPlaneViewer( viewer ) ||
				!((viewer->GetViewerType() == AME_VIEWER_VOLUME || viewer->GetViewerType() == AME_VIEWER_MULTI_3D || viewer->GetViewerType() == AME_VIEWER_SURFACE) && viewer->GetViewerAttr()->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE) )
			{
				return false;
			}
			break;
		case AME_MEASURE_TWO_LINE_ANGLE:
			// MPR/VRでしか使えない
			if ( !IsPlaneViewer( viewer ) && !((viewer->GetViewerType() == AME_VIEWER_VOLUME || viewer->GetViewerType() == AME_VIEWER_SURFACE) && viewer->GetViewerAttr()->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE) )
			{
				return false;
			}
			break;
		case AME_MEASURE_POINT:
			if ( !bMeasuringValue )
			{
				return false;
			}
			//　マルチ3Dで何を測ればいいのかわからんから、false
			if ( viewer->GetViewerType() == AME_VIEWER_MULTI_3D )
			{
				return false;
			}
			// 投影CPR以外はすべてOK
			if ( viewer->GetViewerType() == AME_VIEWER_CPR )
			{
				switch ( viewer->GetViewerAttr()->m_CPRImageType )
				{
					case AME_CPR_PROJECTION:
						return false;
				}
			}

			break;
		case AME_MEASURE_CUBE:
		case AME_MEASURE_ELLIPSE:
		case AME_MEASURE_ROI:
		case AME_MEASURE_FREEHAND:
			if ( !bMeasuringScale )
			{
				return false;
			}
			// 面積系はMPRでしか使えない
			if ( !IsPlaneViewer( viewer ) )
			{
				return false;
			}
			break;
		case AME_MEASURE_BOX:
		case AME_MEASURE_SPHERE:
			if ( !bMeasuringScale )
			{
				return false;
			}
			if ( !IsPlaneViewer( viewer ) && viewer->GetViewerType() != AME_VIEWER_VOLUME && viewer->GetViewerType() != AME_VIEWER_SURFACE )
			{
				return false;
			}
			if ( viewer->GetViewerType() == AME_VIEWER_XY )
			{
				// not used for original slice
				if ( viewer->GetViewerAttr() != nullptr && viewer->GetViewerAttr()->m_iOriginalSliceSize > 0 )
				{
					return false;
				}
				if ( viewer->GetOverlayViewerAttr() != nullptr && viewer->GetOverlayViewerAttr()->m_iOriginalSliceSize > 0 )
				{
					return false;
				}
			}
			if ( viewer->GetViewerAttr() != nullptr && !viewer->GetViewerAttr()->GetImageAttr()->IsEnableOperation( AmeImageAttr::ENABLE_TILT_MEASUREMENT ) )
			{
				return false;
			}
			if ( viewer->GetOverlayViewerAttr() != nullptr && !viewer->GetOverlayViewerAttr()->GetImageAttr()->IsEnableOperation( AmeImageAttr::ENABLE_TILT_MEASUREMENT ) )
			{
				return false;
			}
			break;
		case AME_MEASURE_CLOSEST:
		case AME_MEASURE_DIAMETER:
			if ( !bMeasuringScale )
			{
				return false;
			}
			switch ( viewer->GetViewerType() )
			{
				case AME_VIEWER_XY:
				case AME_VIEWER_XZ:
				case AME_VIEWER_YZ:
				case AME_VIEWER_OBLIQUE:
				case AME_VIEWER_VOLUME:
					return true;
				default:
					return false;
			}
			break;
		case AME_MEASURE_NONE:
			return false;
		default:
			//AmeAssert(0);
			return false;
	}
	return true;
}

// 重ね合わせViewerAttr取得
static AmeViewerAttr* GetOverlayViewerAttr( AmeViewerAttr* pViewerAttr )
{
	if ( pViewerAttr == nullptr )
	{
		return nullptr;
	}
	AmeImageViewer* viewer = pViewerAttr->GetViewer();
	if ( viewer == nullptr )
	{
		return pViewerAttr;
	}
	return viewer->GetOverlayViewerAttr();
}

PNW_DEFMAP( AmeMeasurePlugInGUI ) AmeMeasurePlugInGUIMap[] =
{
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_MEASURE_TYPE, AmeMeasurePlugInGUI::onCmdMeasureType ),
	//PNW_MAPFUNC(EVT_EXECUTE,	AmeMeasurePlugInGUI::ID_MEASURE_ANGLE_TYPE,	AmeMeasurePlugInGUI::onCmdMeasureAngleType),	
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_DELETE_POINT, AmeMeasurePlugInGUI::onCmdDeletePoint ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_FINISH_COPY, AmeMeasurePlugInGUI::onCmdFinishCopy ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_CANCEL_COPY, AmeMeasurePlugInGUI::onCmdCancelCopy ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_FINISH_NOW, AmeMeasurePlugInGUI::onCmdFinishNow ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_BACK_NOW, AmeMeasurePlugInGUI::onCmdBackNow ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_COPY_TO_ALL, AmeMeasurePlugInGUI::onCmdCopyToAll ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_INPUT_SIZE, AmeMeasurePlugInGUI::onCmdInputSize ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_INPUT_ANGLE, AmeMeasurePlugInGUI::onCmdInputAngle ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_RESULT, AmeMeasurePlugInGUI::onCmdResult ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_MEASURE_ALL, AmeMeasurePlugInGUI::onCmdMeasureAll ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_MEASURE_SINGLE, AmeMeasurePlugInGUI::onCmdMeasureSingle ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_RESCALE, AmeMeasurePlugInGUI::onCmdRescale ),
	PNW_MAPFUNC( EVT_CHANGED, AmeMeasurePlugInGUI::ID_RESCALE_TYPE, AmeMeasurePlugInGUI::onCmdRescaleType ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_TIME_CURVE, AmeMeasurePlugInGUI::onCmdTimeCurve ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_FORE_COLOR_IN, AmeMeasurePlugInGUI::onCmdForeColor ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_FORE_COLOR, AmeMeasurePlugInGUI::onCmdForeColor ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_BACK_COLOR_IN, AmeMeasurePlugInGUI::onCmdBackColor ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_BACK_COLOR, AmeMeasurePlugInGUI::onCmdBackColor ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_CP_COLOR_IN, AmeMeasurePlugInGUI::onCmdCpColor ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_CP_COLOR, AmeMeasurePlugInGUI::onCmdCpColor ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_TEXT_FONT, AmeMeasurePlugInGUI::onCmdFont ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_FONT_SIZE, AmeMeasurePlugInGUI::onCmdFontSize ),
	PNW_MAPFUNC( EVT_CHANGED, AmeMeasurePlugInGUI::ID_TEXT_BACK_CHECK, AmeMeasurePlugInGUI::onCmdTextBackCheck ),
	PNW_MAPFUNC( EVT_CHANGED, AmeMeasurePlugInGUI::ID_CHCURSOR_SIZE, AmeMeasurePlugInGUI::onCmdCursorSize ),
	PNW_MAPFUNC( EVT_CHANGED, AmeMeasurePlugInGUI::ID_SHOW_PROJ_WARNING, AmeMeasurePlugInGUI::onCmdShowProjWarningCheck ),
	PNW_MAPFUNC( EVT_CHANGED, AmeMeasurePlugInGUI::ID_SHOW_4POINT_WARNING, AmeMeasurePlugInGUI::onCmdShow4PointWarningCheck ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_LINE_OPTION, AmeMeasurePlugInGUI::onCmdLineOption ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_LINE_OPTION_MENU, AmeMeasurePlugInGUI::onCmdLineOptionMenu ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_TEXT_FRAME_CHECK, AmeMeasurePlugInGUI::onCmdTextFrameCheck ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_SHORT_CUT, AmeMeasurePlugInGUI::onCmdShortCut ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_EXECUTE_CLOSEST, AmeMeasurePlugInGUI::onCmdExecuteClosest ),
	PNW_MAPFUNC( EVT_EXECUTE, AmeMeasurePlugInGUI::ID_EXECUTE_DIAMETER, AmeMeasurePlugInGUI::onCmdExecuteDiameter ),
	PNW_MAPFUNC(EVT_EXECUTE, AmeMeasurePlugInGUI::ID_DELETE_CLOSEST_BOX_ROI, AmeMeasurePlugInGUI::onCmdDeleteClosestBoxROI),
	PNW_MAPFUNC(EVT_EXECUTE, AmeMeasurePlugInGUI::ID_DELETE_CLOSEST_RECT_ROI, AmeMeasurePlugInGUI::onCmdDeleteClosestRectROI),
};


PNW_IMPLEMENT( AmeMeasurePlugInGUI, AmeMeasurePlugInGUIMap );


// コンストラクタ
AmeMeasurePlugInGUI::AmeMeasurePlugInGUI( AmeBasePlugInEngine* pEngineBase )
	: AmeBaseMousePlugInDesktopGUI( pEngineBase ),
	m_frameMeasureList( nullptr )
{
	m_CurrentType = AME_MEASURE_NONE;
	m_iGrab = -1;
	m_bNowCreating = false;
	m_bNowCopying = false;
	m_iCreatingViewerAttrID = -1;
	m_iCreatingImageAttrID = -1;
	m_bFailedToCreate = false;

	m_iDialogFramePositionID = -1;
	m_pCopiedMeasure = nullptr;
	m_pObliqueLiveWire = new AmeObliqueLiveWire;
	m_iCopiedImageAttrID = -1;
	m_CopiedSize = AmeFloat3D::Zero();

	//色設定
	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		AmeMeasureColorSetting* _cs = new AmeMeasureColorSetting();
		_cs->Init();
		m_ColorSetting.push_back( _cs );
	}

	m_pnwMeasureMethodGroup = NULL;
	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		m_pnwMeasureIcon[i] = NULL;
		m_pnwMeasureMethodBtn[i] = NULL;
		m_pnwShortCutBtn[i] = NULL;
		m_pnwMeasureShortcutIcon[i] = NULL;
	}
	m_pnwMeasureAngleBtn = NULL;
	m_pnwMeasureLineBtn = NULL;
	m_pnwMeasurePolylineBtn = NULL;
	//m_pnwOperationFrame = nullptr;
	m_pnwMeasureDetailsFrame = nullptr;
	m_pnwMeasureExecuteClosestButton = nullptr;

	m_pnwCurrentCursor = NULL;
	m_pnwCopyToAllButton = NULL;
	m_pnwInputSizeButton = NULL;
	m_pnwGraphButton = NULL;
	m_pResultDialog = NULL;
	m_pnwMeasureAngleTypeFrame = NULL;
	m_pnwMeasureSettingFrame = NULL;
	m_pnwMeasureLineTypeFrame = NULL;
	m_pnwMeasurePolylineTypeFrame = NULL;
	m_pnwRescaleFrame = NULL;
	m_pnwRescaleCombo = NULL;
	m_pnwRescaleButton = NULL;
	m_pnwLineOption = NULL;
	m_pnwCurveOption = NULL;

	m_pnwTextFontName = NULL;
	m_pnwTextFontCombo = NULL;
	m_pnwMeasureTextColor = NULL;
	m_pnwMeasureChTextColor = NULL;
	m_pnwMeasureBackColor = NULL;
	m_pnwMeasureBackColorButton = NULL;
	m_pnwMeasureCpColor = NULL;
	m_pnwTextBackCheck = NULL;
	m_pnwTextFrameCheck = NULL;
	m_pnwShowProjWarningCheck = NULL;
	m_pnwShow4PointWarningCheck = NULL;
	m_pnwCHCursorSlider = NULL;
	for ( int i = 0; i < STEP_SIZE_NUM; i++ )
	{
		m_pnwStepButton[i] = NULL;
	}
	m_pnwStepLengthFrame = NULL;

	m_pGraphIcon = NULL;
	m_pInputSizeIcon = NULL;
	m_pCopyToAllIcon = NULL;

	m_StepSize[STEP_SIZE_NONE] = -1;
	m_StepSize[STEP_SIZE_25] = 2.5;
	m_StepSize[STEP_SIZE_50] = 5;
	m_StepSize[STEP_SIZE_100] = 10;
	m_StepSize[STEP_SIZE_500] = 50;

	m_bIntegrateHistogram = false;
	for ( int i = 0; i < MODALITY_TYPE_NUM; i++ )
	{
		m_iHistogramStep[i] = 50;
	}
	m_iHistogramStepSUV = 0.010f;

	m_fHistogramStepGY = 0.01f;

	m_Setting.SetPlugInGUI( this );
	PnwColor _color = AppAttr->GetAppColor( AME_COLOR_LABELCOLOR );
	m_ScaleColor = AmeRGBColor( _color & 0xff, _color >> 8 & 0xff, _color >> 16 & 0xff );
}


// デストラクタ
AmeMeasurePlugInGUI::~AmeMeasurePlugInGUI()
{
	ResetLiveWire();

	if ( m_pResultDialog != nullptr )
	{
		delete m_pResultDialog;
		m_pResultDialog = nullptr;
	}

	delete m_pObliqueLiveWire;
	m_pObliqueLiveWire = nullptr;

	if ( m_pCopiedMeasure != nullptr )
	{
		delete m_pCopiedMeasure;
		m_pCopiedMeasure = nullptr;
	}

	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		delete m_pnwMeasureIcon[i];
		delete m_pnwMeasureShortcutIcon[i];
	}

	//色設定
	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		delete m_ColorSetting[i];
	}

	delete m_pGraphIcon;
	delete m_pInputSizeIcon;
	delete m_pCopyToAllIcon;
}


void AmeMeasurePlugInGUI::InitializePlugInGUI()
{
	m_pEngine = (AmeMeasurePlugInEngine*)GetEngine();
}


// 設定パネルを返す。
AmeBasePlugInSettingGUI* AmeMeasurePlugInGUI::GetSettingFrame()
{
	return &m_Setting;
}


void AmeMeasurePlugInGUI::CreateIcon()
{
	// アイコンの生成
	{
		if ( m_pnwMeasureIcon[1] == NULL )
		{
			m_pnwMeasureIcon[1] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE2" ) );
		}
		if ( m_pnwMeasureIcon[2] == NULL )
		{
			m_pnwMeasureIcon[2] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE3" ) );
		}
		if ( m_pnwMeasureIcon[8] == NULL )
		{
			m_pnwMeasureIcon[8] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE9" ) );
		}
		if ( m_pnwMeasureIcon[9] == NULL )
		{
			m_pnwMeasureIcon[9] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE10" ) );
		}
		if ( m_pnwMeasureIcon[10] == NULL )
		{
			m_pnwMeasureIcon[10] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE11" ) );
		}
		if ( m_pnwMeasureIcon[11] == NULL )
		{
			m_pnwMeasureIcon[11] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE12" ) );
		}
		if ( m_pnwMeasureIcon[12] == NULL )
		{
			m_pnwMeasureIcon[12] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE13" ) );
		}
		if ( m_pnwMeasureIcon[13] == NULL )
		{
			m_pnwMeasureIcon[13] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE14" ) );
		}
		if ( m_pnwMeasureIcon[AME_MEASURE_TTTG] == NULL )
		{
			m_pnwMeasureIcon[AME_MEASURE_TTTG] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE15" ) );
		}
		if ( m_pnwMeasureIcon[AME_MEASURE_CLOSEST] == NULL )
		{
			m_pnwMeasureIcon[AME_MEASURE_CLOSEST] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE16" ) );
		}
		if ( m_pnwMeasureIcon[AME_MEASURE_DIAMETER] == NULL )
		{
			m_pnwMeasureIcon[AME_MEASURE_DIAMETER] = AmeLoadPlugInIcon( GetPlugInName(), AmeString::Format( L"AME_ICON_MEASURE17" ) );
		}
		if ( m_pGraphIcon == NULL )
		{
			m_pGraphIcon = AmeLoadPlugInIcon( GetPlugInName(), L"AME_ICON_GRAPH" );
		}
		if ( m_pInputSizeIcon == NULL )
		{
			m_pInputSizeIcon = AmeLoadPlugInIcon( GetPlugInName(), L"AME_ICON_ROI_INPUT_SIZE" );
		}
		if ( m_pCopyToAllIcon == NULL )
		{
			m_pCopyToAllIcon = AmeLoadPlugInIcon( GetPlugInName(), L"AME_ICON_COPY_TO_ALL" );
		}
	}
}


Pnw::PnwBitmap* AmeMeasurePlugInGUI::GetIcon( AmeMeasureType type )
{
	CreateIcon();

	switch ( type )
	{
		case AME_MEASURE_LINE: return AppAttr->GetAppIcon( AME_ICON_MEASURE_LINE );
		case AME_MEASURE_POLYLINE: return m_pnwMeasureIcon[AME_MEASURE_POLYLINE];
		case AME_MEASURE_ANGLE: return m_pnwMeasureIcon[AME_MEASURE_ANGLE];
		case AME_MEASURE_CUBE: return AppAttr->GetAppIcon( AME_ICON_MEASURE_RECTANGLE );
		case AME_MEASURE_ELLIPSE: return AppAttr->GetAppIcon( AME_ICON_MEASURE_ELLIPSE );
		case AME_MEASURE_ROI: return AppAttr->GetAppIcon( AME_ICON_MEASURE_POLYGON );
		case AME_MEASURE_FREEHAND: return AppAttr->GetAppIcon( AME_ICON_MEASURE_FREEHAND );
		case AME_MEASURE_POINT: return AppAttr->GetAppIcon( AME_ICON_MEASURE_POINT );
		case AME_MEASURE_BOX: return m_pnwMeasureIcon[AME_MEASURE_BOX];
		case AME_MEASURE_SPHERE: return m_pnwMeasureIcon[AME_MEASURE_SPHERE];
		case AME_MEASURE_PROJ_ANGLE: return m_pnwMeasureIcon[AME_MEASURE_PROJ_ANGLE];
		case AME_MEASURE_TWO_LINE_ANGLE: return m_pnwMeasureIcon[AME_MEASURE_TWO_LINE_ANGLE];
		case AME_MEASURE_PROJ_LINE: return m_pnwMeasureIcon[AME_MEASURE_PROJ_LINE];
		case AME_MEASURE_CURVE: return m_pnwMeasureIcon[AME_MEASURE_CURVE];
		case AME_MEASURE_TTTG: return m_pnwMeasureIcon[AME_MEASURE_TTTG];
		case AME_MEASURE_CLOSEST: return m_pnwMeasureIcon[AME_MEASURE_CLOSEST];
		case AME_MEASURE_DIAMETER: return m_pnwMeasureIcon[AME_MEASURE_DIAMETER];
	}
	return nullptr;
}


// ツールフレーム
PnwFrame* AmeMeasurePlugInGUI::CreateToolFrame( PnwFrame* pRoot, std::vector<AmeManagedGroupBoxElement>& grouplist )
{
	std::vector<AmeManagedGroupBox*> read_registry_targets;


	PnwVerticalFrame* pTopFrame = new PnwVerticalFrame( pRoot, LAYOUT_FILL_X | LAYOUT_FILL_Y, 0,0,0,0, 0,0,0,0, 0,0 );

	// アイコンの生成
	{
		CreateIcon();
	}

	// 計測タイプ
	{
		AmeManagedGroupBox* group = new AmeManagedGroupBox( pTopFrame, GetPlugInName(), L"MeasureMethod", AppAttr->GetResStr( "AME_MEASURE_METHOD" ), LAYOUT_FILL_X );
		read_registry_targets.push_back( group );
		grouplist.push_back( AmeManagedGroupBoxElement( true, group ) );
		m_pnwMeasureMethodGroup = group;

		{
			PnwVerticalFrame* vframe = new PnwVerticalFrame( group, LAYOUT_FILL_X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );

			{
				PnwHorizontalFrame* hframe = new PnwHorizontalFrame( vframe, LAYOUT_FILL_X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
				m_pnwMeasureLineBtn = new PnwButton( hframe, AppAttr->GetResStr( "AME_COMMON_MEASURE_METHOD_LINE" ), AppAttr->GetAppIcon( AME_ICON_MEASURE_LINE ), this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureLineBtn->SetUserData( IntToPtr( AME_MEASURE_LINE ) );
				m_pnwMeasurePolylineBtn = new PnwButton( hframe, AppAttr->GetResStr( "AME_COMMON_MEASURE_METHOD_POLY_LINE" ), m_pnwMeasureIcon[AME_MEASURE_POLYLINE], this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasurePolylineBtn->SetUserData( (void*)AME_MEASURE_POLYLINE );
				m_pnwMeasureAngleBtn = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_ANGLE" ), m_pnwMeasureIcon[AME_MEASURE_ANGLE], this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureAngleBtn->SetUserData( IntToPtr( AME_MEASURE_ANGLE ) );
				m_pnwMeasureMethodBtn[AME_MEASURE_POINT] = new PnwButton( hframe, AppAttr->GetResStr( "AME_COMMON_MEASURE_METHOD_POINT" ), AppAttr->GetAppIcon( AME_ICON_MEASURE_POINT ), this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureMethodBtn[AME_MEASURE_POINT]->SetUserData( (void*)AME_MEASURE_POINT );
				m_pnwMeasureMethodBtn[AME_MEASURE_CUBE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_CUBE" ), AppAttr->GetAppIcon( AME_ICON_MEASURE_RECTANGLE ), this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureMethodBtn[AME_MEASURE_CUBE]->SetUserData( (void*)AME_MEASURE_CUBE );
			}
			{
				PnwHorizontalFrame* hframe = new PnwHorizontalFrame( vframe, LAYOUT_FILL_X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
				m_pnwMeasureMethodBtn[AME_MEASURE_ELLIPSE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_ELLIPSE" ), AppAttr->GetAppIcon( AME_ICON_MEASURE_ELLIPSE ), this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureMethodBtn[AME_MEASURE_ELLIPSE]->SetUserData( (void*)AME_MEASURE_ELLIPSE );
				m_pnwMeasureMethodBtn[AME_MEASURE_ROI] = new PnwButton( hframe, AppAttr->GetResStr( "AME_COMMON_MEASURE_METHOD_ROI" ), AppAttr->GetAppIcon( AME_ICON_MEASURE_POLYGON ), this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureMethodBtn[AME_MEASURE_ROI]->SetUserData( (void*)AME_MEASURE_ROI );
				m_pnwMeasureMethodBtn[AME_MEASURE_FREEHAND] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_FREEHAND" ), AppAttr->GetAppIcon( AME_ICON_MEASURE_FREEHAND ), this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureMethodBtn[AME_MEASURE_FREEHAND]->SetUserData( (void*)AME_MEASURE_FREEHAND );
				m_pnwMeasureMethodBtn[AME_MEASURE_BOX] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_BOX" ), m_pnwMeasureIcon[AME_MEASURE_BOX], this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureMethodBtn[AME_MEASURE_BOX]->SetUserData( (void*)AME_MEASURE_BOX );
				m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_SPHERE" ), m_pnwMeasureIcon[AME_MEASURE_SPHERE], this, ID_MEASURE_TYPE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_CENTER_X, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
				m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE]->SetUserData( (void*)AME_MEASURE_SPHERE );
			}
			//デザイン
			{
				m_pnwMeasureLineBtn->SetChainStyle( PnwButton::CHAIN_LEFT | PnwButton::CHAIN_TOP );
				m_pnwMeasurePolylineBtn->SetChainStyle( PnwButton::CHAIN_CENTER_X | PnwButton::CHAIN_TOP );
				m_pnwMeasureAngleBtn->SetChainStyle( PnwButton::CHAIN_CENTER_X | PnwButton::CHAIN_TOP );
				m_pnwMeasureMethodBtn[AME_MEASURE_POINT]->SetChainStyle( PnwButton::CHAIN_CENTER_X | PnwButton::CHAIN_TOP );
				m_pnwMeasureMethodBtn[AME_MEASURE_CUBE]->SetChainStyle( PnwButton::CHAIN_RIGHT | PnwButton::CHAIN_TOP );

				m_pnwMeasureMethodBtn[AME_MEASURE_ELLIPSE]->SetChainStyle( PnwButton::CHAIN_LEFT | PnwButton::CHAIN_BOTTOM );
				m_pnwMeasureMethodBtn[AME_MEASURE_ROI]->SetChainStyle( PnwButton::CHAIN_CENTER_X | PnwButton::CHAIN_BOTTOM );
				m_pnwMeasureMethodBtn[AME_MEASURE_FREEHAND]->SetChainStyle( PnwButton::CHAIN_CENTER_X | PnwButton::CHAIN_BOTTOM );
				m_pnwMeasureMethodBtn[AME_MEASURE_BOX]->SetChainStyle( PnwButton::CHAIN_CENTER_X | PnwButton::CHAIN_BOTTOM );
				m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE]->SetChainStyle( PnwButton::CHAIN_RIGHT | PnwButton::CHAIN_BOTTOM );
			}
		}

		{
			PnwHorizontalFrame* hframe = new PnwHorizontalFrame( group, LAYOUT_FILL_X | PACK_UNIFORM_WIDTH );
			new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_MASK_ALL" ), NULL, this, ID_MEASURE_ALL, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_TOP | LAYOUT_CENTER_X, 0, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
			new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_MASK_ONE" ), NULL, this, ID_MEASURE_SINGLE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_TOP | LAYOUT_CENTER_X, 0, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );

			// 全体計測系は、3Dプラグイン、比較3Dのみ使用可能とする。
			AmePlugInGUIInformation info = GetTaskManager()->GetTaskCardPlugInGUI();
			if ( !AmeIs3DViewer( info.m_pPlugInAttr->m_PlugInName ) && info.m_pPlugInAttr->m_PlugInName != L"AME_PG_INTERPRETER3D" )
			{
				hframe->Hide();
			}

			// 全体計測はCDビューアでも利用できない。マスクが使えないので。
			if ( AppAttr->GetStartupMode() == AME_STARTUP_MODE_CD_VIEWER )
			{
				hframe->Hide();
			}
		}
	}

	{
		AmeManagedGroupBox* group = new AmeManagedGroupBox( m_pnwMeasureMethodGroup, GetPlugInName(), L"LineType", AppAttr->GetResStr( "AME_MEASURE_LINE_TYPE" ), LAYOUT_FILL_X );
		group->SetSubGroup( true );
		group->SetEnableHelpBox( false );
		read_registry_targets.push_back( group );
		m_pnwMeasureLineTypeFrame = group;
		m_pnwMeasureLineTypeFrame->Hide();

		PnwHorizontalFrame* hframe = new PnwHorizontalFrame( group, LAYOUT_FILL_X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
		m_pnwMeasureMethodBtn[AME_MEASURE_LINE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_COMMON_MEASURE_METHOD_LINE" ), AppAttr->GetAppIcon( AME_ICON_MEASURE_LINE ), this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_LINE]->SetUserData( IntToPtr( AME_MEASURE_LINE ) );
		m_pnwMeasureMethodBtn[AME_MEASURE_PROJ_LINE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_PROJ_LINE" ), m_pnwMeasureIcon[AME_MEASURE_PROJ_LINE], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_PROJ_LINE]->SetUserData( IntToPtr( AME_MEASURE_PROJ_LINE ) );
		m_pnwMeasureMethodBtn[AME_MEASURE_TTTG] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_TTTG" ), m_pnwMeasureIcon[AME_MEASURE_TTTG], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_TTTG]->SetUserData( IntToPtr( AME_MEASURE_TTTG ) );
		m_pnwMeasureMethodBtn[AME_MEASURE_CLOSEST] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_CLOSEST" ), m_pnwMeasureIcon[AME_MEASURE_CLOSEST], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_CLOSEST]->SetUserData( IntToPtr( AME_MEASURE_CLOSEST ) );
		m_pnwMeasureMethodBtn[AME_MEASURE_DIAMETER] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_DIAMETER" ), m_pnwMeasureIcon[AME_MEASURE_DIAMETER], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_DIAMETER]->SetUserData( IntToPtr( AME_MEASURE_DIAMETER ) );
		if (AppAttr->GetStartupMode() == AME_STARTUP_MODE_CD_VIEWER)
		{ // 最近点計算はCDビューアでは表示しない(マスクが扱えず無意味なので)
			m_pnwMeasureMethodBtn[AME_MEASURE_CLOSEST]->Hide();
			m_pnwMeasureMethodBtn[AME_MEASURE_DIAMETER]->Hide();
		}
	}

	{
		AmeManagedGroupBox* group = new AmeManagedGroupBox( m_pnwMeasureMethodGroup, GetPlugInName(), L"PolyLineType", AppAttr->GetResStr( "AME_MEASURE_POLYLINE_TYPE" ), LAYOUT_FILL_X );
		group->SetSubGroup( true );
		group->SetEnableHelpBox( false );
		read_registry_targets.push_back( group );
		m_pnwMeasurePolylineTypeFrame = group;
		m_pnwMeasurePolylineTypeFrame->Hide();

		PnwHorizontalFrame* hframe = new PnwHorizontalFrame( group, LAYOUT_FILL_X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
		m_pnwMeasureMethodBtn[AME_MEASURE_POLYLINE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_COMMON_MEASURE_METHOD_POLY_LINE" ), m_pnwMeasureIcon[AME_MEASURE_POLYLINE], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_POLYLINE]->SetUserData( IntToPtr( AME_MEASURE_POLYLINE ) );
		m_pnwMeasureMethodBtn[AME_MEASURE_CURVE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_COMMON_MEASURE_METHOD_CURVE" ), m_pnwMeasureIcon[AME_MEASURE_CURVE], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_CURVE]->SetUserData( IntToPtr( AME_MEASURE_CURVE ) );
	}

	{
		AmeManagedGroupBox* group = new AmeManagedGroupBox( m_pnwMeasureMethodGroup, GetPlugInName(), L"AngleType", AppAttr->GetResStr( "AME_MEASURE_ANGLE_TYPE" ), LAYOUT_FILL_X );
		group->SetSubGroup( true );
		group->SetEnableHelpBox( false );
		read_registry_targets.push_back( group );
		m_pnwMeasureAngleTypeFrame = group;
		m_pnwMeasureAngleTypeFrame->Hide();

		PnwHorizontalFrame* hframe = new PnwHorizontalFrame( group, LAYOUT_FILL_X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
		m_pnwMeasureMethodBtn[AME_MEASURE_ANGLE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_ANGLE_3POINT" ), m_pnwMeasureIcon[AME_MEASURE_ANGLE], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_ANGLE]->SetUserData( IntToPtr( AME_MEASURE_ANGLE ) );
		m_pnwMeasureMethodBtn[AME_MEASURE_TWO_LINE_ANGLE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_ANGLE_4POINT" ), m_pnwMeasureIcon[AME_MEASURE_TWO_LINE_ANGLE], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_TWO_LINE_ANGLE]->SetUserData( IntToPtr( AME_MEASURE_TWO_LINE_ANGLE ) );
		m_pnwMeasureMethodBtn[AME_MEASURE_PROJ_ANGLE] = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_METHOD_ANGLE_PROJ_3POINT" ), m_pnwMeasureIcon[AME_MEASURE_PROJ_ANGLE], this, ID_MEASURE_TYPE, LAYOUT_CENTER_X | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_LARGE );
		m_pnwMeasureMethodBtn[AME_MEASURE_PROJ_ANGLE]->SetUserData( IntToPtr( AME_MEASURE_PROJ_ANGLE ) );
	}

	{
		AmeManagedGroupBox* group = new AmeManagedGroupBox( pTopFrame, GetPlugInName(), L"Operation", AppAttr->GetResStr( "AME_COMMON_OPERATION" ), LAYOUT_FILL_X );
		read_registry_targets.push_back( group );
		grouplist.push_back( AmeManagedGroupBoxElement( false, group ) );
		m_pnwMeasureOperationFrame = group;

		{
			m_pnwClosestOperationFrame = new PnwVerticalFrame( group, LAYOUT_FILL_X );
			new PnwLabel( m_pnwClosestOperationFrame, AppAttr->GetResStr( "AME_MEASURE_CLOSEST_GUIDANCE_INPUT_ROI" ), nullptr, LAYOUT_FILL_X | LAYOUT_CENTER_Y, 0, 0, 0, 0, 10, 10, 10, 10 );
			m_pnwMeasureExecuteClosestButton = new PnwButton( m_pnwClosestOperationFrame,
			AppAttr->GetResStr( "AME_MEASURE_EXECUTE_CLOSEST_SEARCH" ),
			nullptr,
			this, ID_EXECUTE_CLOSEST, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT, 0, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_MIDDLE );
			m_pnwMeasureExecuteClosestButton->SetRepeat( false );
			m_pnwMeasureExecuteClosestButton->SetButtonStyle( PnwButton::BUTTON_EXECUTE );

			m_pnwDiameterOperationFrame = new PnwVerticalFrame( group, LAYOUT_FILL_X );
			new PnwLabel( m_pnwDiameterOperationFrame, AppAttr->GetResStr( "AME_MEASURE_DIAMETER_GUIDANCE_INPUT_ROI" ), nullptr, LAYOUT_FILL_X | LAYOUT_CENTER_Y, 0, 0, 0, 0, 10, 10, 10, 10 );
			m_pnwMeasureExecuteDiameterButton = new PnwButton( m_pnwDiameterOperationFrame,
			AppAttr->GetResStr( "AME_MEASURE_EXECUTE_DIAMETER" ),
			nullptr,
			this, ID_EXECUTE_DIAMETER, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT, 0, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_MIDDLE );
			m_pnwMeasureExecuteDiameterButton->SetRepeat( false );
			m_pnwMeasureExecuteDiameterButton->SetButtonStyle( PnwButton::BUTTON_EXECUTE );
		}

		{
			m_pnwCopyToAllButton = new PnwButton( group, AppAttr->GetResStr( "AME_MEASURE_COPY_TO_ALL" ), m_pCopyToAllIcon, this, ID_COPY_TO_ALL, LAYOUT_FIX_HEIGHT | LAYOUT_TOP | LAYOUT_LEFT, 0, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );

			// 全画像へコピーは、許可されたプラグインのみ使用可能とする。
			AmeBaseAnalysisPlugInDesktopGUI* plugin = dynamic_cast<AmeBaseAnalysisPlugInDesktopGUI*>(GetTaskManager()->GetTaskCardPlugInGUI().m_pPlugIn);
			if ( plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) == L"" )
			{
				m_pnwCopyToAllButton->Hide();
			}
		}
	}

	{
		AmeManagedGroupBox* group = new AmeManagedGroupBox( pTopFrame, GetPlugInName(), L"DisplaySetting", AppAttr->GetResStr( "AME_COMMON_DISPLAY_SETTING" ), LAYOUT_FILL_X );
		read_registry_targets.push_back( group );
		grouplist.push_back( AmeManagedGroupBoxElement( false, group ) );
		m_pnwMeasureSettingFrame = group;
		m_pnwMeasureSettingFrame->Hide();

		PnwVerticalFrame* setting_frame = new PnwVerticalFrame( m_pnwMeasureSettingFrame, LAYOUT_FILL_X );

		//設定パネル作成
		for ( int i = FRAME_INI; i < FRAME_NUM; i++ )
		{
			m_SettingFrame.push_back( new PnwVerticalFrame( setting_frame, LAYOUT_FILL_X ) );
			m_SettingFrame[i]->Hide();
		}

		//設定パネル０
		//フォントの種類とサイズ（共通）
		{
			PnwHorizontalFrame* hframe = new PnwHorizontalFrame( m_SettingFrame[FRAME_FONT], LAYOUT_FILL_X );
			new PnwLabel( hframe, AppAttr->GetResStr( "AME_COMMON_FONT" ), NULL, LAYOUT_CENTER_Y );

			{
				PnwHorizontalFrame* frame = new PnwHorizontalFrame( hframe, LAYOUT_FILL_X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
				m_pnwTextFontName = new PnwLabel( frame, L"", NULL, LAYOUT_CENTER_Y | LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT, 0, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
				m_pnwTextFontName->SetThickFrame( true );
				m_pnwTextFontName->SetJustify( PnwLabel::CENTER );


				PnwButton* btn = new PnwButton( frame, L"", AppAttr->GetAppIcon( AME_ICON_COMMON_CHANGE ), this, ID_TEXT_FONT, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
				btn->SetToolTipText( AppAttr->GetResStr( "AME_MEASURE_TEXT_CHANGE_FONT_TOOL_TIP" ) );


				new PnwLabel( frame, AppAttr->GetResStr( "AME_MEASURE_TEXT_FONT_SIZE" ), NULL, LAYOUT_CENTER_Y );

				m_pnwTextFontCombo = new PnwComboBox( hframe, 2, this, ID_FONT_SIZE, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT | LAYOUT_RIGHT, 0, 0, 50, AppAttr->m_WidgetSize.AME_COMBOBOX_SIZE );
				m_pnwTextFontCombo->SetNumVisible( 5 );
				m_pnwTextFontCombo->AddItem( AppAttr->GetResStr( "AME_MEASURE_TEXT_FONT_SIZE_1" ), NULL, 10 ); //1倍
				m_pnwTextFontCombo->AddItem( AppAttr->GetResStr( "AME_MEASURE_TEXT_FONT_SIZE_2" ), NULL, 20 ); //2倍
				m_pnwTextFontCombo->AddItem( AppAttr->GetResStr( "AME_MEASURE_TEXT_FONT_SIZE_3" ), NULL, 30 ); //3倍
				m_pnwTextFontCombo->AddItem( AppAttr->GetResStr( "AME_MEASURE_TEXT_FONT_SIZE_4" ), NULL, 40 ); //4倍
				m_pnwTextFontCombo->AddItem( AppAttr->GetResStr( "AME_MEASURE_TEXT_FONT_SIZE_5" ), NULL, 50 ); //5倍


				//m_DefaultFontNameは暫定で0番目
				m_pnwTextFontName->SetText( m_ColorSetting[0]->getTextFontName() );
				UpdateFontSizeCombo( m_ColorSetting[0]->getTextFontRate() );
			}
		}

		//設定パネル１
		{
			//前景色
			PnwHorizontalFrame* hframe = new PnwHorizontalFrame( m_SettingFrame[FRAME_FORECOLOR], LAYOUT_FILL_X );
			new PnwLabel( hframe, AppAttr->GetResStr( "AME_MEASURE_TEXT_COLOR" ), NULL, LAYOUT_CENTER_Y );

			{
				PnwHorizontalFrame* frame = new PnwHorizontalFrame( hframe, LAYOUT_RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
				//m_DefaultTextColorは暫定で0番目
				m_pnwMeasureTextColor = new PnwColorWell( frame, m_ColorSetting[0]->getFgColor(), this, ID_FORE_COLOR, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, 100, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
				m_pnwMeasureTextColor->SetColorDialogTitle( AppAttr->GetResStr( "AME_COMMON_COLOR_SETTING" ) );

				PnwButton* btn = new PnwButton( frame, L"", AppAttr->GetAppIcon( AME_ICON_COMMON_CHANGE ), this, ID_FORE_COLOR_IN, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
				btn->SetToolTipText( AppAttr->GetResStr( "AME_MEASURE_TEXT_CHANGE_COLOR_TOOL_TIP" ) );
			}
		}

//設定パネル２
		{
		//背景色
			{
				PnwHorizontalFrame* hframe = new PnwHorizontalFrame( m_SettingFrame[FRAME_BACKCOLOR], LAYOUT_FILL_X );
				m_pnwTextBackCheck = new PnwCheckButton( hframe, AppAttr->GetResStr( "AME_COLOR_PROPERTY_BG_COLOR" ), this, ID_TEXT_BACK_CHECK, LAYOUT_CENTER_Y );

				{
					PnwHorizontalFrame* frame = new PnwHorizontalFrame( hframe, LAYOUT_RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
					//m_DefaultBackColorは暫定で0番目
					m_pnwMeasureBackColor = new PnwColorWell( frame, m_ColorSetting[0]->getBgColor(), this, ID_BACK_COLOR, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, 100, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
					m_pnwMeasureBackColor->SetColorDialogTitle( AppAttr->GetResStr( "AME_COMMON_COLOR_SETTING" ) );
					m_pnwTextBackCheck->SetCheck( m_ColorSetting[0]->getTextBackCheck() );

					m_pnwMeasureBackColorButton = new PnwButton( frame, L"", AppAttr->GetAppIcon( AME_ICON_COMMON_CHANGE ), this, ID_BACK_COLOR_IN, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
					m_pnwMeasureBackColorButton->SetToolTipText( AppAttr->GetResStr( "AME_COLOR_PROPERTY_CHAMGE_BG_COLOR" ) );

					m_pnwMeasureBackColorButton->SetEnabled( m_ColorSetting[0]->getTextBackCheck() );
					m_pnwMeasureBackColor->SetEnabled( m_ColorSetting[0]->getTextBackCheck() );
				}
			}
			{
				m_pnwTextFrameCheck = new PnwCheckButton( m_SettingFrame[FRAME_BACKCOLOR], AppAttr->GetResStr( "AME_MEASURE_TEXT_FRAME" ), this, ID_TEXT_FRAME_CHECK, LAYOUT_CENTER_Y );
				//m_DefaultTextFrameは暫定で0番目
				m_pnwTextFrameCheck->SetCheck( m_ColorSetting[0]->getTextFrameCheck() );
			}
		}

		//設定パネル３
		{
			//制御点の色
			PnwHorizontalFrame* hframe = new PnwHorizontalFrame( m_SettingFrame[FRAME_CPCOLOR], LAYOUT_FILL_X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
			new PnwLabel( hframe, AppAttr->GetResStr( "AME_MEASURE_CP_COLOR" ), NULL, LAYOUT_CENTER_Y );

			{
				PnwHorizontalFrame* frame = new PnwHorizontalFrame( hframe, LAYOUT_RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
				//m_DefaultTextColorは暫定で0番目
				m_pnwMeasureCpColor = new PnwColorWell( frame, m_ColorSetting[0]->getCpColor(), this, ID_CP_COLOR, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, 100, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
				m_pnwMeasureCpColor->SetColorDialogTitle( AppAttr->GetResStr( "AME_COMMON_COLOR_SETTING" ) );

				PnwButton* btn = new PnwButton( frame, L"", AppAttr->GetAppIcon( AME_ICON_COMMON_CHANGE ), this, ID_CP_COLOR_IN, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
				btn->SetToolTipText( AppAttr->GetResStr( "AME_MEASURE_CHANGE_CP_COLOR_TOOL_TIP" ) );
			}
		}

		//設定パネル４
		{
			//ポイント計測のサイズ設定スライダー
			PnwHorizontalFrame* hframe = new PnwHorizontalFrame( m_SettingFrame[FRAME_CURSORSLIDER], LAYOUT_FILL_X, 0, 0, 0, 0, PNW_SPACING, PNW_SPACING, 0, 0 );
			new PnwLabel( hframe, AppAttr->GetResStr( "AME_MEASURE_CHCURSOR_WIDTH" ), NULL, LAYOUT_CENTER_Y );

			m_pnwCHCursorSlider = new PnwSliderSet( hframe, this, ID_CHCURSOR_SIZE, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT, 0, 0, 0, AppAttr->m_WidgetSize.AME_SLIDERSET_SIZE );
			m_pnwCHCursorSlider->SetRange( 2, 30 );
			m_pnwCHCursorSlider->SetValue( m_ColorSetting[0]->getCHCursorSize() );
		}

		//設定パネル５
		{
			// サイズ入力
			m_pnwInputSizeButton = new PnwButton( m_SettingFrame[FRAME_SIZEBUTTON], AppAttr->GetResStr( "AME_MEASURE_INPUT_SIZE" ), m_pInputSizeIcon, this, ID_INPUT_SIZE, LAYOUT_FIX_HEIGHT | LAYOUT_TOP | LAYOUT_RIGHT | LAYOUT_FIX_WIDTH, 0, 0, 150, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
		}

		//設定パネル６
		{
			// 注意書きの表示
			m_pnwShowProjWarningCheck = new PnwCheckButton( m_SettingFrame[FRAME_PROJWARNING], AppAttr->GetResStr( "AME_MEASURE_SHOW_PROJ_3POINT_WARNING" ), this, ID_SHOW_PROJ_WARNING );
			m_pnwShowProjWarningCheck->SetCheck( m_ColorSetting[0]->getShowProjWarning() );
		}

		//設定パネル７
		{
			// 注意書きの表示
			m_pnwShow4PointWarningCheck = new PnwCheckButton( m_SettingFrame[FRAME_4POINTWARNING], AppAttr->GetResStr( "AME_MEASURE_SHOW_4POINT_WARNING" ), this, ID_SHOW_4POINT_WARNING );
			m_pnwShow4PointWarningCheck->SetCheck( m_ColorSetting[0]->getShow4PointWarning() );
		}

		//設定パネル８
		{
			// 直線・折れ線計測表示オプションコンボボックス
			{
				PnwHorizontalFrame* hframe = new PnwHorizontalFrame( m_SettingFrame[FRAME_LINEOPTION], LAYOUT_FILL_X, 0, 0, 0, 0, PNW_SPACING, PNW_SPACING, 0, 0 );
				new PnwLabel( hframe, AppAttr->GetResStr( "AME_COMMON_DISPLAY_OPTION" ), NULL, LAYOUT_LEFT | LAYOUT_CENTER_Y );

				m_pnwLineOption = new PnwComboBox( hframe, 2, this, ID_LINE_OPTION, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT | LAYOUT_RIGHT, 0, 0, 175, AppAttr->m_WidgetSize.AME_COMBOBOX_SIZE );
				m_pnwLineOption->SetNumVisible( 3 );
				m_pnwLineOption->AddItem( AppAttr->GetResStr( "AME_MEASURE_LINE_NO_OPTION" ), NULL, 60 );
				m_pnwLineOption->AddItem( AppAttr->GetResStr( "AME_MEASURE_LINE_OPTION_SCALE" ), NULL, 80 );
				m_pnwLineOption->AddItem( AppAttr->GetResStr( "AME_MEASURE_LINE_OPTION_CS_SCALE" ), NULL, 100 );

				m_pnwCurveOption = new PnwComboBox( hframe, 2, this, ID_LINE_OPTION, LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT | LAYOUT_RIGHT, 0, 0, 175, AppAttr->m_WidgetSize.AME_COMBOBOX_SIZE );
				m_pnwCurveOption->SetNumVisible( 2 );
				m_pnwCurveOption->AddItem( AppAttr->GetResStr( "AME_MEASURE_LINE_NO_OPTION" ), NULL, 60 );
				m_pnwCurveOption->AddItem( AppAttr->GetResStr( "AME_MEASURE_LINE_OPTION_SCALE" ), NULL, 80 );
				m_pnwCurveOption->Hide();
			}
		}

		group->SetMinimized( true );
	}

	{
		AmeManagedGroupBox* group = new AmeManagedGroupBox( pTopFrame, GetPlugInName(), L"Detail", AppAttr->GetResStr( "AME_MEASURE_DETAIL_GROUP" ), LAYOUT_FILL_X );
		read_registry_targets.push_back( group );
		grouplist.push_back( AmeManagedGroupBoxElement( false, group ) );
		m_pnwMeasureDetailsFrame = group;
		// リスケール
		{
			m_pnwRescaleFrame = new PnwHorizontalFrame( group, LAYOUT_FILL_X, 0, 0, 0, 0 );
			new PnwLabel( m_pnwRescaleFrame, AppAttr->GetResStr( "AME_MEASURE_RESCALE_LABEL" ), NULL, LAYOUT_CENTER_Y | LAYOUT_LEFT );

			m_pnwRescaleCombo = new PnwComboBox( m_pnwRescaleFrame, 20, this, ID_RESCALE_TYPE, LAYOUT_CENTER_Y | LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT, 0, 0, 0, AppAttr->m_WidgetSize.AME_COMBOBOX_SIZE );
			m_pnwRescaleCombo->SetEnabled( false );

			m_pnwRescaleButton = new PnwButton( m_pnwRescaleFrame, AppAttr->GetResStr( "AME_MEASURE_RESCALE_CHANGE" ), NULL, this, ID_RESCALE, LAYOUT_CENTER_Y | LAYOUT_FIX_WIDTH | LAYOUT_FIX_HEIGHT, 0, 0, 50, AppAttr->m_WidgetSize.AME_BUTTON_SIZE_NORMAL );
		}

		{
			PnwHorizontalFrame* hframe = new PnwHorizontalFrame( group, LAYOUT_FILL_X );
			m_pnwGraphButton = new PnwButton( hframe, AppAttr->GetResStr( "AME_MEASURE_SHOW_RESULT" ), m_pGraphIcon, this, ID_RESULT, LAYOUT_FILL_X | LAYOUT_FIX_HEIGHT | LAYOUT_TOP | LAYOUT_CENTER_X, 0, 0, 0, AppAttr->m_WidgetSize.AME_EXECUTE_BTN_HEIGHT );
			m_pnwGraphButton->SetButtonStyle( PnwButton::BUTTON_EXECUTE );

			//マルチ３Ｄでは、グラフ表示すると誤診を招く可能性があるため、利用させない。
			//（例）マルチ３ＤでＭＰＲダイアログで計測しグラフを表示し、他の画像をアクティブにすると、グラフの表示は前の画像のものになっている。
			if ( GetTaskManager()->GetTaskCardPlugInGUI().GetPlugInName() == L"AME_PG_MULTI3D" )
			{
				m_pnwGraphButton->Hide();
			}
		}
	}

	// 操作
	{
		AmeManagedGroupBox* group = new AmeManagedGroupBox( pTopFrame, GetPlugInName(), L"Operation", AppAttr->GetResStr( "AME_COMMON_OPERATION" ), LAYOUT_FILL_X | LAYOUT_BOTTOM );
		read_registry_targets.push_back( group );
		grouplist.push_back( AmeManagedGroupBoxElement( false, group ) );
		PnwHorizontalFrame* hframe = new PnwHorizontalFrame( group, LAYOUT_FILL_X | LAYOUT_FILL_Y, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );

		m_frameMeasureList = new Ame::MeasureFrameList( hframe, GetPlugInName(), L"MeasurementList", LAYOUT_FILL_X | LAYOUT_FILL_Y );
		m_frameMeasureList->SignalLeftClickList.connect( std::bind( &AmeMeasurePlugInGUI::SlotCmdMeasurementList, this, std::placeholders::_1, std::placeholders::_2 ) );
		m_frameMeasureList->SignalExecuteComment.connect( std::bind( &AmeMeasurePlugInGUI::SlotCmdComment, this, std::placeholders::_1, std::placeholders::_2 ) );
		m_frameMeasureList->SignalExecuteDelete.connect( std::bind( &AmeMeasurePlugInGUI::SlotCmdDelete, this ) );
		m_frameMeasureList->SignalExecuteDeleteAll.connect( std::bind( &AmeMeasurePlugInGUI::SlotCmdDeleteAll, this ) );
		m_frameMeasureList->SignalExecuteCopy.connect( std::bind( &AmeMeasurePlugInGUI::SlotCmdCopy, this ) );
	}

	//レジストリ読み込み
	for ( size_t i = 0; i < read_registry_targets.size(); i++ )
	{
		read_registry_targets[i]->ReadRegistry();
	}

	return pTopFrame;
}


// ハンドラの登録
void AmeMeasurePlugInGUI::RegisterHandler()
{
	EventHandler.m_Handler[AME_HANDLE_LEFT_BTN_PRESS] = onLeftBtnPress;
	EventHandler.m_Handler[AME_HANDLE_LEFT_BTN_RELEASE] = onLeftBtnRelease;
	EventHandler.m_Handler[AME_HANDLE_MOTION] = onMotion;
	EventHandler.m_Handler[AME_HANDLE_DOUBLE_CLICKED] = onDoubleClicked;
	EventHandler.m_Handler[AME_HANDLE_KEY_PRESS] = onKeyPress;
	EventHandler.m_Handler[AME_HANDLE_LEAVE] = onLeave;
}


void AmeMeasurePlugInGUI::WriteRegistry( AmeRegistryFileBase* reg )
{
	for ( int count = 0; count < AME_MEASURE_NUM; count++ )
	{
		AmeString section = AmeString::Format( L"Measure%d", count );
		m_ColorSetting[count]->Save( reg, section );
	}
	reg->SetElement( GetPlugInName(), L"m_bIntegrateHistogram", m_bIntegrateHistogram );
	for ( int count = 0; count < MODALITY_TYPE_NUM; count++ )
	{
		reg->SetElement( GetPlugInName(), AmeString::Format( L"m_iHistogramStep_%d", count ), m_iHistogramStep[count] );
	}
	reg->SetElement( GetPlugInName(), L"m_iHistogramStepSUV", m_iHistogramStepSUV );
	reg->SetElement( GetPlugInName(), L"m_fHistogramStepGY", m_fHistogramStepGY );

	if ( m_frameMeasureList != nullptr )
	{
		m_frameMeasureList->GetMeasurementList()->WriteRegistry();
	}
}

void AmeMeasurePlugInGUI::ReadRegistry( AmeRegistryFileBase* reg )
{
	for ( int count = 0; count < AME_MEASURE_NUM; count++ )
	{
		AmeString section = AmeString::Format( L"Measure%d", count );
		m_ColorSetting[count]->Load( reg, section );
		m_ColorSetting[count]->Init();
	}
	reg->GetElement( GetPlugInName(), L"m_bIntegrateHistogram", m_bIntegrateHistogram, false );
	for ( int count = 0; count < MODALITY_TYPE_NUM; count++ )
	{
		reg->GetElement( GetPlugInName(), AmeString::Format( L"m_iHistogramStep_%d", count ), m_iHistogramStep[count], 50 );
	}
	reg->GetElement( GetPlugInName(), L"m_iHistogramStepSUV", m_iHistogramStepSUV, 0.010f );
	reg->GetElement( GetPlugInName(), L"m_fHistogramStepGY", m_fHistogramStepGY, 0.01f );
}

// メニューの使用
bool AmeMeasurePlugInGUI::ShowMouseMenu( AmeImageViewer*, PnwEventArgs* )
{
	return true;
}

// カスケードメニューの使用
bool AmeMeasurePlugInGUI::ShowCascadeMenu( AmeImageViewer* viewer, PnwEventArgs* )
{
	//0番目以外計測させない
	if ( viewer == nullptr || viewer->GetCurrentViewerAttrIndex() != 0 )
	{
		return false;
	}
	else
	{
		return true;
	}
}


// 排他メニューの使用
bool AmeMeasurePlugInGUI::CreateExclusiveMenu( PnwWidget* menu, AmeImageViewer* viewer, PnwEventArgs* )
{
	PnwMenuCommand* me;

	AmeMeasureDrawObject* draw = m_pEngine->GetCurrentMeasure( false );

	// 生成中の多角形ROI計測の確定
	if ( m_bNowCreating )
	{
		if ( draw != nullptr )
		{
			switch ( draw->GetType() )
			{
				case AME_MEASURE_PROJ_ANGLE:
				{
					AmeMeasureProjAngle* line = static_cast<AmeMeasureProjAngle*>(draw);
					me = new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_COMMON_UNDO" ), NULL, this, ID_BACK_NOW );
					if ( line->m_vecProjectedPoint.size() <= 2 || m_iCreatingImageAttrID != viewer->GetViewerAttr()->GetImageAttr()->GetImageAttrID() )
					{
						me->SetEnabled( false );
					}

					Ame::MenuCommand* cmd = new Ame::MenuCommand( menu, AppAttr->GetResStr( "AME_MEASURE_POINT_CANCEL" ), NULL, m_frameMeasureList, Ame::MeasureFrameList::ID_MENU_COMMAND_CLICKED );
					cmd->SignalClicked.connect( std::bind( &AmeMeasurePlugInGUI::SlotCmdDelete, this ) );

					me = new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_COMMON_FINISH" ), NULL, this, ID_FINISH_NOW );
					if ( draw->GetType() == AME_MEASURE_ANGLE && line->m_vecProjectedPoint.size() <= 2 )
					{
						me->SetEnabled( false );
					}
					else if ( draw->GetType() == AME_MEASURE_PROJ_ANGLE && line->m_vecProjectedPoint.size() <= 2 )
					{
						me->SetEnabled( false );
					}
					else if ( draw->GetType() == AME_MEASURE_TWO_LINE_ANGLE && line->m_vecProjectedPoint.size() <= 3 )
					{
						me->SetEnabled( false );
					}
					else if ( m_iCreatingImageAttrID != viewer->GetViewerAttr()->GetImageAttr()->GetImageAttrID() )
					{
						// 作成開始した画像以外では確定できない
						me->SetEnabled( false );
					}
					return true;
				}
				case AME_MEASURE_ANGLE:
				case AME_MEASURE_TWO_LINE_ANGLE:
				case AME_MEASURE_POLYLINE:
				case AME_MEASURE_CURVE:
				{
					AmeMeasurePointArrayBase* line = static_cast<AmeMeasurePointArrayBase*>(draw);
					me = new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_COMMON_UNDO" ), NULL, this, ID_BACK_NOW );
					if ( line->m_vecPoint.size() <= 2 || m_iCreatingImageAttrID != viewer->GetViewerAttr()->GetImageAttr()->GetImageAttrID() )
					{
						me->SetEnabled( false );
					}

					Ame::MenuCommand* cmd = new Ame::MenuCommand( menu, AppAttr->GetResStr( "AME_MEASURE_POINT_CANCEL" ), NULL, m_frameMeasureList, Ame::MeasureFrameList::ID_MENU_COMMAND_CLICKED );
					cmd->SignalClicked.connect( std::bind( &AmeMeasurePlugInGUI::SlotCmdDelete, this ) );

					me = new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_COMMON_FINISH" ), NULL, this, ID_FINISH_NOW );
					if ( draw->GetType() == AME_MEASURE_ANGLE && line->m_vecPoint.size() <= 2 )
					{
						me->SetEnabled( false );
					}
					else if ( draw->GetType() == AME_MEASURE_PROJ_ANGLE && line->m_vecPoint.size() <= 2 )
					{
						me->SetEnabled( false );
					}
					else if ( draw->GetType() == AME_MEASURE_TWO_LINE_ANGLE && line->m_vecPoint.size() <= 3 )
					{
						me->SetEnabled( false );
					}
					else if ( m_iCreatingImageAttrID != viewer->GetViewerAttr()->GetImageAttr()->GetImageAttrID() )
					{
						// 作成開始した画像以外では確定できない
						me->SetEnabled( false );
					}
					return true;
				}
				case AME_MEASURE_ROI:
				case AME_MEASURE_FREEHAND:
				{
					AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(draw);
					me = new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_COMMON_UNDO" ), NULL, this, ID_BACK_NOW );
					if ( roi->m_vecPoints.size() <= 2 || m_iCreatingViewerAttrID != viewer->GetViewerAttr()->GetViewerAttrID() )
					{
						me->SetEnabled( false );
					}

					Ame::MenuCommand* cmd = new Ame::MenuCommand( menu, AppAttr->GetResStr( "AME_MEASURE_POINT_CANCEL" ), NULL, m_frameMeasureList, Ame::MeasureFrameList::ID_MENU_COMMAND_CLICKED );
					cmd->SignalClicked.connect( std::bind( &AmeMeasurePlugInGUI::SlotCmdDelete, this ) );

					me = new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_COMMON_FINISH" ), NULL, this, ID_FINISH_NOW );
					if ( m_iCreatingViewerAttrID != viewer->GetViewerAttr()->GetViewerAttrID() )
					{
						// 作成開始したビューア以外では確定できない
						me->SetEnabled( false );
					}
					return true;
				}
			}
		}
	}

	// コピーの確定・キャンセル
	if ( m_bNowCopying )
	{
		new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_COMMON_DETERMINE" ), NULL, this, ID_FINISH_COPY );
		new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_MEASURE_COPY_CANCEL" ), NULL, this, ID_CANCEL_COPY );
		return true;
	}

	return false;
}


// ツールバーボタンの有効、無効チェック
void AmeMeasurePlugInGUI::IsShownOnShortcutBar( AmeShortcutBar* bar, const AmeSelectableFrame* frame, AmeViewerAttr* attr )
{
	if ( GetTaskManagerEngine()->IsConsole() )
	{
		return;
	}

	if ( !GetTaskManager()->IsShowObjectOnViewer() )
	{
		return;
	}

	if ( frame == NULL ) return;
	if ( attr == NULL || attr->GetViewer() == NULL )
	{
		return;
	}

	for ( size_t i = 0; i < AME_MEASURE_NUM; i++ )
	{
		bar->ShowEntry( AmeMeasureTypeString[i] );
	}

	//ショートカットバーの状態更新
	UpdateInterfaceForShortcutBar( attr );

}


// フォントサイズコンボボックスの更新
void AmeMeasurePlugInGUI::UpdateFontSizeCombo( int rate )
{
	if ( m_pnwTextFontCombo == NULL )
	{
		return;
	}

	for ( int i = 0; i < m_pnwTextFontCombo->GetNumItems(); i++ )
	{
		int c_rate = (int)m_pnwTextFontCombo->GetItemData( i );
		if ( c_rate == rate )
		{
			m_pnwTextFontCombo->SetCurrentItem( i );
			break;
		}
	}
}


// ショートカットボタン生成
void AmeMeasurePlugInGUI::CreateShortcutBar( AmeShortcutBar* bar )
{
	if ( GetTaskManagerEngine()->IsConsole() )
	{
		// コンテキストメニューで対応
		return;
	}

	for ( size_t i = AME_MEASURE_LINE; i < AME_MEASURE_NUM; i++ )
	{
		if ( m_pnwMeasureShortcutIcon[i] == nullptr )
		{
			AmeString fname2 = AmeString::Format( L"AME_ICON_SHORTCUT_MEASURE%Id", i + 1 );
			m_pnwMeasureShortcutIcon[i] = AmeLoadPlugInIcon( GetPlugInName(), fname2 );
		}
	}

	//初期化
	for ( size_t i = 0; i < AME_MEASURE_NUM; i++ )
	{
		int id = AmeMeasureDisplayOrder[i];
		//m_pnwShortCutBtn[id] = bar->AddButton(AME_SHORTCUTBAR_PRIORITY_SHORTCUT, GetPlugInID(), AmeMeasureTypeString[id], L"", m_pnwMeasureShortcutIcon[id], AppAttr->GetResStr(AmeMeasureResourceString[id]), this, ID_SHORT_CUT, IntToPtr(id), false, true , AME_KEYBIND_NONE);
		m_pnwShortCutBtn[id] = bar->AddShortCut( AME_SHORTCUTBAR_PRIORITY_SHORTCUT, GetPlugInID(), AmeMeasureTypeString[id], L"", m_pnwMeasureShortcutIcon[id], AppAttr->GetResStr( AmeMeasureResourceString[id] ), this, ID_SHORT_CUT, IntToPtr( id ), false );
	}

}


// 通常メニューの生成
bool AmeMeasurePlugInGUI::CreateMouseMenu( PnwWidget* menu, AmeImageViewer* viewer, PnwEventArgs* )
{
	//0番目以外計測させない
	if ( viewer == nullptr || viewer->GetCurrentViewerAttrIndex() != 0 )
	{
		return false;
	}

	bool bCascade = !(this->IsCurrentMouseMode());

	PnwMenuCommand* me[AME_MEASURE_NUM];

	CreateIcon();

	for ( size_t i = 0; i < AME_MEASURE_NUM; i++ )
	{
		int id = AmeMeasureDisplayOrder[i];
		me[id] = new PnwMenuCommand( menu, AppAttr->GetResStr( AmeMeasureResourceString[id] ), NULL, this, ID_MEASURE_TYPE );
		switch ( id )
		{
			case AME_MEASURE_LINE: me[id]->SetIcon( AppAttr->GetAppIcon( AME_ICON_MEASURE_LINE ) ); break;
			case AME_MEASURE_POLYLINE: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_POLYLINE] ); break;
			case AME_MEASURE_ANGLE: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_ANGLE] ); break;
			case AME_MEASURE_CUBE: me[id]->SetIcon( AppAttr->GetAppIcon( AME_ICON_MEASURE_RECTANGLE ) ); break;
			case AME_MEASURE_ELLIPSE: me[id]->SetIcon( AppAttr->GetAppIcon( AME_ICON_MEASURE_ELLIPSE ) ); break;
			case AME_MEASURE_ROI: me[id]->SetIcon( AppAttr->GetAppIcon( AME_ICON_MEASURE_POLYGON ) ); break;
			case AME_MEASURE_FREEHAND: me[id]->SetIcon( AppAttr->GetAppIcon( AME_ICON_MEASURE_FREEHAND ) ); break;
			case AME_MEASURE_POINT: me[id]->SetIcon( AppAttr->GetAppIcon( AME_ICON_MEASURE_POINT ) ); break;
			case AME_MEASURE_BOX: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_BOX] ); break;
			case AME_MEASURE_SPHERE: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_SPHERE] ); break;
			case AME_MEASURE_PROJ_ANGLE: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_PROJ_ANGLE] ); break;
			case AME_MEASURE_TWO_LINE_ANGLE: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_TWO_LINE_ANGLE] ); break;
			case AME_MEASURE_PROJ_LINE: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_PROJ_LINE] ); break;
			case AME_MEASURE_CURVE: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_CURVE] ); break;
			case AME_MEASURE_TTTG: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_TTTG] ); break;
			case AME_MEASURE_CLOSEST: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_CLOSEST] ); break;
			case AME_MEASURE_DIAMETER: me[id]->SetIcon( m_pnwMeasureIcon[AME_MEASURE_DIAMETER] ); break;
		}
		if ( (id == AME_MEASURE_CLOSEST || id == AME_MEASURE_DIAMETER) && AppAttr->GetStartupMode() == AME_STARTUP_MODE_CD_VIEWER )
		{
			// 最近点計算と直径計測はCDビューアでは表示しない(マスクが扱えず無意味なので)
			me[id]->Hide();
		}
	}

	if ( !viewer->GetViewerAttr()->GetImageAttr()->IsEnableOperation( AmeImageAttr::ENABLE_TILT_MEASUREMENT ) )
	{
		me[AME_MEASURE_BOX]->Hide();
		me[AME_MEASURE_SPHERE]->Hide();
	}

	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		me[i]->SetUserData( IntToPtr( i ) );
	}

	if ( (!bCascade) && (m_CurrentType >= 0) && (m_CurrentType < AME_MEASURE_NUM) )
	{
		me[m_CurrentType]->SetActiveState( true );
	}

	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		bool bValidScale;
		if ( !IsMeasureCreatable( (AmeMeasureType)i, viewer, bValidScale ) )
		{
			me[i]->SetEnabled( false );
		}
	}

	// 全体計測系は、3Dプラグインのみ使用可能とする。
	// 全体計測はCDビューアでも利用できない。マスクが使えないので。
	AmePlugInGUIInformation info = GetTaskManager()->GetTaskCardPlugInGUI();
	if ( (AmeIs3DViewer( info.m_pPlugInAttr->m_PlugInName ) || info.m_pPlugInAttr->m_PlugInName == L"AME_PG_INTERPRETER3D") &&
		AppAttr->GetStartupMode() != AME_STARTUP_MODE_CD_VIEWER )
	{
		new PnwMenuSeparator( menu );
		new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_MEASURE_MES_ALL" ), NULL, this, ID_MEASURE_ALL );
		new PnwMenuCommand( menu, AppAttr->GetResStr( "AME_MEASURE_MES_ONE" ), NULL, this, ID_MEASURE_SINGLE );
	}

	return true;
}


// ダイアログを閉じる
void AmeMeasurePlugInGUI::CloseDialog()
{
	if ( m_pResultDialog != nullptr )
	{
		m_pResultDialog->Hide();
	}
}


void AmeMeasurePlugInGUI::ReleaseImageAttr( AmeImageAttr* imga )
{
	if ( imga != nullptr && m_pResultDialog != nullptr &&
		(m_pResultDialog->GetCurrentImageAttrID( 0 ) == imga->GetImageAttrID() || m_pResultDialog->GetCurrentImageAttrID( 1 ) == imga->GetImageAttrID()) )
	{
		m_pResultDialog->Hide();
	}

	__super::ReleaseImageAttr( imga );
}


void AmeMeasurePlugInGUI::NotifyViewerAttributeChangeForGUI( const AmeViewerAttr* pViewerAttr, const unsigned __int64& notify_type, const AmeNotifyViewerAttributeParameter&, std::set<AmeUpdateViewer>& mapViewer )
{
	if ( notify_type & (AME_NOTIFY_MESSAGE_MASK | AME_NOTIFY_MESSAGE_ACTIVE_BIT | AME_NOTIFY_MESSAGE_RENDERINGINFO_MULTI_MASK) )
	{ // マスクや表示状態の更新があった場合は最近点計算用キャッシュを無効化
		m_pEngine->InvalidateMaskBorderCache();
	}

	// オリジナルスライス位置
	if ( notify_type & AME_NOTIFY_MESSAGE_ORIGINAL_SLICE_POS )
	{
		// ※2Dビューアのスライス変更に対して表示単位を更新するため
		if ( IsCurrentMouseMode() )
		{
			UpdateInterface();
		}
	}

	// マスク編集
	if ( notify_type & (AME_NOTIFY_MESSAGE_MASK | AME_NOTIFY_MESSAGE_ACTIVE_BIT) )
	{
		if ( m_pResultDialog != nullptr )
		{
			if ( pViewerAttr != nullptr && pViewerAttr->GetImageAttr()->GetImageAttrID() == m_pResultDialog->GetCurrentImageAttrID() )
			{
				// 計測ダイアログを閉じる
				switch ( m_pResultDialog->GetCurrentMeasureType() )
				{
					case AME_MEASURE_ALL:
					case AME_MEASURE_SINGLE:
						m_pEngine->DeleteImageMeasure();
						m_pResultDialog->Hide();
						m_pResultDialog->UpdateHistogram();
						UpdateInterface();
						break;
				}
			}
		}
	}

	// 現在位置
	if ( notify_type & AME_NOTIFY_MESSAGE_CP )
	{
		//表示スライスのヒストグラム表示中なら消す
		if ( m_pResultDialog != nullptr )
		{
			if ( m_pResultDialog->GetCurrentMeasureType() == AME_MEASURE_SINGLE )
			{
				m_pEngine->DeleteImageMeasure();
				m_pResultDialog->Hide();
				m_pResultDialog->UpdateHistogram();
				UpdateInterface();
			}
		}
	}

	// 回転
	if ( notify_type & (AME_NOTIFY_MESSAGE_ROTATION | AME_NOTIFY_MESSAGE_CPR_ROTATION) )
	{
		AmeImageViewer* viewer = nullptr;
		if ( pViewerAttr != nullptr && (viewer = pViewerAttr->GetViewer()) != nullptr && viewer->GetViewerType() == AME_VIEWER_CPR )
		{
			for ( size_t i = 0; i < m_pEngine->m_AllObjects.size(); i++ )
			{
				AmeMeasureDrawObject* obj = m_pEngine->m_AllObjects[i];
				if ( obj != nullptr && obj->GetType() == AME_MEASURE_POINT )
				{
					// ポイント計測のCPR回転に合わせて、3D座標を更新
					AmeMeasurePoint* point = (AmeMeasurePoint*)obj;
					if ( point->m_bValidCPR && point->m_ViewerIdentity == viewer->GetViewerAttr()->GetViewerAttrIDString() )
					{
						AmeFloat2D disp_pos;
						if ( viewer->PathToDisplay( point->m_CPRPoint, disp_pos ) )
						{
							viewer->DisplayToVoxel( disp_pos, point->m_Point );
						}

						std::vector<AmeViewerAttr*> attrlist = GetTaskManager()->GetAllVisibleViewerAttrs();
						for ( size_t n = 0; n < attrlist.size(); n++ )
						{
							if ( attrlist[n] != NULL && attrlist[n]->GetViewer() != NULL )
							{
								//if(point->IsVisible(attrlist[n]->GetViewer())) // 見えなくなった時に画面が更新されないのでコメントアウト
								{
									mapViewer.insert( AmeUpdateViewer( attrlist[n], AmeUpdateViewer::AME_UPDATE_VIEWER ) );
								}
							}
						}
					}
				}
			}
		}
	}
}


AmeString AmeMeasurePlugInGUI::RequestMessage( const AmeString& message, AmeBasePlugInGUI* )
{
	std::vector<AmeString> strs;
	message.Split( strs, L" " );
	if ( strs.empty() ) return L"";

	if ( strs[0] == L"MeasureLine" )
	{
		AmeImageViewer* viewer = m_pViewerAttr->GetViewer();
		if ( strs.size() >= 7 && viewer != NULL )
		{
			if ( m_bNowCreating )
			{
				FinishCreatingMeasure();
			}

			m_CurrentType = AME_MEASURE_LINE;
			ChangeCurrentByID( -1 );
			SetCurrentColorSetting();

			// 新規作成
			AmeMeasureLine* line = new AmeMeasureLine( GetTaskManager(), GetPlugInID(), m_pViewerAttr->GetImageAttr() );
			line->m_vecPoint[0] = AmeFloat3D( strs[1].ToFloat(), strs[2].ToFloat(), strs[3].ToFloat() );
			line->m_vecPoint[1] = AmeFloat3D( strs[4].ToFloat(), strs[5].ToFloat(), strs[6].ToFloat() );

			line->m_iOrgViewerType = viewer->GetViewerType();
			line->m_FrameSerialID = viewer->GetViewerFrame()->GetSerialID();
			line->m_ViewerIdentity = m_pViewerAttr->GetViewerAttrIDString();
			line->m_ImageAttrIDString = m_pViewerAttr->GetImageAttr()->GetImageAttrIDString();
			if ( viewer->GetOverlayViewerAttr() != NULL &&
			   (viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
				viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
			{
				line->m_OverlayImageIdentity = viewer->GetOverlayViewerAttr()->GetImageAttr()->GetImageAttrIDString();
			}
			else
			{
				line->m_OverlayImageIdentity = L"";
			}

			int index = 0;
			if ( m_pEngine->m_AllObjects.size() != 0 )
			{
				index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
			}
			line->SetIndex( index );

			m_pEngine->m_AllObjects.push_back( line );
			GetTaskManager()->AddDrawObject( line );

			ChangeCurrentByID( line->GetObjectID() );

			m_CurrentType = AME_MEASURE_NONE;
			UpdateInterface();
			UpdateInterfaceForShortcutBar( m_pViewerAttr );
			return L"OK";
		}
	}

	return L"";
}


// ViewerAttrの登録
void AmeMeasurePlugInGUI::SetViewerAttr( AmeViewerAttr* attr )
{
	AmeBasePlugInGUI::SetViewerAttr( attr );
	m_ROIData.InvalidateIfNotMatch( attr );
	if ( attr == NULL )
	{
		return;
	}
	AmeImageViewer* pViewer = attr->GetViewer();

	//0番目以外計測させない。
	if ( pViewer != NULL && pViewer->GetCurrentViewerAttrIndex() != 0 )
	{
		if ( m_pResultDialog != NULL && m_pResultDialog->Visible )
		{
			m_pResultDialog->Hide();
		}
		//選択解除
		ClearCurrentSelection();
	}

	//動画再生中なら消す
	if ( attr->m_bNowAnimation )
	{
		if ( m_pResultDialog != NULL )
		{
			m_pResultDialog->Hide();
		}
	}

	//表示スライスのヒストグラム表示中なら消す
	if ( m_pResultDialog != NULL )
	{
		if ( m_pResultDialog->GetCurrentMeasureType() == AME_MEASURE_SINGLE )
		{
			m_pResultDialog->Hide();
		}
	}


	//UI更新(マルチ3Dの対象画像切替時にショートカット更新等も必要なため)
	UpdateInterfaceForShortcutBar( m_pViewerAttr );


	if ( IsCurrentMouseMode() )
	{
		if ( attr->GetViewer() != NULL )
		{
			attr->GetViewer()->Update();
		}
		UpdateHistogram();
		UpdateInterface();
		AmeCheckSUVCalculationCondition( GetTaskManager(), m_pViewerAttr );
	}
}

void AmeMeasurePlugInGUI::SetImageAttr( AmeImageAttr* imga )
{
	__super::SetImageAttr( imga );

	//全体計測のヒストグラム表示中なら消す
	if ( m_pResultDialog != NULL )
	{
		if ( m_pResultDialog->GetCurrentMeasureType() == AME_MEASURE_ALL )
		{
			m_pResultDialog->Hide();
		}
	}
}

void AmeMeasurePlugInGUI::DrawScaleOnCorner( AmeImageCanvas* viewer, bool corner_fg )
{
	m_pEngine->GetMeasureValues( viewer->GetViewerAttr()->GetImageAttr() );
	std::map<int, AmeMeasureDrawObject*>::iterator it = m_pEngine->m_map_measuredLength.begin();
	//目盛り間隔表示
	bool b_flg = false;
	int _div = m_pEngine->m_UserParams.m_iScaleDivLength;
	for ( ; it != m_pEngine->m_map_measuredLength.end(); ++it )
	{

		int _type = it->second->GetType();
		if ( _type > AME_MEASURE_POLYLINE && _type != AME_MEASURE_CURVE )continue;
		if ( it->second->IsVisible( viewer ) && _div != 0 )
		{
			if ( it->second->GetCurrentColorSetting().getLineOption() == LINE_OPTION_SCALE )
			{
				b_flg = true;
				break;
			}
		}
	}

	//目盛り間隔用ID
	AmeString scale_id = L"linemeasure_scale";
	if ( b_flg && corner_fg )
	{
		viewer->GetViewerAttr()->GetCornerContainer()->Add( scale_id, AmeViewerCornerContainer::CORNER_LEFT_BOTTOM );
		viewer->GetViewerAttr()->GetCornerContainer()->SetColor( scale_id, m_ScaleColor );
		AmeString scale_label = AppAttr->GetResStr( "AME_MEASURE_SHOW_DIVLENGTH" );
		if ( _div == 1 )
		{
			AmeString _scale = AmeString::Format( L" %.1fmm", m_StepSize[_div] );
			scale_label = scale_label + _scale;
		}
		else
		{
			AmeString _scale = AmeString::Format( L" %dmm", (int)m_StepSize[_div] );
			scale_label = scale_label + _scale;
		}
		viewer->GetViewerAttr()->GetCornerContainer()->SetString( scale_id, scale_label );
	}
	else
	{
		viewer->GetViewerAttr()->GetCornerContainer()->Delete( scale_id );
	}
}

// 計測結果の画面表示 識別ID更新
void AmeMeasurePlugInGUI::UpdateLineMeasureResults( AmeImageCanvas* viewer, AmeMeasureDrawObject* obj, bool bDraw )
{
	m_pEngine->GetMeasureValues( viewer->GetViewerAttr()->GetImageAttr() );
	std::map<int, AmeMeasureDrawObject*>::iterator it = m_pEngine->m_map_measuredLength.begin();

	for ( ; it != m_pEngine->m_map_measuredLength.end(); ++it )
	{
		if ( it->second->GetIndex() == obj->GetIndex() )break;
	}


	if ( it != m_pEngine->m_map_measuredLength.end() )
	{
		//☆ 計測結果の識別ID作成
		AmeString str_id = AmeString::Format( L"linemeasure_%d", it->first );
		if ( it->second->IsVisible( viewer ) && bDraw )
		{
			if ( !viewer->GetViewerAttr()->GetCornerContainer()->IsItemContained( str_id ) )
			{
				viewer->GetViewerAttr()->GetCornerContainer()->Add( str_id, AmeViewerCornerContainer::CORNER_LEFT_BOTTOM );
			}
			//左下に描画する

			AmeString _length = AmeString::Format( L"%2d: %.1f mm", it->first, it->second->m_Result.Length );
			PnwColor color( it->second->GetCurrentColorSetting().getFgColor() );
			viewer->GetViewerAttr()->GetCornerContainer()->SetColor( str_id, AmeRGBColor( PNW_RED( color ), PNW_GREEN( color ), PNW_BLUE( color ) ) );
			viewer->GetViewerAttr()->GetCornerContainer()->SetString( str_id, _length );
		}
		else if ( viewer->GetViewerAttr()->GetCornerContainer()->IsItemContained( str_id ) )
		{
			viewer->GetViewerAttr()->GetCornerContainer()->Delete( str_id );
		}
	}
	SortLineMeasureResults( viewer );
}

// 計測結果の四隅表示のソート
void AmeMeasurePlugInGUI::SortLineMeasureResults( AmeImageCanvas* viewer )
{
	//別のベクタにコピー
	std::vector<AmeViewerCornerContainer::CornerItem>v_cornerItem;
	v_cornerItem.resize( viewer->GetViewerAttr()->GetCornerContainer()->GetAllItems().size() );
	std::copy(
		viewer->GetViewerAttr()->GetCornerContainer()->GetAllItems().begin(),
		viewer->GetViewerAttr()->GetCornerContainer()->GetAllItems().end(),
		v_cornerItem.begin()
	);
	//四隅情報を全削除
	std::map<int, AmeViewerCornerContainer::CornerItem>_cornerMap;
	for ( std::vector<AmeViewerCornerContainer::CornerItem>::iterator it = v_cornerItem.begin(); it != v_cornerItem.end(); ++it )
	{
		std::map<int, AmeMeasureDrawObject*>::iterator jt = m_pEngine->m_map_measuredLength.begin();
		for ( ; jt != m_pEngine->m_map_measuredLength.end(); ++jt )
		{
			AmeString str_id = AmeString::Format( L"linemeasure_%d", jt->first );
			if ( str_id == it->m_ItemID )
			{
				_cornerMap.insert( std::pair<int, AmeViewerCornerContainer::CornerItem>( jt->first, *it ) );
				viewer->GetViewerAttr()->GetCornerContainer()->Delete( it->m_ItemID );
				break;
			}
		}
	}
	//四隅情報を再登録
	std::map<int, AmeViewerCornerContainer::CornerItem>::reverse_iterator mt = _cornerMap.rbegin();
	for ( ; mt != _cornerMap.rend(); ++mt )
	{
		viewer->GetViewerAttr()->GetCornerContainer()->Add( mt->second.m_ItemID, AmeViewerCornerContainer::CORNER_LEFT_BOTTOM );
		viewer->GetViewerAttr()->GetCornerContainer()->SetColor( mt->second.m_ItemID, mt->second.m_Color );
		viewer->GetViewerAttr()->GetCornerContainer()->SetString( mt->second.m_ItemID, mt->second.m_String );
	}
}

// 現在のカーソルを取得
PnwCursor* AmeMeasurePlugInGUI::GetDefaultCursor( PnwEventArgs*, AmeImageViewer* )
{
	if ( m_bFailedToCreate )
	{
		return AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
	}

	return m_pnwCurrentCursor;
}

void AmeMeasurePlugInGUI::ChangeMouseMode( int new_mouse_mode )
{
	if ( new_mouse_mode == this->GetPlugInID() )
	{

		UpdateInterface();
		AmeCheckSUVCalculationCondition( GetTaskManager(), m_pViewerAttr );

		//ツールバー対策(複数ボタンをツールバーに登録するので、この制御が必要)
		UpdateToolBarShortCutBtn();

	}
	else
	{
		if ( m_bNowCopying )
		{
			CancelCopy();
		}

		//現在計測モードで、次に違うモードに移動するなら
		if ( GetTaskManager()->GetMouseMode() == this->GetPlugInID() )
		{
			for ( int i = 0; i < AME_MEASURE_NUM; i++ )
			{
				if ( m_pnwShortCutBtn[i] != NULL )
				{
					m_pnwShortCutBtn[i]->SetButtonState( false );
				}
			}
			///他のプラグインに移動すると、計測方法は戻す。（ショートカットから計測を選択して、他のモードに変えた場合の対処）
			ChangeMeasuremType(AME_MEASURE_NONE);
		}


	}
}


bool AmeMeasurePlugInGUI::GetDrawObjectPriorityHigh( AmeImageViewer*, PnwEventArgs* )
{
	if ( !m_bNowCreating && (m_CurrentType == AME_MEASURE_CLOSEST || m_CurrentType == AME_MEASURE_DIAMETER) )
	{
		return true;
	}
	return (m_bNowCreating || m_bNowCopying || m_CurrentType != AME_MEASURE_NONE) ? false : true;
}


// ソートファンクター
class MeasureObjectSortFunc
{
public:
	bool operator()( const AmeMeasureDrawObject* x, const AmeMeasureDrawObject* y ) const
	{
		return x->GetIndex() < y->GetIndex();
	}
};


void AmeMeasurePlugInGUI::UpdateInterfaceAfterRegisterTaskCard()
{
	ApplySetting();
}


void AmeMeasurePlugInGUI::UnRegisterObject()
{
	// 描画オブジェクトをTaskManagerから抜く
	for ( size_t i = 0; i < m_pEngine->m_AllObjects.size(); i++ )
	{
		AmeDrawObject* obj = m_pEngine->m_AllObjects[i];

		obj->SetOwner( NULL, GetPlugInID() );
		if ( obj->GetObjectID() != -1 )
		{
			GetTaskManager()->RemoveDrawObject( obj->GetObjectID() );
		}
	}
}


int AmeMeasurePlugInGUI::UpdateInterfaceAfterLoadingSnapshot()
{
	// Index順にソートする
	std::sort( m_pEngine->m_AllObjects.begin(), m_pEngine->m_AllObjects.end(), MeasureObjectSortFunc() );

	// 描画オブジェクトをTaskManagerに登録しておく
	for ( size_t i = 0; i < m_pEngine->m_AllObjects.size(); i++ )
	{
		AmeDrawObject* obj = m_pEngine->m_AllObjects[i];

		obj->SetOwner( GetTaskManager(), GetPlugInID() );
		if ( obj->GetObjectID() == -1 )
		{
			GetTaskManager()->AddDrawObject( obj );
		}
		// objがAmeMeasureDrawObjectであることが前提。snapshot読み込み用だが要見直し。
		dynamic_cast<AmeMeasureDrawObject*>(obj)->GetCurrentColorSetting().Init();

	}

	UpdateInterface();

	//設定反映
	ApplySetting();

	return AME_ERROR_SUCCESS;
}


// LiveWire処理の終了
bool AmeMeasurePlugInGUI::FinishFreehandLiveWire( AmeViewerAttr* attr, AmeMeasureRoi* roi )
{
	if ( m_LiveWire.ViewerAttrID != attr->GetViewerAttrID() )
	{
		return false;
	}

	AmeImageViewer* viewer = attr->GetViewer();
	if ( viewer == nullptr )
	{
		return false;
	}

	// 点列の追加
	AmeFloat3D axisX = viewer->GetViewerAxisX();
	AmeFloat3D axisY = viewer->GetViewerAxisY();
	switch ( viewer->GetViewerType() )
	{
		case AME_VIEWER_XY:
		case AME_VIEWER_YZ:
		case AME_VIEWER_XZ:
		{
			AmeInt3D currentpos = attr->m_CurrentPosition;
			AmeInt3D lastpos = currentpos;
			int size = (int)m_LiveWire.Path.size();
			for ( int i = 0; i < size; i++ )
			{
				AmeFloat3D vec;
				switch ( viewer->GetViewerType() )
				{
					case AME_VIEWER_XY:
						vec = AmeFloat3D( m_LiveWire.Path[i].X, m_LiveWire.Path[i].Y, currentpos.Z );
						break;
					case AME_VIEWER_XZ:
						vec = AmeFloat3D( m_LiveWire.Path[i].X, currentpos.Y, m_LiveWire.Path[i].Y );
						break;
					case AME_VIEWER_YZ:
						vec = AmeFloat3D( currentpos.X, m_LiveWire.Path[i].X, m_LiveWire.Path[i].Y );
						break;
				}
				AmeFloat3D disp_vec = ConvertVoxelToIso( vec - roi->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );
				roi->m_vecPoints.push_back( AmeFloat2D( axisX.InnerProduct( disp_vec ), axisY.InnerProduct( disp_vec ) ) );

				if ( i == size - 1 )
				{
					lastpos = vec.ToInt();
				}
			}
			m_LiveWire.Counter.push_back( size );
			m_LiveWire.SeedList.push_back( lastpos );
		}
		break;
		case AME_VIEWER_OBLIQUE:
		{
			AmeInt3D lastpos = roi->m_BasePoint.ToRoundInt();
			for ( size_t i = 0; i < m_LiveWire.ObliquePath.size(); i++ )
			{
				AmeFloat3D disp_vec = ConvertVoxelToIso( m_LiveWire.ObliquePath[i] - roi->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );
				roi->m_vecPoints.push_back( AmeFloat2D( axisX.InnerProduct( disp_vec ), axisY.InnerProduct( disp_vec ) ) );

				if ( i + 1 == m_LiveWire.ObliquePath.size() ) lastpos = m_LiveWire.ObliquePath[i].ToRoundInt();
			}
			m_LiveWire.Counter.push_back( (int)m_LiveWire.ObliquePath.size() );
			m_LiveWire.SeedList.push_back( lastpos );
		}
		break;
	}

	// 重心の調整
	roi->AdjustGravity( viewer );

	AmeFloat2D pos;
	viewer->VoxelToDisplay( roi->m_BasePoint, pos );

	ResetLiveWire();
	return true;
}


// LiveWire処理の開始
bool AmeMeasurePlugInGUI::BeginLiveWire( AmeViewerAttr* attr, const AmeInt3D& pos )
{
	m_LiveWire.Seed = pos;
	m_LiveWire.Proc.SetApplyWLFlag( true );
	m_LiveWire.Proc.SetWL( attr->m_WindowLevel.Width, attr->m_WindowLevel.Level );
	m_LiveWire.Proc.SetWeight( 0.2f, 0.7f, 0.1f );
	m_LiveWire.Path.clear();
	m_LiveWire.ObliquePath.clear();
	m_LiveWire.ImageAttrID = attr->GetImageAttr()->GetImageAttrID();
	m_LiveWire.ViewerAttrID = attr->GetViewerAttrID();

	m_LiveWire.Proc.SetImage( attr->GetImageAttr()->GetImage() );

	AmeImageViewer* pViewer = attr->GetViewer();
	if ( pViewer == nullptr )
	{
		return false;
	}
	switch ( pViewer->GetViewerType() )
	{
		case AME_VIEWER_XY:
			// LiveWireモード
			m_LiveWire.Proc.SetTargetPlane( AmeLiveWire3DHelper::AME_LIVEWIRE_XY );
			m_LiveWire.Proc.Execute( m_LiveWire.Seed.ToSize() );
			m_LiveWire.Path.push_back( AmeInt2D( m_LiveWire.Seed.X, m_LiveWire.Seed.Y ) );
			return true;
		case AME_VIEWER_XZ:
			// LiveWireモード
			m_LiveWire.Proc.SetTargetPlane( AmeLiveWire3DHelper::AME_LIVEWIRE_XZ );
			m_LiveWire.Proc.Execute( m_LiveWire.Seed.ToSize() );
			m_LiveWire.Path.push_back( AmeInt2D( m_LiveWire.Seed.X, m_LiveWire.Seed.Z ) );
			return true;
		case AME_VIEWER_YZ:
			// LiveWireモード
			m_LiveWire.Proc.SetTargetPlane( AmeLiveWire3DHelper::AME_LIVEWIRE_YZ );
			m_LiveWire.Proc.Execute( m_LiveWire.Seed.ToSize() );
			m_LiveWire.Path.push_back( AmeInt2D( m_LiveWire.Seed.Y, m_LiveWire.Seed.Z ) );
			return true;
		case AME_VIEWER_OBLIQUE:
			m_pObliqueLiveWire->SetImage( attr->GetImageAttr()->GetImage() );
			m_pObliqueLiveWire->SetObliqueParam( attr->m_DisplayCenter, attr->m_RotationMatrix );
			m_pObliqueLiveWire->StartLiveWire( m_LiveWire.Seed.ToFloat(), attr->m_WindowLevel.Width, attr->m_WindowLevel.Level );
			//m_LiveWire.ObliquePath.push_back(m_LiveWire.Seed.ToFloat());
			return true;
		default:
			ResetLiveWire();
			m_bNowCreating = false;
			break;
	}

	if ( m_LiveWire.SeedList.size() > 0 )
	{
		m_LiveWire.SeedList[m_LiveWire.SeedList.size() - 1] = pos;
	}

	return false;
}


// LiveWire処理の初期化
void AmeMeasurePlugInGUI::ResetLiveWire()
{
	m_LiveWire.Proc.Stop();
	m_LiveWire.Path.clear();
	m_LiveWire.ObliquePath.clear();
	m_LiveWire.ImageAttrID = -1;
	m_LiveWire.ViewerAttrID = -1;

	if ( m_pObliqueLiveWire != nullptr )
	{
		m_pObliqueLiveWire->EndLiveWire();
	}
}

// ビューア使用許可
bool AmeMeasurePlugInGUI::PermitViewerType( const AME_VIEWER_TYPE viewer_type )
{
	switch ( viewer_type )
	{
		case AME_VIEWER_XY:
		case AME_VIEWER_YZ:
		case AME_VIEWER_XZ:
		case AME_VIEWER_VOLUME:
		case AME_VIEWER_OBLIQUE:
		case AME_VIEWER_MULTI_3D:
		case AME_VIEWER_CPR:
		case AME_VIEWER_SURFACE:
			return true;
		default:
			return false;
	}
}


// UI concept modeの切替
void AmeMeasurePlugInGUI::UpdateInterfaceForUIConceptMode()
{
	switch ( GetTaskManager()->GetUIConceptMode() )
	{
		case AME_UI_CONCEPT_MODE::DEEP:
		{
			// 直線
			if (m_pnwMeasureLineTypeFrame != nullptr && m_pnwMeasureLineTypeFrame->GetNumChildren() > 0 )
			{
				PnwHorizontalFrame* hFrame = dynamic_cast<PnwHorizontalFrame*>(m_pnwMeasureLineTypeFrame->GetChild( 0 ));
				m_pnwMeasureMethodBtn[AME_MEASURE_LINE]->SetParent( hFrame );
				m_pnwMeasureMethodBtn[AME_MEASURE_PROJ_LINE]->SetParent( hFrame );
				m_pnwMeasureMethodBtn[AME_MEASURE_CLOSEST]->SetParent(hFrame);
				m_pnwMeasureMethodBtn[AME_MEASURE_DIAMETER]->SetParent( hFrame );
			}
			// 折れ線
			if (m_pnwMeasurePolylineTypeFrame != nullptr && m_pnwMeasurePolylineTypeFrame->GetNumChildren() > 0 )
			{
				PnwHorizontalFrame* hFrame = dynamic_cast<PnwHorizontalFrame*>(m_pnwMeasurePolylineTypeFrame->GetChild( 0 ));
				m_pnwMeasureMethodBtn[AME_MEASURE_POLYLINE]->SetParent( hFrame );
				m_pnwMeasureMethodBtn[AME_MEASURE_CURVE]->SetParent( hFrame );
			}
			// 角度
			if (m_pnwMeasureAngleTypeFrame != nullptr && m_pnwMeasureAngleTypeFrame->GetNumChildren() > 0 )
			{
				PnwHorizontalFrame* hFrame = dynamic_cast<PnwHorizontalFrame*>(m_pnwMeasureAngleTypeFrame->GetChild( 0 ));
				m_pnwMeasureMethodBtn[AME_MEASURE_ANGLE]->SetParent( hFrame );
				m_pnwMeasureMethodBtn[AME_MEASURE_TWO_LINE_ANGLE]->SetParent( hFrame );
				m_pnwMeasureMethodBtn[AME_MEASURE_PROJ_ANGLE]->SetParent( hFrame );
			}
			// その他
			if (m_pnwMeasureLineBtn != nullptr && m_pnwMeasureLineBtn->GetParent() != nullptr &&
				m_pnwMeasureLineBtn->GetParent()->GetParent() != nullptr &&
				m_pnwMeasureLineBtn->GetParent()->GetParent()->GetParent() != nullptr )
			{
				// Floating中の場合に0となるため、m_pnwMeasureMethodGroup->GetNumChildren()は使わない
				PnwWidget* measureMethodGroupBox = m_pnwMeasureLineBtn->GetParent()->GetParent()->GetParent();
				if ( measureMethodGroupBox->GetNumChildren() > 0 )
				{
					PnwVerticalFrame* vFrame = dynamic_cast<PnwVerticalFrame*>(measureMethodGroupBox->GetChild( 0 ));
					if ( vFrame != nullptr && vFrame->GetNumChildren() > 1 )
					{
						{
							PnwHorizontalFrame* hFrame = dynamic_cast<PnwHorizontalFrame*>(vFrame->GetChild( 0 ));
							m_pnwMeasureLineBtn->Show();
							m_pnwMeasurePolylineBtn->Show();
							m_pnwMeasureAngleBtn->Show();

							m_pnwMeasureMethodBtn[AME_MEASURE_POINT]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_CUBE]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_POINT]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_CUBE]->SetParent( hFrame );
						}
						{
							PnwHorizontalFrame* hFrame = dynamic_cast<PnwHorizontalFrame*>(vFrame->GetChild( 1 ));
							m_pnwMeasureMethodBtn[AME_MEASURE_ELLIPSE]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_ROI]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_FREEHAND]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_BOX]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE]->SetParent( nullptr );

							m_pnwMeasureMethodBtn[AME_MEASURE_ELLIPSE]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_ROI]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_FREEHAND]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_BOX]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE]->SetParent( hFrame );
						}
					}
				}
			}
		}
		break;
		case AME_UI_CONCEPT_MODE::QUICK:
		{
			m_pnwMeasureLineBtn->Hide();
			m_pnwMeasurePolylineBtn->Hide();
			m_pnwMeasureAngleBtn->Hide();

			if ( m_pnwMeasureLineBtn->GetParent() != nullptr &&
				m_pnwMeasureLineBtn->GetParent()->GetParent() != nullptr &&
				m_pnwMeasureLineBtn->GetParent()->GetParent()->GetParent() != nullptr )
			{
				// Floating中の場合に0となるため、m_pnwMeasureMethodGroup->GetNumChildren()は使わない
				PnwWidget* measureMethodGroupBox = m_pnwMeasureLineBtn->GetParent()->GetParent()->GetParent();
				if ( measureMethodGroupBox->GetNumChildren() > 0 )
				{
					PnwVerticalFrame* vFrame = dynamic_cast<PnwVerticalFrame*>(measureMethodGroupBox->GetChild( 0 ));
					if ( vFrame != nullptr && vFrame->GetNumChildren() > 1 )
					{
						{
							PnwHorizontalFrame* hFrame = dynamic_cast<PnwHorizontalFrame*>(vFrame->GetChild( 0 ));
							// 直線
							m_pnwMeasureMethodBtn[AME_MEASURE_LINE]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_PROJ_LINE]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_CLOSEST]->SetParent(hFrame);
							m_pnwMeasureMethodBtn[AME_MEASURE_DIAMETER]->SetParent( hFrame );
							// 折れ線
							m_pnwMeasureMethodBtn[AME_MEASURE_POLYLINE]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_CURVE]->SetParent( hFrame );
							// 角度
							m_pnwMeasureMethodBtn[AME_MEASURE_ANGLE]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_TWO_LINE_ANGLE]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_PROJ_ANGLE]->SetParent( hFrame );
						}
						{
							// その他
							PnwHorizontalFrame* hFrame = dynamic_cast<PnwHorizontalFrame*>(vFrame->GetChild( 1 ));
							m_pnwMeasureMethodBtn[AME_MEASURE_POINT]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_CUBE]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_ELLIPSE]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_ROI]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_FREEHAND]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_BOX]->SetParent( nullptr );
							m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE]->SetParent( nullptr );

							m_pnwMeasureMethodBtn[AME_MEASURE_POINT]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_CUBE]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_ELLIPSE]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_ROI]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_FREEHAND]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_BOX]->SetParent( hFrame );
							m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE]->SetParent( hFrame );
							
						}
					}

				}
			}
		}
		break;
		default:
			break;
	}
}


//ショートカットからの呼び出し
long AmeMeasurePlugInGUI::onCmdShortCut( PnwObject* obj, PnwEventType, PnwEventArgs* )
{
	PnwObject* target_obj = obj;
	//モード解除するためにボタンをおされたようにする。
	int type = PtrToInt( obj->GetUserData() );
	if ( m_CurrentType == type )
	{
		switch ( type )
		{
			case AME_MEASURE_LINE:
			case AME_MEASURE_PROJ_LINE:
			case AME_MEASURE_TTTG:
			{
				target_obj = m_pnwMeasureLineBtn;
			}
			break;
			case AME_MEASURE_POLYLINE:
			case AME_MEASURE_CURVE:
			{
				target_obj = m_pnwMeasurePolylineBtn;
			}
			break;
			case AME_MEASURE_ANGLE:
			case AME_MEASURE_PROJ_ANGLE:
			case AME_MEASURE_TWO_LINE_ANGLE:
			{
				target_obj = m_pnwMeasureAngleBtn;
			}
			break;
		}

		type = AME_MEASURE_NONE;
	}

	ChangeMeasuremType( (AmeMeasureType)type );

	return 1;
}


void AmeMeasurePlugInGUI::ChangeMeasuremType( AmeMeasureType type )
{
	m_CurrentType = type;

	// 作成中の計測があれば確定
	if ( m_bNowCreating )
	{
		FinishCreatingMeasure();
	}

	// コピー中の計測があれば取りやめ
	CancelCopy();

	// 新規作成するときはカレントをなくす
	if ( m_CurrentType != AME_MEASURE_NONE )
	{
		ChangeCurrentByID( -1 );
	}

	SetCurrentColorSetting();

	UpdateInterface();
	UpdateInterfaceForShortcutBar( m_pViewerAttr );

	//ツールバー対策(複数ボタンをツールバーに登録するので、この制御が必要)
	UpdateToolBarShortCutBtn();

	if ( type == AME_MEASURE_CLOSEST || type == AME_MEASURE_DIAMETER )
	{
		if ( m_ROIData.m_BoxROIDrawer == nullptr )
		{
			m_ROIData.m_BoxROIDrawer = ROIDrawObjectFactory::CreateBoxROI(*AppAttr, *GetTaskManager(), GetPlugInID(), *m_pViewerAttr->GetImageAttr() );
			if ( m_ROIData.m_BoxROIDrawer == nullptr )
			{
				assert( false );
				return;
			}
			m_ROIData.m_BoxROIDrawer->SetCallbackOnMotion(std::bind(&AmeMeasurePlugInGUI::OnMotionClosestBoxROI, this, std::placeholders::_1, std::placeholders::_2));
			GetTaskManager()->AddDrawObject( m_ROIData.m_BoxROIDrawer.get() );
		}
		m_ROIData.m_BoxROIDrawer->ClearPopupMenu(); // メニューを一旦削除
		m_ROIData.m_BoxROIDrawer->AddPopupMenu(AppAttr->GetResStr("AME_COMMON_DELETE"), AppAttr->GetAppIcon(AME_ICON_COMMON_DELETE_ONE), this, ID_DELETE_CLOSEST_BOX_ROI);
		if (type == AME_MEASURE_CLOSEST)
		{
			m_ROIData.m_BoxROIDrawer->AddPopupMenu(AppAttr->GetResStr("AME_MEASURE_EXECUTE_CLOSEST_SEARCH"), AppAttr->GetAppIcon(AME_ICON_CHECK), this, ID_EXECUTE_CLOSEST);
		}
		else
		{
			m_ROIData.m_BoxROIDrawer->AddPopupMenu(AppAttr->GetResStr("AME_MEASURE_EXECUTE_DIAMETER"), AppAttr->GetAppIcon(AME_ICON_CHECK), this, ID_EXECUTE_DIAMETER);
		}
		if ( m_ROIData.m_RectROIDrawer == nullptr )
		{
			m_ROIData.m_RectROIDrawer = ROIDrawObjectFactory::CreateRectROI(*AppAttr, *GetTaskManager(), GetPlugInID(), *m_pViewerAttr->GetImageAttr() );
			if ( m_ROIData.m_RectROIDrawer == nullptr )
			{
				assert( false );
				return;
			}
			GetTaskManager()->AddDrawObject( m_ROIData.m_RectROIDrawer.get() );
		}
		m_ROIData.m_RectROIDrawer->ClearPopupMenu(); // メニューを一旦削除
		m_ROIData.m_RectROIDrawer->AddPopupMenu(AppAttr->GetResStr("AME_COMMON_DELETE"), AppAttr->GetAppIcon(AME_ICON_COMMON_DELETE_ONE), this, ID_DELETE_CLOSEST_RECT_ROI);
		if (type == AME_MEASURE_CLOSEST)
		{
			m_ROIData.m_RectROIDrawer->AddPopupMenu(AppAttr->GetResStr("AME_MEASURE_EXECUTE_CLOSEST_SEARCH"), AppAttr->GetAppIcon(AME_ICON_CHECK), this, ID_EXECUTE_CLOSEST);
		}
		else
		{
			m_ROIData.m_RectROIDrawer->AddPopupMenu(AppAttr->GetResStr("AME_MEASURE_EXECUTE_DIAMETER"), AppAttr->GetAppIcon(AME_ICON_CHECK), this, ID_EXECUTE_DIAMETER);
		}
	}
	else
	{
		if ( m_ROIData.m_BoxROIDrawer != nullptr )
		{
			m_ROIData.m_BoxROIDrawer->EnableDisplay( false );
		}
		if ( m_ROIData.m_RectROIDrawer != nullptr )
		{
			m_ROIData.m_RectROIDrawer->EnableDisplay( false );
		}
	}
	// 計測が選択状態ならフォーカスをはずす
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
}


// 計測タイプの切り替え
long AmeMeasurePlugInGUI::onCmdMeasureType( PnwObject* obj, PnwEventType, PnwEventArgs* )
{
	int type = PtrToInt( obj->GetUserData() );

	// 強制的に自分のモードに変える
	if ( !IsCurrentMouseMode() )
	{
		GetTaskManager()->SetMouseMode( GetPlugInID() );
	}

	if ( m_CurrentType == type )
	{
		switch ( type )
		{
			case AME_MEASURE_LINE:
			case AME_MEASURE_PROJ_LINE:
			case AME_MEASURE_TTTG:
				if ( obj != m_pnwMeasureLineBtn )
				{
					// 直線ボタンでなければ何もしない(右クリックメニューで同じものを押しても解除しないように)
					return 1;
				}
				break;
			case AME_MEASURE_POLYLINE:
			case AME_MEASURE_CURVE:
				if ( obj != m_pnwMeasurePolylineBtn )
				{
					// 折れ線ボタンでなければ何もしない(右クリックメニューで同じものを押しても解除しないように)
					return 1;
				}
				break;
			case AME_MEASURE_ANGLE:
			case AME_MEASURE_PROJ_ANGLE:
			case AME_MEASURE_TWO_LINE_ANGLE:
				if ( obj != m_pnwMeasureAngleBtn )
				{
					// 角度ボタンでなければ何もしない(右クリックメニューで同じものを押しても解除しないように)
					return 1;
				}
				break;
		}
		type = AME_MEASURE_NONE;
	}

	ChangeMeasuremType( (AmeMeasureType)type );

	return 1;
}


//ツールバー対策(複数ボタンをツールバーに登録するので、この制御が必要)
void AmeMeasurePlugInGUI::UpdateToolBarShortCutBtn()
{
	if ( GetTaskManager() == nullptr || GetTaskManager()->GetShortcutBar() == nullptr )
	{
		return;
	}

	if ( m_CurrentType != AME_MEASURE_NONE )
	{
		GetTaskManager()->GetShortcutBar()->SetShortCutID( AmeMeasureTypeString[m_CurrentType] );
	}
	else
	{
		//解除
		AmeString id = GetTaskManager()->GetShortcutBar()->GetShortCutID();
		for ( int i = 0; i < AME_MEASURE_NUM; i++ )
		{
			if ( id == AmeMeasureTypeString[i] )
			{
				GetTaskManager()->GetShortcutBar()->SetShortCutID( L"" );
				break;
			}
		}
	}

}


/*
long AmeMeasurePlugInGUI::onCmdMeasureAngleType(PnwObject*, PnwEventType, PnwEventArgs*)
{
	PnwButton *btn = (PnwButton*)obj;
	PnwMenuPane pane(GetTaskManager());

	//登録
	std::map<int,AmeString>m_namemap;
	m_namemap.insert(std::pair<int, AmeString>(AME_MEASURE_ANGLE,          AmeString(L"AME_MEASURE_METHOD_ANGLE_3POINT")));			//GetResStr 不要文字列ヒット用コメント
	m_namemap.insert(std::pair<int, AmeString>(AME_MEASURE_PROJ_ANGLE,     AmeString(L"AME_MEASURE_METHOD_ANGLE_PROJ_3POINT")));	//GetResStr 不要文字列ヒット用コメント
	m_namemap.insert(std::pair<int, AmeString>(AME_MEASURE_TWO_LINE_ANGLE, AmeString(L"AME_MEASURE_METHOD_ANGLE_4POINT")));			//GetResStr 不要文字列ヒット用コメント

	//マップに格納する
	std::map<int, PnwMenuCommand*>m_pnwMeasureMethodMenu;
	for(std::map<int, AmeString>::iterator n_it= m_namemap.begin(); n_it!=m_namemap.end(); ++n_it)
	{
		m_pnwMeasureMethodMenu.insert(
			std::pair<int,PnwMenuCommand*>(
				n_it->first,
				new PnwMenuCommand(
					&pane,
					AppAttr->GetResStr(n_it->second.Text()),
					m_pnwMeasureIcon[n_it->first],
					this,
					ID_MEASURE_TYPE
				)
			)
		);
		m_pnwMeasureMethodMenu[n_it->first]->SetUserData( IntToPtr(n_it->first) );
	}

	for(std::map<int, PnwMenuCommand*>::iterator it=m_pnwMeasureMethodMenu.begin(); it!=m_pnwMeasureMethodMenu.end(); ++it)
	{
		it->second->SetActiveState(m_CurrentType == it->first);
		bool bValidScale;
		if (m_pViewerAttr != NULL &&
			m_pViewerAttr->GetViewer() != NULL &&
			IsMeasureCreatable((AmeMeasureType) it->first, m_pViewerAttr->GetViewer(), bValidScale)
			)
		{
			it->second->SetEnabled(true);
		} else {
			it->second->SetEnabled(false);
		}
	}

#if 0
	m_pnwMeasureMethodMenu[AME_MEASURE_ANGLE] = new PnwMenuCommand(&pane, AppAttr->GetResStr("AME_MEASURE_METHOD_ANGLE_3POINT"), m_pnwMeasureIcon[AME_MEASURE_ANGLE], this, ID_MEASURE_TYPE);
	m_pnwMeasureMethodMenu[AME_MEASURE_ANGLE]->SetUserData( IntToPtr(AME_MEASURE_ANGLE) );
	m_pnwMeasureMethodMenu[AME_MEASURE_TWO_LINE_ANGLE] = new PnwMenuCommand(&pane, AppAttr->GetResStr("AME_MEASURE_METHOD_ANGLE_4POINT"), m_pnwMeasureIcon[AME_MEASURE_TWO_LINE_ANGLE], this, ID_MEASURE_TYPE);
	m_pnwMeasureMethodMenu[AME_MEASURE_TWO_LINE_ANGLE]->SetUserData( IntToPtr(AME_MEASURE_TWO_LINE_ANGLE) );
	m_pnwMeasureMethodMenu[AME_MEASURE_PROJ_ANGLE] = new PnwMenuCommand(&pane, AppAttr->GetResStr("AME_MEASURE_METHOD_ANGLE_3POINT"), m_pnwMeasureIcon[AME_MEASURE_PROJ_ANGLE], this, ID_MEASURE_TYPE);
	m_pnwMeasureMethodMenu[AME_MEASURE_PROJ_ANGLE]->SetUserData( IntToPtr(AME_MEASURE_PROJ_ANGLE) );

	for(int i=0; i<AME_MEASURE_NUM; i++){
		if(m_pnwMeasureMethodMenu[i] != NULL){
			m_pnwMeasureMethodMenu[i]->SetActiveState(m_CurrentType == i);

			bool bValidScale;
			if (m_pViewerAttr != NULL && m_pViewerAttr->GetViewer() != NULL && IsMeasureCreatable((AmeMeasureType) i, m_pViewerAttr->GetViewer(), bValidScale)){
				m_pnwMeasureMethodMenu[i]->SetEnabled(true);
			} else {
				m_pnwMeasureMethodMenu[i]->SetEnabled(false);
			}
		}
	}
#endif
	pane.PopupByWidget(btn, PnwMenuPane::POPUP_BOTTOM);

	return 1;
}
*/


// コピー中の計測を確定
void AmeMeasurePlugInGUI::FinishCopy( AmeImageViewer* viewer )
{
	AmeViewerAttr* attr = viewer->GetViewerAttr();

	bool validScale;
	if ( !IsMeasureCreatable( m_pCopiedMeasure->GetType(), viewer, validScale ) )
	{
		CancelCopy();
		return;
	}

	m_pCopiedMeasure->SetImageAttr( attr->GetImageAttr() );
	m_pCopiedMeasure->m_iOrgViewerType = viewer->GetViewerType();
	m_pCopiedMeasure->m_FrameSerialID = viewer->GetViewerFrame()->GetSerialID();
	m_pCopiedMeasure->m_ViewerIdentity = attr->GetViewerAttrIDString();
	m_pCopiedMeasure->m_ImageAttrIDString = attr->GetImageAttr()->GetImageAttrIDString();

	if ( viewer->GetOverlayViewerAttr() != NULL &&
	   (viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
		viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
	{
		m_pCopiedMeasure->m_OverlayImageIdentity = viewer->GetOverlayViewerAttr()->GetImageAttr()->GetImageAttrIDString();
	}
	else
	{
		m_pCopiedMeasure->m_OverlayImageIdentity = L"";
	}

	switch ( m_pCopiedMeasure->GetType() )
	{
		case AME_MEASURE_LINE:
			((AmeMeasureLine*)m_pCopiedMeasure)->m_bValidCPR = (viewer->GetViewerType() == AME_VIEWER_CPR);
			((AmeMeasureLine*)m_pCopiedMeasure)->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
			break;
		case AME_MEASURE_POINT:
			((AmeMeasurePoint*)m_pCopiedMeasure)->m_bValidCPR = (viewer->GetViewerType() == AME_VIEWER_CPR);
			((AmeMeasurePoint*)m_pCopiedMeasure)->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
			break;
	}


	int index = 0;
	if ( m_pEngine->m_AllObjects.size() != 0 )
	{
		index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
	}
	m_pCopiedMeasure->SetIndex( index );

	m_pEngine->m_AllObjects.push_back( m_pCopiedMeasure );
	GetTaskManager()->AddDrawObject( m_pCopiedMeasure );

	AmeMeasureDrawObject* drawobj = m_pCopiedMeasure;

	m_pCopiedMeasure = NULL;	// タスクに登録したのでポインタのみ外す

	CancelCopy();

	ChangeCurrentByID( drawobj->GetObjectID() );
}


// コピー中の計測があれば取りやめ
bool AmeMeasurePlugInGUI::CancelCopy()
{
	if ( !m_bNowCopying )
	{
		return false;
	}

	// コピー中の計測があれば取りやめ
	if ( m_pCopiedMeasure != NULL )
	{
		delete m_pCopiedMeasure;
		m_pCopiedMeasure = NULL;
	}

	m_bNowCopying = false;
	if ( m_frameMeasureList != NULL )
	{
		m_frameMeasureList->SetCopyButtonState( false );
	}

	return true;
}

// モダリティタイプ取得
AME_MODALITY_TYPE AmeMeasurePlugInGUI::GetModalityType( const int index )
{
	AmeViewerAttr* pViewerAttr = m_pViewerAttr;
	if ( index != 0 )
	{
		pViewerAttr = GetOverlayViewerAttr( pViewerAttr );
	}
	if ( pViewerAttr == NULL )
	{
		return MODALITY_TYPE_ERROR;
	}

	AmeString modality = AmeGetModalityString( pViewerAttr->GetImageAttr()->GetDICOMHeader( 0 ) );
	return AmeGetModalityType( modality );
}

// 削除
void AmeMeasurePlugInGUI::SlotCmdDelete()
{
	AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Deleting measurement." );

	// 削除
	DeleteSelectedMeasure();

	if ( m_pResultDialog != NULL )
	{
		m_pResultDialog->Hide();
		m_pResultDialog->InitDisplayRange();
	}
	//目盛り間隔表示の削除
	DeleteScaleOnCorner();

	UpdateInterface();
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
}


void AmeMeasurePlugInGUI::DeleteSelectedMeasure( void )
{
	// 作りかけのものがあればいったん確定
	if ( m_bNowCreating )
	{
		FinishCreatingMeasure();
	}

	// 選択されているものを削除
	std::vector<AmeViewerAttr*> attrlist = GetTaskManagerEngine()->GetAllViewerAttrs();
	std::vector<AmeMeasureDrawObject*>::iterator it;
	std::vector<AmeMeasureDrawObject*> delete_targets;
	for ( it = m_pEngine->m_AllObjects.begin(); it != m_pEngine->m_AllObjects.end(); it++ )
	{
		if ( (*it)->m_bSelected || *it == m_pEngine->GetCurrentMeasure( false ) )
		{
			//☆　四隅情報の削除
			m_pEngine->DeleteSelectedMeasureCornerInfo( *it );
			delete_targets.push_back( (*it) );
		}
	}

	for ( size_t i = 0; i < delete_targets.size(); i++ )
	{
		GetTaskManager()->DeleteDrawObject( delete_targets[i]->GetObjectID() );
	}

	ChangeCurrentByID( -1 );
}

// すべて削除
void AmeMeasurePlugInGUI::SlotCmdDeleteAll()
{
	if ( DIALOG_RESULT_OK != AmeDontAskAgainOKCancelQuestion( GetTaskManager(), GetPlugInName(), L"MeasureDeleteAll",
		AppAttr->GetResStr( "AME_COMMON_QUESTION" ), AppAttr->GetResStr( "AME_COMMON_MEASURE_ALL_DELETE_OK" ) ) )
	{
		return;
	}

	AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Deleting all measurements." );

	{
		if ( m_bNowCreating )
		{
			FinishCreatingMeasure();
		}

		CancelCopy();

		//☆　四隅情報の削除
		std::vector<AmeMeasureDrawObject*>::iterator it;
		for ( it = m_pEngine->m_AllObjects.begin(); it != m_pEngine->m_AllObjects.end(); ++it )
		{
			m_pEngine->DeleteSelectedMeasureCornerInfo( *it );
		}

		//目盛り間隔表示の削除
		DeleteScaleOnCorner();
		m_pEngine->m_map_measuredLength.clear();


		//回りくどいが、TaskManagerから再度、本プラグインのエンジンの削除にまわってきて、そのなかでm_AllObjectsの中身が変更されるので、一旦退避しておく必要がある。
		std::vector<AmeMeasureDrawObject*> delete_targets = m_pEngine->m_AllObjects;
		for ( size_t i = 0; i < delete_targets.size(); i++ )
		{
			GetTaskManager()->DeleteDrawObject( delete_targets[i]->GetObjectID() );
		}
		m_pEngine->m_AllObjects.clear();

		m_pEngine->ChangeCurrentByID( -1 );

		// 詳細ダイアログは隠す
		if ( m_pResultDialog != NULL )
		{
			m_pResultDialog->Hide();
			m_pResultDialog->InitDisplayRange();
		}

		SetCurrentColorSetting();
	}

	UpdateInterface();
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
}

// 目盛り間隔表示の削除
void AmeMeasurePlugInGUI::DeleteScaleOnCorner()
{
	std::vector<AmeViewerAttr*> attrlist = GetTaskManagerEngine()->GetAllViewerAttrs();
	AmeString str_id = AmeString::Format( L"linemeasure_scale" );
	std::vector<AmeViewerAttr*>::iterator vt = attrlist.begin();
	for ( ; vt != attrlist.end(); ++vt )
	{
		if ( (*vt)->GetCornerContainer()->IsItemContained( str_id ) )
		{
			(*vt)->GetCornerContainer()->Delete( str_id );
		}
	}
}

// この点を削除
long AmeMeasurePlugInGUI::onCmdDeletePoint( PnwObject*, PnwEventType, PnwEventArgs* )
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL )
		return 1;

	if ( m_iDeletingPoint == -1 )
		return 1;

	switch ( current->GetType() )
	{
		case AME_MEASURE_POLYLINE:
		{
			AmeMeasurePolyLine* polyline = static_cast<AmeMeasurePolyLine*>(current);
			polyline->m_vecPoint.erase( polyline->m_vecPoint.begin() + m_iDeletingPoint );
			break;
		}
		case AME_MEASURE_CURVE:
		{
			AmeMeasureCurve* curve = static_cast<AmeMeasureCurve*>(current);
			curve->m_vecPoint.erase( curve->m_vecPoint.begin() + m_iDeletingPoint );
			break;
		}
		case AME_MEASURE_ROI:
		{
			AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(current);
			roi->m_vecPoints.erase( roi->m_vecPoints.begin() + m_iDeletingPoint );
			break;
		}
	}

	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

	return 1;
}

// 複製
void AmeMeasurePlugInGUI::SlotCmdCopy()
{
	if ( m_pViewerAttr == NULL )
	{
		return;
	}

	// コピー中ならコピーを取りやめる
	if ( m_bNowCopying )
	{
		CancelCopy();
		GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
		return;
	}

	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL )
	{
		return;
	}

	// 作成中なら複製されない
	if ( m_bNowCreating )
	{
		PnwMessageBox::Warning( GetTaskManager(), MESSAGE_BUTTON_OK, AppAttr->GetResStr( "AME_COMMON_WARNING" ), AppAttr->GetResStr( "AME_MEASURE_NOT_COPY_ON_CREATING" ) );
		return;
	}

	AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Copying measurement. (type: %d)", current->GetType() );

	AmeMeasureDrawObject* base = NULL;
	AmeSize3D imageSize = m_pViewerAttr->GetImageAttr()->GetImageSize();
	AmeFloat3D offset( 0, 0, 0 );

	AmeImageViewer* pViewer = m_pViewerAttr->GetViewer();
	if ( pViewer != NULL )
	{
		offset = m_pViewerAttr->GetViewer()->GetViewerAxisX() * 10.f + m_pViewerAttr->GetViewer()->GetViewerAxisY() * 10.f;
	}

	AmeViewerAttr* src_attr = GetTaskManagerEngine()->SearchViewerAttrByID( current->m_ViewerIdentity );
	AmeFloatMatrix matrix;
	AmeFloat3D scale( 1, 1, 1 );
	if ( src_attr != NULL )
	{
		matrix = src_attr->m_RotationMatrix.Transpose();
		scale = src_attr->GetImageAttr()->GetVoxelSpacing();
	}
	m_CopiedPoints.clear();

	switch ( current->GetType() )
	{
		case AME_MEASURE_LINE:
		{
			AmeMeasureLine* line = new AmeMeasureLine( static_cast<AmeMeasureLine*>(current) );
			for ( int i = 0; i < 2; i++ )
			{
				line->m_vecPoint[i] = line->m_vecPoint[i] + offset;
			}
			base = line;

			AmeFloat3D center = (line->m_vecPoint[0] + line->m_vecPoint[1]) / 2;
			m_CopiedPoints.push_back( matrix * ((line->m_vecPoint[0] - center) * scale) );
			m_CopiedPoints.push_back( matrix * ((line->m_vecPoint[1] - center) * scale) );

			m_CopiedValidCPR = (line->m_iOrgViewerType == AME_VIEWER_CPR) ? line->m_bValidCPR : false;
			m_CopiedCPRPoint[0] = line->m_vecCPRPoint[0];
			m_CopiedCPRPoint[1] = line->m_vecCPRPoint[1];
		}
		break;
		case AME_MEASURE_PROJ_LINE:
		{
			//CTRL+C等でコピーされた状態だと、投影ポイントがDrawで内部変数が変わったままなので、強制的に更新する。
			AmeMeasureProjectedLine* org_line = static_cast<AmeMeasureProjectedLine*>(current);
			if ( pViewer != NULL )
			{
				org_line->SetProjectedPoints( pViewer, AmeInt2D( pViewer->GetWidth(), pViewer->GetHeight() ) );

				AmeMeasureProjectedLine* line = new AmeMeasureProjectedLine( org_line );
				m_CopiedProjPoints.clear();
				for ( int i = 0; i < (int)line->m_vecProjectedPoint.size(); i++ )
				{
					line->m_vecProjectedPoint[i].X = line->m_vecProjectedPoint[i].X + offset.X;
					line->m_vecProjectedPoint[i].Y = line->m_vecProjectedPoint[i].Y + offset.Y;
					line->StorePoints( m_pViewerAttr );

					AmeFloat2D _center;
					pViewer->VoxelToDisplay( m_pViewerAttr->GetRenderingInfo()->GetRotationCenter(), _center );
					m_CopiedProjPoints.push_back( line->m_vecProjectedPoint[i] - _center );
				}
				base = line;
			}
		}
		break;
		case AME_MEASURE_TTTG:
		{
			AmeMeasureTTTG* tttg = new AmeMeasureTTTG( static_cast<AmeMeasureTTTG*>(current) );
			for ( int i = 0; i < AmeMeasurePointArrayBase::CTRL_PT_NUM; i++ )
			{
				tttg->m_vecPoint[i] = tttg->m_vecPoint[i] + offset;
			}
			base = tttg;

			AmeFloat3D center = (tttg->m_vecPoint[AmeMeasurePointArrayBase::CTRL_PT_START] + tttg->m_vecPoint[AmeMeasurePointArrayBase::CTRL_PT_END]) / 2;
			for ( int i = 0; i < AmeMeasurePointArrayBase::CTRL_PT_NUM; i++ )
			{
				m_CopiedPoints.push_back( matrix * ((tttg->m_vecPoint[i] - center) * scale) );
			}
		}
		break;
		case AME_MEASURE_POLYLINE:
		{
			AmeMeasurePolyLine* line = new AmeMeasurePolyLine( static_cast<AmeMeasurePolyLine*>(current) );
			for ( int i = 0; i < (int)line->m_vecPoint.size(); i++ )
			{
				line->m_vecPoint[i].X = line->m_vecPoint[i].X + offset.X;
				line->m_vecPoint[i].Y = line->m_vecPoint[i].Y + offset.Y;
				line->m_vecPoint[i].Z = line->m_vecPoint[i].Z + offset.Z;

				m_CopiedPoints.push_back( matrix * ((line->m_vecPoint[i] - line->m_vecPoint[0]) * scale) );
			}
			base = line;
		}
		break;
		case AME_MEASURE_CURVE:
		{
			AmeMeasureCurve* curve = new AmeMeasureCurve( static_cast<AmeMeasureCurve*>(current) );
			for ( int i = 0; i < (int)curve->m_vecPoint.size(); i++ )
			{
				curve->m_vecPoint[i].X = curve->m_vecPoint[i].X + offset.X;
				curve->m_vecPoint[i].Y = curve->m_vecPoint[i].Y + offset.Y;
				curve->m_vecPoint[i].Z = curve->m_vecPoint[i].Z + offset.Z;

				m_CopiedPoints.push_back( matrix * ((curve->m_vecPoint[i] - curve->m_vecPoint[0]) * scale) );
			}
			base = curve;
		}
		break;	case AME_MEASURE_ANGLE:
		{
			AmeMeasureAngle* angle = new AmeMeasureAngle( static_cast<AmeMeasureAngle*>(current) );
			for ( int i = 0; i < (int)angle->m_vecPoint.size(); i++ )
			{
				angle->m_vecPoint[i].X = angle->m_vecPoint[i].X + offset.X;
				angle->m_vecPoint[i].Y = angle->m_vecPoint[i].Y + offset.Y;
				angle->m_vecPoint[i].Z = angle->m_vecPoint[i].Z + offset.Z;

				m_CopiedPoints.push_back( matrix * ((angle->m_vecPoint[i] - angle->m_vecPoint[0]) * scale) );
			}
			base = angle;
		}
		break;
		case AME_MEASURE_PROJ_ANGLE:
		{
			//CTRL+C等でコピーされた状態だと、投影ポイントがDrawで内部変数が変わったままなので、強制的に更新する。
			AmeMeasureProjAngle* org_angle = static_cast<AmeMeasureProjAngle*>(current);
			if ( pViewer != NULL )
			{
				org_angle->SetProjectedPoints( pViewer, AmeInt2D( pViewer->GetWidth(), pViewer->GetHeight() ) );

				AmeMeasureProjAngle* angle = new AmeMeasureProjAngle( org_angle );
				m_CopiedProjPoints.clear();

				for ( int i = 0; i < (int)angle->m_vecProjectedPoint.size(); i++ )
				{
					angle->m_vecProjectedPoint[i].X = angle->m_vecProjectedPoint[i].X + offset.X;
					angle->m_vecProjectedPoint[i].Y = angle->m_vecProjectedPoint[i].Y + offset.Y;
					angle->StorePoints( m_pViewerAttr );

					AmeFloat2D _center;
					pViewer->VoxelToDisplay( m_pViewerAttr->GetRenderingInfo()->GetRotationCenter(), _center );
					m_CopiedProjPoints.push_back( angle->m_vecProjectedPoint[i] - _center );
				}
				base = angle;
			}
		}
		break;
		case AME_MEASURE_TWO_LINE_ANGLE:
		{
			AmeMeasureTwoLineAngle* angle = new AmeMeasureTwoLineAngle( static_cast<AmeMeasureTwoLineAngle*>(current) );
			for ( int i = 0; i < (int)angle->m_vecPoint.size(); i++ )
			{
				angle->m_vecPoint[i].X = angle->m_vecPoint[i].X + offset.X;
				angle->m_vecPoint[i].Y = angle->m_vecPoint[i].Y + offset.Y;
				angle->m_vecPoint[i].Z = angle->m_vecPoint[i].Z + offset.Z;

				m_CopiedPoints.push_back( matrix * ((angle->m_vecPoint[i] - angle->m_vecPoint[0]) * scale) );
			}
			base = angle;
		}
		break;
		case AME_MEASURE_POINT:
		{
			AmeMeasurePoint* point = new AmeMeasurePoint( static_cast<AmeMeasurePoint*>(current) );
			point->m_Point.X = min( (float)imageSize.X - 1, point->m_Point.X + offset.X );
			point->m_Point.Y = min( (float)imageSize.Y - 1, point->m_Point.Y + offset.Y );
			point->m_Point.Z = min( (float)imageSize.Z - 1, point->m_Point.Z + offset.Z );
			base = point;
		}
		break;
		case AME_MEASURE_CUBE:
		{
			AmeMeasureCube* cube = new AmeMeasureCube( static_cast<AmeMeasureCube*>(current) );
			cube->m_BasePoint.X = min( (float)imageSize.X - 1, cube->m_BasePoint.X + offset.X );
			cube->m_BasePoint.Y = min( (float)imageSize.Y - 1, cube->m_BasePoint.Y + offset.Y );
			cube->m_BasePoint.Z = min( (float)imageSize.Z - 1, cube->m_BasePoint.Z + offset.Z );
			base = cube;

			AmeFloat2D size = cube->m_Size * src_attr->GetImageAttr()->GetMinVoxelSpacing();
			m_CopiedSize = AmeFloat3D( size.X, size.Y, 0 );
		}
		break;
		case AME_MEASURE_ELLIPSE:
		{
			AmeMeasureEllipse* elli = new AmeMeasureEllipse( static_cast<AmeMeasureEllipse*>(current) );
			elli->m_BasePoint.X = min( (float)imageSize.X - 1, elli->m_BasePoint.X + offset.X );
			elli->m_BasePoint.Y = min( (float)imageSize.Y - 1, elli->m_BasePoint.Y + offset.Y );
			elli->m_BasePoint.Z = min( (float)imageSize.Z - 1, elli->m_BasePoint.Z + offset.Z );
			base = elli;

			AmeFloat2D size = elli->m_Radius * src_attr->GetImageAttr()->GetMinVoxelSpacing();
			m_CopiedSize = AmeFloat3D( size.X, size.Y, 0 );
		}
		break;
		case AME_MEASURE_ROI:
		{
			AmeMeasureRoi* roi = new AmeMeasureRoi( static_cast<AmeMeasureRoi*>(current) );
			roi->m_BasePoint.X = min( (float)imageSize.X - 1, roi->m_BasePoint.X + offset.X );
			roi->m_BasePoint.Y = min( (float)imageSize.Y - 1, roi->m_BasePoint.Y + offset.Y );
			roi->m_BasePoint.Z = min( (float)imageSize.Z - 1, roi->m_BasePoint.Z + offset.Z );
			for ( int i = 0; i < (int)roi->m_vecPoints.size(); i++ )
			{
				roi->m_vecPoints[i].X = roi->m_vecPoints[i].X + offset.X;
				roi->m_vecPoints[i].Y = roi->m_vecPoints[i].Y + offset.Y;

				m_CopiedPoints.push_back( AmeFloat3D(
					roi->m_vecPoints[i].X * src_attr->GetImageAttr()->GetMinVoxelSpacing(),
					roi->m_vecPoints[i].Y * src_attr->GetImageAttr()->GetMinVoxelSpacing(),
					0.0f ) );
			}
			base = roi;
		}
		break;
		case AME_MEASURE_FREEHAND:
		{
			AmeMeasureFreehand* roi = new AmeMeasureFreehand( static_cast<AmeMeasureFreehand*>(current) );
			roi->m_BasePoint.X = min( (float)imageSize.X - 1, roi->m_BasePoint.X + offset.X );
			roi->m_BasePoint.Y = min( (float)imageSize.Y - 1, roi->m_BasePoint.Y + offset.Y );
			roi->m_BasePoint.Z = min( (float)imageSize.Z - 1, roi->m_BasePoint.Z + offset.Z );
			for ( int i = 0; i < (int)roi->m_vecPoints.size(); i++ )
			{
				roi->m_vecPoints[i].X = roi->m_vecPoints[i].X + offset.X;
				roi->m_vecPoints[i].Y = roi->m_vecPoints[i].Y + offset.Y;

				m_CopiedPoints.push_back( AmeFloat3D(
					roi->m_vecPoints[i].X * src_attr->GetImageAttr()->GetMinVoxelSpacing(),
					roi->m_vecPoints[i].Y * src_attr->GetImageAttr()->GetMinVoxelSpacing(),
					0.0f ) );
			}
			base = roi;
		}
		break;
		case AME_MEASURE_BOX:
		{
			AmeMeasureBox* box = new AmeMeasureBox( static_cast<AmeMeasureBox*>(current) );
			box->m_BasePoint.X = min( (float)imageSize.X - 1, box->m_BasePoint.X + offset.X );
			box->m_BasePoint.Y = min( (float)imageSize.Y - 1, box->m_BasePoint.Y + offset.Y );
			box->m_BasePoint.Z = min( (float)imageSize.Z - 1, box->m_BasePoint.Z + offset.Z );
			base = box;

			m_CopiedSize = box->m_Size * src_attr->GetImageAttr()->GetMinVoxelSpacing();
		}
		break;
		case AME_MEASURE_SPHERE:
		{
			AmeMeasureSphere* sphere = new AmeMeasureSphere( static_cast<AmeMeasureSphere*>(current) );
			sphere->m_BasePoint.X = min( (float)imageSize.X - 1, sphere->m_BasePoint.X + offset.X );
			sphere->m_BasePoint.Y = min( (float)imageSize.Y - 1, sphere->m_BasePoint.Y + offset.Y );
			sphere->m_BasePoint.Z = min( (float)imageSize.Z - 1, sphere->m_BasePoint.Z + offset.Z );
			base = sphere;

			m_CopiedSize.X = sphere->m_fRadius * src_attr->GetImageAttr()->GetMinVoxelSpacing();
		}
		break;
	}

	// 登録
	if ( base != NULL )
	{

		if ( m_pViewerAttr->GetViewer() != NULL )
		{
			std::vector<AmeMeasureDrawObject*> measures = m_pEngine->GetAllMeasure();
			base->AdjustResultBoxPosition( m_pViewerAttr->GetViewer(), base->GetBasePoint( m_pViewerAttr->GetViewer() ), (int)measures.size() );
		}

		m_iCopiedImageAttrID = base->GetImageAttr()->GetImageAttrID();
		m_pCopiedMeasure = base;
		m_bNowCopying = true;
		if ( m_frameMeasureList != NULL )
		{
			m_frameMeasureList->SetCopyButtonState( true );
		}

		GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	}
}

// 複製の確定
long AmeMeasurePlugInGUI::onCmdFinishCopy( PnwObject*, PnwEventType, PnwEventArgs* )
{
	AmeViewerAttr* attr = GetTaskManager()->GetCurrentViewerAttr();
	if ( attr == NULL || attr->GetViewer() == NULL )
		return 1;

	FinishCopy( attr->GetViewer() );
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	return 1;
}

// 複製のキャンセル
long AmeMeasurePlugInGUI::onCmdCancelCopy( PnwObject*, PnwEventType, PnwEventArgs* )
{
	CancelCopy();
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	return 1;
}

// 作成途中の計測を確定
long AmeMeasurePlugInGUI::onCmdFinishNow( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pViewerAttr == NULL )
	{
		return 1;
	}

	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current != NULL )
	{
		if ( current->GetType() == AME_MEASURE_FREEHAND )
		{
			FinishFreehandLiveWire( m_pViewerAttr, static_cast<AmeMeasureRoi*>(current) );
		}
	}

	FinishCreatingMeasure();

	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	return 1;
}

// 作成途中の計測の点をひとつ戻す
long AmeMeasurePlugInGUI::onCmdBackNow( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pViewerAttr == NULL )
	{
		return 1;
	}

	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL )
	{
		return 1;
	}

	switch ( current->GetType() )
	{
		case AME_MEASURE_POLYLINE:
		{
			AmeMeasurePolyLine* line = static_cast<AmeMeasurePolyLine*>(current);
			if ( line->m_vecPoint.size() > 2 )
			{
				line->m_vecPoint.pop_back();
			}
		}
		break;
		case AME_MEASURE_CURVE:
		{
			AmeMeasureCurve* curve = static_cast<AmeMeasureCurve*>(current);
			if ( curve->m_vecPoint.size() > 2 )
			{
				curve->m_vecPoint.pop_back();
			}
		}
		break;
		case AME_MEASURE_ANGLE:
		{
			AmeMeasureAngle* angle = static_cast<AmeMeasureAngle*>(current);
			if ( angle->m_vecPoint.size() > 2 )
			{
				angle->m_vecPoint.pop_back();
			}
		}
		break;
		case AME_MEASURE_PROJ_ANGLE:
		{
			AmeMeasureProjAngle* angle = static_cast<AmeMeasureProjAngle*>(current);
			if ( angle->m_vecProjectedPoint.size() > 2 )
			{
				angle->m_vecProjectedPoint.pop_back();
			}
		}
		break;
		case AME_MEASURE_TWO_LINE_ANGLE:
		{
			AmeMeasureTwoLineAngle* angle = static_cast<AmeMeasureTwoLineAngle*>(current);
			if ( angle->m_vecPoint.size() > 2 )
			{
				angle->m_vecPoint.pop_back();
			}
		}
		break;
		case AME_MEASURE_ROI:
		{
			AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(current);
			if ( roi->m_vecPoints.size() > 2 )
			{
				roi->m_vecPoints.pop_back();
			}
		}
		break;
		case AME_MEASURE_FREEHAND:
		{
			AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(current);
			if ( m_LiveWire.Counter.size() > 1 )
			{
				int num = m_LiveWire.Counter[m_LiveWire.Counter.size() - 1];
				if ( (int)roi->m_vecPoints.size() - num >= 1 )
				{
					AmeViewerAttr* attr = GetTaskManagerEngine()->GetViewerAttr( m_LiveWire.ViewerAttrID );

					// 現在のLiveWireを停止
					ResetLiveWire();

					// 後ろの点を削除
					roi->m_vecPoints.resize( roi->m_vecPoints.size() - num );
					m_LiveWire.Counter.pop_back();
					m_LiveWire.SeedList.pop_back();

					// LiveWireを再開
					if ( attr != NULL )
					{
						BeginLiveWire( attr, m_LiveWire.SeedList[m_LiveWire.SeedList.size() - 1] );
					}
				}
			}
		}
		break;
	}

	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	return 1;
}


long AmeMeasurePlugInGUI::onCmdCopyToAll( PnwObject*, PnwEventType, PnwEventArgs* )
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL ) return 1;

	AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Copying the measurement to all.");

	AmeMeasureRegionBase* region = dynamic_cast<AmeMeasureRegionBase*>(current);
	if ( region != NULL )
	{
		CopyToAllRegion();
	}
	AmeMeasurePointArrayBase* array = dynamic_cast<AmeMeasurePointArrayBase*>(current);
	if ( array != NULL )
	{
		CopyToAllPointArray();
	}
	AmeMeasurePoint* point = dynamic_cast<AmeMeasurePoint*>(current);
	if ( point != NULL )
	{
		CopyToAllPoint();
	}
	AmeMeasureVolumeBase* volume = dynamic_cast<AmeMeasureVolumeBase*>(current);
	if ( volume != NULL )
	{
		CopyToAllVolume();
	}

	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

	return 1;
}

int AmeMeasurePlugInGUI::CopyToAllPointArray()
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL ) return 1;

	if ( !(
		current->GetType() == AME_MEASURE_LINE
		|| current->GetType() == AME_MEASURE_POLYLINE
		|| current->GetType() == AME_MEASURE_TTTG
		|| current->GetType() == AME_MEASURE_CURVE
		|| current->GetType() == AME_MEASURE_ANGLE
		|| current->GetType() == AME_MEASURE_TWO_LINE_ANGLE
		|| current->GetType() == AME_MEASURE_POINT) )
	{
		return 1;
	}

	// 全画像へコピーは、2Dプラグインのみ使用可能とする。
	bool bSameViewerTypeOnly = false;
	{
		AmeBaseAnalysisPlugInDesktopGUI* src_plugin = dynamic_cast<AmeBaseAnalysisPlugInDesktopGUI*>(GetTaskManager()->GetTaskCardPlugInGUI().m_pPlugIn);
		bSameViewerTypeOnly = (src_plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) == L"SAME_VIEWER_TYPE");
	}

	AmeViewerAttr* base_attr = GetTaskManagerEngine()->GetCurrentViewerAttr();
	if ( base_attr == NULL )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NOT_FOUND_MEASURED_IMAGE, AppAttr->GetResStr( "AME_MEASURE_NOT_FOUND_MEASURED_IMAGE" ), L"Not found measured image." );
		return 1;
	}
	if ( base_attr->GetViewer() == NULL )
	{
		return 1;
	}

	// 複製対象のビューアを列挙
	std::vector<AmeViewerAttr*> targets;
	std::vector<AmeViewerAttr*> attrlist = GetTaskManager()->GetAllVisibleViewerAttrs();
	for ( size_t n = 0; n < attrlist.size(); n++ )
	{
		if ( attrlist[n] != NULL && attrlist[n] != base_attr && attrlist[n]->GetViewer() != NULL )
		{
			// カラー画像は不可
			if ( attrlist[n]->GetImageAttr()->IsColorImage() )
			{
				continue;
			}
			// 計測を配置可能かどうか
			bool bValidScale;
			if ( !IsMeasureCreatable( current->GetType(), attrlist[n]->GetViewer(), bValidScale ) )
			{
				continue;
			}
			if ( bSameViewerTypeOnly )
			{
				// 同じ種類のビューアのみ
				if ( base_attr->GetViewer()->GetViewerType() != attrlist[n]->GetViewer()->GetViewerType() )
				{
					continue;
				}
				// オブリークなら同じ向きかどうかも
				if ( base_attr->GetViewer()->GetViewerType() == AME_VIEWER_OBLIQUE )
				{
					if ( base_attr->GetViewer()->GetViewerAxisZ().InnerProduct( attrlist[n]->GetViewer()->GetViewerAxisZ() ) < 0.99f )
						continue;
				}
			}
			// 対象とする
			targets.push_back( attrlist[n] );
		}
	}

	if ( targets.empty() )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NO_COPY_TARGET, AppAttr->GetResStr( "AME_MEASURE_NO_ORIGINAL_SLICE" ), L"No target viewer for copying." );
		return 1;
	}

	// 各ビューアに複製
	for ( size_t n = 0; n < targets.size(); n++ )
	{
		AmeSize3D base_size = base_attr->GetImageAttr()->GetImageSize();
		AmeFloat3D base_scale = base_attr->GetImageAttr()->GetVoxelSpacing();
		AmeFloat3D base_scale_size = base_size.ToFloat() * base_scale;
		AME_VIEWER_TYPE type = AME_VIEWER_XY;
		if ( m_pViewerAttr->m_iOriginalSliceSize > 0 )
		{
		}
		else if ( base_attr->GetViewer() != NULL )
		{
			type = base_attr->GetViewer()->GetViewerType();
		}

		AmeViewerAttr* target_attr = targets[n];
		AmeSize3D imageSize = target_attr->GetImageAttr()->GetImageSize();
		AmeFloat3D scale = target_attr->GetImageAttr()->GetVoxelSpacing();
		AmeFloat3D scale_size = scale * imageSize.ToFloat();
		AmeFloat3D offset( 0, 0, 0 );

		unique_ptr<AmeMeasureDrawObject> base;

		std::vector<AmeFloat3D> new_points;
		AmeMeasurePointArrayBase* array = dynamic_cast<AmeMeasurePointArrayBase*>(current);
		if ( array != NULL )
		{
			AmeFloat3D base_point( 0, 0, 0 );
			std::vector<AmeFloat3D> points = array->m_vecPoint;
			for ( const auto& p : points )
			{
				AmeFloat3D pos = (p - base_size.ToFloat() * 0.5f) * base_scale;
				switch ( type )
				{
					case AME_VIEWER_XY:
					default:
						base_point.X = pos.X / max( base_scale_size.X, base_scale_size.Y );
						base_point.Y = pos.Y / max( base_scale_size.X, base_scale_size.Y );
						base_point.Z = pos.Z / max( base_scale_size.X, base_scale_size.Y );
						break;
					case AME_VIEWER_XZ:
						base_point.X = pos.X / max( base_scale_size.X, base_scale_size.Z );
						base_point.Y = pos.Z / max( base_scale_size.X, base_scale_size.Z );
						base_point.Z = pos.Y / max( base_scale_size.X, base_scale_size.Z );
						break;
					case AME_VIEWER_YZ:
						base_point.X = pos.Y / max( base_scale_size.Y, base_scale_size.Z );
						base_point.Y = pos.Z / max( base_scale_size.Y, base_scale_size.Z );
						base_point.Z = pos.X / max( base_scale_size.Y, base_scale_size.Z );
						break;
				}

				AME_VIEWER_TYPE target_type = AME_VIEWER_XY;
				AmeFloat3D cur_pos( 0, 0, 0 );
				if ( target_attr->m_iOriginalSliceSize == 0 && target_attr->GetViewer() != NULL )
				{
					target_type = target_attr->GetViewer()->GetViewerType();
					cur_pos = target_attr->m_CurrentPosition.ToFloat();
				}

				pos = cur_pos;
				switch ( target_type )
				{
					case AME_VIEWER_XY:
					default:
						pos.X = base_point.X * imageSize.X * max( scale_size.X, scale_size.Y ) / scale_size.X + imageSize.X * 0.5f;
						pos.Y = base_point.Y * imageSize.Y * max( scale_size.X, scale_size.Y ) / scale_size.Y + imageSize.Y * 0.5f;
						pos.Z = base_point.Z * imageSize.Z * max( scale_size.X, scale_size.Y ) / scale_size.Z + imageSize.Z * 0.5f;
						break;
					case AME_VIEWER_XZ:
						pos.X = base_point.X * imageSize.X * max( scale_size.X, scale_size.Z ) / scale_size.X + imageSize.X * 0.5f;
						pos.Z = base_point.Y * imageSize.Z * max( scale_size.X, scale_size.Z ) / scale_size.Z + imageSize.Z * 0.5f;
						pos.Y = base_point.Z * imageSize.Y * max( scale_size.X, scale_size.Z ) / scale_size.Y + imageSize.Y * 0.5f;
						break;
					case AME_VIEWER_YZ:
						pos.Y = base_point.X * imageSize.Y * max( scale_size.Y, scale_size.Z ) / scale_size.Y + imageSize.Y * 0.5f;
						pos.Z = base_point.Y * imageSize.Z * max( scale_size.Y, scale_size.Z ) / scale_size.Z + imageSize.Z * 0.5f;
						pos.X = base_point.Z * imageSize.X * max( scale_size.Y, scale_size.Z ) / scale_size.X + imageSize.X * 0.5f;
						break;
				}

				new_points.push_back( pos );
			}
		}

		switch ( current->GetType() )
		{
			case AME_MEASURE_LINE:
			{
				AmeMeasureLine* line = new AmeMeasureLine( static_cast<AmeMeasureLine*>(current) );
				line->m_vecPoint = new_points;
				base.reset( line );
			}
			break;
			case AME_MEASURE_POLYLINE:
			{
				AmeMeasurePolyLine* polyline = new AmeMeasurePolyLine( static_cast<AmeMeasurePolyLine*>(current) );
				polyline->m_vecPoint = new_points;
				base.reset( polyline );
			}
			break;
			case AME_MEASURE_TTTG:
			{
				AmeMeasureTTTG* tttg = new AmeMeasureTTTG( static_cast<AmeMeasureTTTG*>(current) );
				tttg->m_vecPoint = new_points;
				base.reset( tttg );
			}
			break;
			case AME_MEASURE_CURVE:
			{
				AmeMeasureCurve* curve = new AmeMeasureCurve( static_cast<AmeMeasureCurve*>(current) );
				curve->m_vecPoint = new_points;
				base.reset( curve );
			}
			break;
			case AME_MEASURE_ANGLE:
			{
				AmeMeasureAngle* angle = new AmeMeasureAngle( static_cast<AmeMeasureAngle*>(current) );
				angle->m_vecPoint = new_points;
				base.reset( angle );
			}
			break;
			case AME_MEASURE_TWO_LINE_ANGLE:
			{
				AmeMeasureTwoLineAngle* twoangle = new AmeMeasureTwoLineAngle( static_cast<AmeMeasureTwoLineAngle*>(current) );
				twoangle->m_vecPoint = new_points;
				base.reset( twoangle );
			}
			break;
		}

		// 登録
		AmeImageViewer* pTargetViewer = target_attr->GetViewer();
		if ( base != nullptr && pTargetViewer != nullptr )
		{
			base->SetImageAttr( target_attr->GetImageAttr() );
			base->m_iOrgViewerType = pTargetViewer->GetViewerType();
			base->m_FrameSerialID = pTargetViewer->GetViewerFrame()->GetSerialID();
			base->m_ViewerIdentity = target_attr->GetViewerAttrIDString();
			base->m_ImageAttrIDString = target_attr->GetImageAttr()->GetImageAttrIDString();

			AmeViewerAttr* pOverlayAttr = pTargetViewer->GetOverlayViewerAttr();
			if ( pOverlayAttr != NULL &&
				(pOverlayAttr->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
				pOverlayAttr->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
			{
				base->m_OverlayImageIdentity = pOverlayAttr->GetImageAttr()->GetImageAttrIDString();
			}
			else
			{
				base->m_OverlayImageIdentity = L"";
			}

			int index = 0;
			if ( m_pEngine->m_AllObjects.size() != 0 )
			{
				index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
			}
			base->SetIndex( index );

			AmeMeasureDrawObject* object = base.release();
			m_pEngine->m_AllObjects.push_back( object );
			GetTaskManager()->AddDrawObject( object );
		}
	}

	return 1;
}
int AmeMeasurePlugInGUI::CopyToAllPoint()
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL ) return 1;

	if ( !(current->GetType() == AME_MEASURE_POINT) )
	{
		return 1;
	}

	// 全画像へコピーは、2Dプラグインのみ使用可能とする。
	bool bSameViewerTypeOnly = false;
	{
		AmeBaseAnalysisPlugInDesktopGUI* src_plugin = dynamic_cast<AmeBaseAnalysisPlugInDesktopGUI*>(GetTaskManager()->GetTaskCardPlugInGUI().m_pPlugIn);
		bSameViewerTypeOnly = (src_plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) == L"SAME_VIEWER_TYPE");
	}

	AmeViewerAttr* base_attr = GetTaskManagerEngine()->GetCurrentViewerAttr();
	if ( base_attr == NULL )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NOT_FOUND_MEASURED_IMAGE, AppAttr->GetResStr( "AME_MEASURE_NOT_FOUND_MEASURED_IMAGE" ), L"Not found measured image." );
		return 1;
	}
	if ( base_attr->GetViewer() == NULL )
	{
		return 1;
	}

	// 複製対象のビューアを列挙
	std::vector<AmeViewerAttr*> attrlist = GetTaskManager()->GetAllVisibleViewerAttrs();
	std::vector<AmeViewerAttr*> targets;
	for ( size_t n = 0; n < attrlist.size(); n++ )
	{
		if ( attrlist[n] != NULL && attrlist[n] != base_attr && attrlist[n]->GetViewer() != NULL )
		{
			// カラー画像は不可
			if ( attrlist[n]->GetImageAttr()->IsColorImage() )
			{
				continue;
			}
			// 計測を配置可能かどうか
			bool bValidScale;
			if ( !IsMeasureCreatable( current->GetType(), attrlist[n]->GetViewer(), bValidScale ) )
			{
				continue;
			}
			if ( bSameViewerTypeOnly )
			{
				// 同じ種類のビューアのみ
				if ( base_attr->GetViewer()->GetViewerType() != attrlist[n]->GetViewer()->GetViewerType() )
				{
					continue;
				}
				// オブリークなら同じ向きかどうかも
				if ( base_attr->GetViewer()->GetViewerType() == AME_VIEWER_OBLIQUE )
				{
					if ( base_attr->GetViewer()->GetViewerAxisZ().InnerProduct( attrlist[n]->GetViewer()->GetViewerAxisZ() ) < 0.99f )
						continue;
				}
			}
			// 対象とする
			targets.push_back( attrlist[n] );
		}
	}

	if ( targets.empty() )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NO_COPY_TARGET, AppAttr->GetResStr( "AME_MEASURE_NO_ORIGINAL_SLICE" ), L"No target viewer for copying." );
		return 1;
	}

	// 各ビューアに複製
	for ( size_t n = 0; n < targets.size(); n++ )
	{
		AmeSize3D base_size = base_attr->GetImageAttr()->GetImageSize();
		AmeFloat3D base_scale = base_attr->GetImageAttr()->GetVoxelSpacing();
		AmeFloat3D base_scale_size = base_size.ToFloat() * base_scale;
		AME_VIEWER_TYPE type = AME_VIEWER_XY;
		if ( m_pViewerAttr->m_iOriginalSliceSize > 0 )
		{
		}
		else if ( base_attr->GetViewer() != NULL )
		{
			type = base_attr->GetViewer()->GetViewerType();
		}

		AmeViewerAttr* target_attr = targets[n];
		AmeSize3D imageSize = target_attr->GetImageAttr()->GetImageSize();
		AmeFloat3D scale = target_attr->GetImageAttr()->GetVoxelSpacing();
		AmeFloat3D scale_size = scale * imageSize.ToFloat();
		AmeFloat3D offset( 0, 0, 0 );

		AmeFloat3D pos;
		AmeMeasurePoint* point = dynamic_cast<AmeMeasurePoint*>(current);
		if ( point != NULL )
		{
			AmeFloat2D base_point( 0, 0 );
			AmeFloat3D p = point->m_Point;
			pos = (p - base_size.ToFloat() * 0.5f) * base_scale;
			switch ( type )
			{
				case AME_VIEWER_XY:
				default:
					base_point.X = pos.X / max( base_scale_size.X, base_scale_size.Y );
					base_point.Y = pos.Y / max( base_scale_size.X, base_scale_size.Y );
					break;
				case AME_VIEWER_XZ:
					base_point.X = pos.X / max( base_scale_size.X, base_scale_size.Z );
					base_point.Y = pos.Z / max( base_scale_size.X, base_scale_size.Z );
					break;
				case AME_VIEWER_YZ:
					base_point.X = pos.Y / max( base_scale_size.Y, base_scale_size.Z );
					base_point.Y = pos.Z / max( base_scale_size.Y, base_scale_size.Z );
					break;
			}

			AME_VIEWER_TYPE target_type = AME_VIEWER_XY;
			AmeFloat3D cur_pos( 0, 0, 0 );
			if ( target_attr->m_iOriginalSliceSize == 0 && target_attr->GetViewer() != NULL )
			{
				target_type = target_attr->GetViewer()->GetViewerType();
				cur_pos = target_attr->m_CurrentPosition.ToFloat();
			}

			pos = cur_pos;
			switch ( target_type )
			{
				case AME_VIEWER_XY:
				default:
					pos.X = base_point.X * imageSize.X * max( scale_size.X, scale_size.Y ) / scale_size.X + imageSize.X * 0.5f;
					pos.Y = base_point.Y * imageSize.Y * max( scale_size.X, scale_size.Y ) / scale_size.Y + imageSize.Y * 0.5f;
					break;
				case AME_VIEWER_XZ:
					pos.X = base_point.X * imageSize.X * max( scale_size.X, scale_size.Z ) / scale_size.X + imageSize.X * 0.5f;
					pos.Z = base_point.Y * imageSize.Z * max( scale_size.X, scale_size.Z ) / scale_size.Z + imageSize.Z * 0.5f;
					break;
				case AME_VIEWER_YZ:
					pos.Y = base_point.X * imageSize.Y * max( scale_size.Y, scale_size.Z ) / scale_size.Y + imageSize.Y * 0.5f;
					pos.Z = base_point.Y * imageSize.Z * max( scale_size.Y, scale_size.Z ) / scale_size.Z + imageSize.Z * 0.5f;
					break;
			}
		}

		unique_ptr<AmeMeasurePoint> new_point = make_unique<AmeMeasurePoint>( static_cast<AmeMeasurePoint*>(current) );
		if ( new_point == nullptr )
		{
			assert( false );
			continue;
		}
		new_point->m_Point = pos;
		// 登録
		AmeImageViewer* pTargetViewer = target_attr->GetViewer();
		if ( pTargetViewer != nullptr )
		{
			new_point->SetImageAttr( target_attr->GetImageAttr() );
			new_point->m_iOrgViewerType = pTargetViewer->GetViewerType();
			new_point->m_FrameSerialID = pTargetViewer->GetViewerFrame()->GetSerialID();
			new_point->m_ViewerIdentity = target_attr->GetViewerAttrIDString();
			new_point->m_ImageAttrIDString = target_attr->GetImageAttr()->GetImageAttrIDString();

			AmeViewerAttr* pOverlayAttr = pTargetViewer->GetOverlayViewerAttr();
			if ( pOverlayAttr != NULL &&
				(pOverlayAttr->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
				pOverlayAttr->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
			{
				new_point->m_OverlayImageIdentity = pOverlayAttr->GetImageAttr()->GetImageAttrIDString();
			}
			else
			{
				new_point->m_OverlayImageIdentity = L"";
			}

			int index = 0;
			if ( m_pEngine->m_AllObjects.size() != 0 )
			{
				index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
			}
			new_point->SetIndex( index );

			AmeMeasurePoint* base = new_point.release();
			m_pEngine->m_AllObjects.push_back( base );
			GetTaskManager()->AddDrawObject( base );
		}
	}
	return 1;
}
int AmeMeasurePlugInGUI::CopyToAllRegion()
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL ) return 1;

	// 複製可能なのは面積系のみ
	if ( !(
		current->GetType() == AME_MEASURE_CUBE
		|| current->GetType() == AME_MEASURE_ELLIPSE
		|| current->GetType() == AME_MEASURE_ROI
		|| current->GetType() == AME_MEASURE_FREEHAND) )
	{
		return 1;
	}

	// 全画像へコピーは、2Dプラグインのみ使用可能とする。
	bool bSameViewerTypeOnly = false;
	{
		AmeBaseAnalysisPlugInDesktopGUI* src_plugin = dynamic_cast<AmeBaseAnalysisPlugInDesktopGUI*>(GetTaskManager()->GetTaskCardPlugInGUI().m_pPlugIn);
		bSameViewerTypeOnly = (src_plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) == L"SAME_VIEWER_TYPE");
	}


	AmeViewerAttr* base_attr = GetTaskManagerEngine()->GetCurrentViewerAttr();
	if ( base_attr == NULL )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NOT_FOUND_MEASURED_IMAGE, AppAttr->GetResStr( "AME_MEASURE_NOT_FOUND_MEASURED_IMAGE" ), L"Not found measured image." );
		return 1;
	}
	if ( base_attr->GetViewer() == NULL )
	{
		return 1;
	}

	// 複製対象のビューアを列挙
	std::vector<AmeViewerAttr*> attrlist = GetTaskManager()->GetAllVisibleViewerAttrs();
	std::vector<AmeViewerAttr*> targets;
	for ( size_t n = 0; n < attrlist.size(); n++ )
	{
		if ( attrlist[n] != NULL && attrlist[n] != base_attr && attrlist[n]->GetViewer() != NULL )
		{
			// カラー画像は不可
			if ( attrlist[n]->GetImageAttr()->IsColorImage() )
			{
				continue;
			}
			// 計測を配置可能かどうか
			bool bValidScale;
			if ( !IsMeasureCreatable( current->GetType(), attrlist[n]->GetViewer(), bValidScale ) )
			{
				continue;
			}
			if ( bSameViewerTypeOnly )
			{
				// 同じ種類のビューアのみ
				if ( base_attr->GetViewer()->GetViewerType() != attrlist[n]->GetViewer()->GetViewerType() )
				{
					continue;
				}
				// オブリークなら同じ向きかどうかも
				if ( base_attr->GetViewer()->GetViewerType() == AME_VIEWER_OBLIQUE )
				{
					if ( base_attr->GetViewer()->GetViewerAxisZ().InnerProduct( attrlist[n]->GetViewer()->GetViewerAxisZ() ) < 0.99f )
						continue;
				}
			}
			// 対象とする
			targets.push_back( attrlist[n] );
		}
	}

	if ( targets.empty() )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NO_COPY_TARGET, AppAttr->GetResStr( "AME_MEASURE_NO_ORIGINAL_SLICE" ), L"No target viewer for copying." );
		return 1;
	}

	AmeFloat2D base_point( 0, 0 );
	{
		AmeSize3D size = base_attr->GetImageAttr()->GetImageSize();
		AmeFloat3D scale = base_attr->GetImageAttr()->GetVoxelSpacing();
		AmeFloat3D scale_size = size.ToFloat() * scale;
		AME_VIEWER_TYPE type = AME_VIEWER_XY;
		if ( m_pViewerAttr->m_iOriginalSliceSize > 0 )
		{
		}
		else if ( base_attr->GetViewer() != NULL )
		{
			type = base_attr->GetViewer()->GetViewerType();
		}

		AmeMeasureRegionBase* region = dynamic_cast<AmeMeasureRegionBase*>(current);
		if ( region != NULL )
		{
			AmeFloat3D pos = (region->m_BasePoint - size.ToFloat() * 0.5f) * scale;
			switch ( type )
			{
				case AME_VIEWER_XY:
				default:
					base_point.X = pos.X / max( scale_size.X, scale_size.Y );
					base_point.Y = pos.Y / max( scale_size.X, scale_size.Y );
					break;
				case AME_VIEWER_XZ:
					base_point.X = pos.X / max( scale_size.X, scale_size.Z );
					base_point.Y = pos.Z / max( scale_size.X, scale_size.Z );
					break;
				case AME_VIEWER_YZ:
					base_point.X = pos.Y / max( scale_size.Y, scale_size.Z );
					base_point.Y = pos.Z / max( scale_size.Y, scale_size.Z );
					break;
				case AME_VIEWER_OBLIQUE:
					AmeFloat3D axisX = base_attr->GetViewer()->GetViewerAxisX();
					AmeFloat3D axisY = base_attr->GetViewer()->GetViewerAxisY();
					base_point = AmeFloat2D( axisX.InnerProduct( pos ), axisY.InnerProduct( pos ) );
					break;
			}
		}
	}

	// 各ビューアに複製
	for ( size_t n = 0; n < targets.size(); n++ )
	{
		AmeViewerAttr* target_attr = targets[n];
		AmeSize3D imageSize = target_attr->GetImageAttr()->GetImageSize();
		AmeFloat3D scale = target_attr->GetImageAttr()->GetVoxelSpacing();
		AmeFloat3D scale_size = scale * imageSize.ToFloat();
		float scale_rate = base_attr->GetImageAttr()->GetMinVoxelSpacing() / target_attr->GetImageAttr()->GetMinVoxelSpacing();
		AmeFloat3D offset( 0, 0, 0 );

		AmeMeasureDrawObject* base = NULL;

		AME_VIEWER_TYPE target_type = AME_VIEWER_XY;
		AmeFloat3D cur_pos( 0, 0, 0 );
		if ( target_attr->m_iOriginalSliceSize == 0 && target_attr->GetViewer() != NULL )
		{
			target_type = target_attr->GetViewer()->GetViewerType();
			cur_pos = target_attr->m_CurrentPosition.ToFloat();
		}

		AmeFloat3D pos = cur_pos;
		switch ( target_type )
		{
			case AME_VIEWER_XY:
			default:
				pos.X = base_point.X * imageSize.X * max( scale_size.X, scale_size.Y ) / scale_size.X + imageSize.X * 0.5f;
				pos.Y = base_point.Y * imageSize.Y * max( scale_size.X, scale_size.Y ) / scale_size.Y + imageSize.Y * 0.5f;
				break;
			case AME_VIEWER_XZ:
				pos.X = base_point.X * imageSize.X * max( scale_size.X, scale_size.Z ) / scale_size.X + imageSize.X * 0.5f;
				pos.Z = base_point.Y * imageSize.Z * max( scale_size.X, scale_size.Z ) / scale_size.Z + imageSize.Z * 0.5f;
				break;
			case AME_VIEWER_YZ:
				pos.Y = base_point.X * imageSize.Y * max( scale_size.Y, scale_size.Z ) / scale_size.Y + imageSize.Y * 0.5f;
				pos.Z = base_point.Y * imageSize.Z * max( scale_size.Y, scale_size.Z ) / scale_size.Z + imageSize.Z * 0.5f;
				break;
			case AME_VIEWER_OBLIQUE:
				AmeFloat3D axisX = target_attr->GetViewer()->GetViewerAxisX();
				AmeFloat3D axisY = target_attr->GetViewer()->GetViewerAxisY();
				pos = base_point.X * axisX + base_point.Y * axisY;
				pos /= scale;
				pos += imageSize.ToFloat() * 0.5f;
				break;
		}

		switch ( current->GetType() )
		{
			case AME_MEASURE_CUBE:
			{
				AmeMeasureCube* cube = new AmeMeasureCube( static_cast<AmeMeasureCube*>(current) );
				cube->m_BasePoint = pos;
				cube->m_Size = cube->m_Size * scale_rate;
				base = cube;
			}
			break;
			case AME_MEASURE_ELLIPSE:
			{
				AmeMeasureEllipse* elli = new AmeMeasureEllipse( static_cast<AmeMeasureEllipse*>(current) );
				elli->m_BasePoint = pos;
				elli->m_Radius = elli->m_Radius * scale_rate;
				base = elli;
			}
			break;
			case AME_MEASURE_ROI:
			{
				AmeMeasureRoi* roi = new AmeMeasureRoi( static_cast<AmeMeasureRoi*>(current) );
				roi->m_BasePoint = pos;
				for ( int i = 0; i < (int)roi->m_vecPoints.size(); i++ )
				{
					roi->m_vecPoints[i] *= scale_rate;
				}
				base = roi;
			}
			break;
			case AME_MEASURE_FREEHAND:
			{
				AmeMeasureFreehand* roi = new AmeMeasureFreehand( static_cast<AmeMeasureFreehand*>(current) );
				roi->m_BasePoint = pos;
				for ( int i = 0; i < (int)roi->m_vecPoints.size(); i++ )
				{
					roi->m_vecPoints[i] *= scale_rate;
				}
				base = roi;
			}
			break;
		}
		// 登録
		AmeImageViewer* pTargetViewer = target_attr->GetViewer();
		if ( base != nullptr && pTargetViewer != nullptr )
		{
			base->SetImageAttr( target_attr->GetImageAttr() );
			base->m_iOrgViewerType = pTargetViewer->GetViewerType();
			base->m_FrameSerialID = pTargetViewer->GetViewerFrame()->GetSerialID();
			base->m_ViewerIdentity = target_attr->GetViewerAttrIDString();
			base->m_ImageAttrIDString = target_attr->GetImageAttr()->GetImageAttrIDString();

			AmeViewerAttr* pOverlayAttr = pTargetViewer->GetOverlayViewerAttr();
			if ( pOverlayAttr != NULL &&
				(pOverlayAttr->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
				pOverlayAttr->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
			{
				base->m_OverlayImageIdentity = pOverlayAttr->GetImageAttr()->GetImageAttrIDString();
			}
			else
			{
				base->m_OverlayImageIdentity = L"";
			}

			int index = 0;
			if ( m_pEngine->m_AllObjects.size() != 0 )
			{
				index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
			}
			base->SetIndex( index );

			m_pEngine->m_AllObjects.push_back( base );
			GetTaskManager()->AddDrawObject( base );
		}
	}

	return 1;
}

int AmeMeasurePlugInGUI::CopyToAllVolume()
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL ) return 1;

	if ( !(
		current->GetType() == AME_MEASURE_BOX
		|| current->GetType() == AME_MEASURE_SPHERE) )
	{
		return 1;
	}

	// 全画像へコピーは、2Dプラグインのみ使用可能とする。
	bool bSameViewerTypeOnly = false;
	{
		AmeBaseAnalysisPlugInDesktopGUI* src_plugin = dynamic_cast<AmeBaseAnalysisPlugInDesktopGUI*>(GetTaskManager()->GetTaskCardPlugInGUI().m_pPlugIn);
		bSameViewerTypeOnly = (src_plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) == L"SAME_VIEWER_TYPE");
	}


	AmeViewerAttr* base_attr = GetTaskManagerEngine()->GetViewerAttr( current->m_iMeasuredViewerAttrID[0] );
	if ( base_attr == NULL )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NOT_FOUND_MEASURED_IMAGE, AppAttr->GetResStr( "AME_MEASURE_NOT_FOUND_MEASURED_IMAGE" ), L"Not found measured image." );
		return 1;
	}
	if ( base_attr->GetViewer() == NULL )
	{
		return 1;
	}

	// 複製対象のビューアを列挙
	std::vector<AmeViewerAttr*> attrlist = GetTaskManager()->GetAllVisibleViewerAttrs();
	std::vector<AmeViewerAttr*> targets;
	for ( size_t n = 0; n < attrlist.size(); n++ )
	{
		if ( attrlist[n] != NULL && attrlist[n] != base_attr && attrlist[n]->GetViewer() != NULL )
		{
			// カラー画像は不可
			if ( attrlist[n]->GetImageAttr()->IsColorImage() )
			{
				continue;
			}
			// 計測を配置可能かどうか
			bool bValidScale;
			if ( !IsMeasureCreatable( current->GetType(), attrlist[n]->GetViewer(), bValidScale ) )
			{
				continue;
			}
			if ( bSameViewerTypeOnly )
			{
				// 同じ種類のビューアのみ
				if ( base_attr->GetViewer()->GetViewerType() != attrlist[n]->GetViewer()->GetViewerType() )
				{
					continue;
				}
				// オブリークなら同じ向きかどうかも
				if ( base_attr->GetViewer()->GetViewerType() == AME_VIEWER_OBLIQUE )
				{
					if ( base_attr->GetViewer()->GetViewerAxisZ().InnerProduct( attrlist[n]->GetViewer()->GetViewerAxisZ() ) < 0.99f )
						continue;
				}
			}
			// 対象とする
			targets.push_back( attrlist[n] );
		}
	}

	if ( targets.empty() )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NO_COPY_TARGET, AppAttr->GetResStr( "AME_MEASURE_NO_ORIGINAL_SLICE" ), L"No target viewer for copying." );
		return 1;
	}

	AmeFloat2D base_point( 0, 0 );
	{
		AmeSize3D size = base_attr->GetImageAttr()->GetImageSize();
		AmeFloat3D scale = base_attr->GetImageAttr()->GetVoxelSpacing();
		AmeFloat3D scale_size = size.ToFloat() * scale;
		AME_VIEWER_TYPE type = AME_VIEWER_XY;
		if ( m_pViewerAttr->m_iOriginalSliceSize > 0 )
		{
		}
		else if ( base_attr->GetViewer() != NULL )
		{
			type = base_attr->GetViewer()->GetViewerType();
		}

		AmeMeasureVolumeBase* volume = dynamic_cast<AmeMeasureVolumeBase*>(current);
		if ( volume != NULL )
		{
			AmeFloat3D pos = (volume->m_BasePoint - size.ToFloat() * 0.5f) * scale;
			switch ( type )
			{
				case AME_VIEWER_XY:
				default:
					base_point.X = pos.X / max( scale_size.X, scale_size.Y );
					base_point.Y = pos.Y / max( scale_size.X, scale_size.Y );
					break;
				case AME_VIEWER_XZ:
					base_point.X = pos.X / max( scale_size.X, scale_size.Z );
					base_point.Y = pos.Z / max( scale_size.X, scale_size.Z );
					break;
				case AME_VIEWER_YZ:
					base_point.X = pos.Y / max( scale_size.Y, scale_size.Z );
					base_point.Y = pos.Z / max( scale_size.Y, scale_size.Z );
					break;
			}
		}
	}

	// 各ビューアに複製
	for ( size_t n = 0; n < targets.size(); n++ )
	{
		AmeViewerAttr* target_attr = targets[n];
		AmeSize3D imageSize = target_attr->GetImageAttr()->GetImageSize();
		AmeFloat3D scale = target_attr->GetImageAttr()->GetVoxelSpacing();
		AmeFloat3D scale_size = scale * imageSize.ToFloat();
		float scale_rate = base_attr->GetImageAttr()->GetMinVoxelSpacing() / target_attr->GetImageAttr()->GetMinVoxelSpacing();
		AmeFloat3D offset( 0, 0, 0 );

		AmeMeasureDrawObject* base = NULL;

		AME_VIEWER_TYPE target_type = AME_VIEWER_XY;
		AmeFloat3D cur_pos( 0, 0, 0 );
		if ( target_attr->m_iOriginalSliceSize == 0 && target_attr->GetViewer() != NULL )
		{
			target_type = target_attr->GetViewer()->GetViewerType();
			cur_pos = target_attr->m_CurrentPosition.ToFloat();
		}

		AmeFloat3D pos = cur_pos;
		switch ( target_type )
		{
			case AME_VIEWER_XY:
			default:
				pos.X = base_point.X * imageSize.X * max( scale_size.X, scale_size.Y ) / scale_size.X + imageSize.X * 0.5f;
				pos.Y = base_point.Y * imageSize.Y * max( scale_size.X, scale_size.Y ) / scale_size.Y + imageSize.Y * 0.5f;
				break;
			case AME_VIEWER_XZ:
				pos.X = base_point.X * imageSize.X * max( scale_size.X, scale_size.Z ) / scale_size.X + imageSize.X * 0.5f;
				pos.Z = base_point.Y * imageSize.Z * max( scale_size.X, scale_size.Z ) / scale_size.Z + imageSize.Z * 0.5f;
				break;
			case AME_VIEWER_YZ:
				pos.Y = base_point.X * imageSize.Y * max( scale_size.Y, scale_size.Z ) / scale_size.Y + imageSize.Y * 0.5f;
				pos.Z = base_point.Y * imageSize.Z * max( scale_size.Y, scale_size.Z ) / scale_size.Z + imageSize.Z * 0.5f;
				break;
		}

		switch ( current->GetType() )
		{
			case AME_MEASURE_BOX:
			{
				AmeMeasureBox* box = new AmeMeasureBox( static_cast<AmeMeasureBox*>(current) );
				box->m_BasePoint = pos;
				box->m_Size = box->m_Size * scale_rate;
				base = box;
			}
			break;
			case AME_MEASURE_SPHERE:
			{
				AmeMeasureSphere* sphere = new AmeMeasureSphere( static_cast<AmeMeasureSphere*>(current) );
				sphere->m_BasePoint = pos;
				sphere->m_fRadius = sphere->m_fRadius * scale_rate;
				base = sphere;
			}
			break;
		}
		// 登録
		AmeImageViewer* pTargetViewer = target_attr->GetViewer();
		if ( base != nullptr && pTargetViewer != nullptr )
		{
			base->SetImageAttr( target_attr->GetImageAttr() );
			base->m_iOrgViewerType = pTargetViewer->GetViewerType();
			base->m_FrameSerialID = pTargetViewer->GetViewerFrame()->GetSerialID();
			base->m_ViewerIdentity = target_attr->GetViewerAttrIDString();
			base->m_ImageAttrIDString = target_attr->GetImageAttr()->GetImageAttrIDString();

			AmeViewerAttr* pOverlayAttr = pTargetViewer->GetOverlayViewerAttr();
			if ( pOverlayAttr != NULL &&
				(pOverlayAttr->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
				pOverlayAttr->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
			{
				base->m_OverlayImageIdentity = pOverlayAttr->GetImageAttr()->GetImageAttrIDString();
			}
			else
			{
				base->m_OverlayImageIdentity = L"";
			}

			int index = 0;
			if ( m_pEngine->m_AllObjects.size() != 0 )
			{
				index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
			}
			base->SetIndex( index );

			m_pEngine->m_AllObjects.push_back( base );
			GetTaskManager()->AddDrawObject( base );
		}
	}

	return 1;
}

// サイズ入力
long AmeMeasurePlugInGUI::onCmdInputSize( PnwObject*, PnwEventType, PnwEventArgs* )
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == nullptr )
	{
		return 1;
	}

	AmeViewerAttr* base_attr = GetTaskManagerEngine()->GetViewerAttr( current->m_iMeasuredViewerAttrID[0] );
	if ( base_attr == NULL )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NOT_FOUND_MEASURED_IMAGE, AppAttr->GetResStr( "AME_MEASURE_NOT_FOUND_MEASURED_IMAGE" ), L"Not found measured image." );
		return 1;
	}

	float min_scale = base_attr->GetImageAttr()->GetMinVoxelSpacing();
	AmeFloat3D size( 0, 0, 0 );
	AmeFloat2D sign( 1, 1 );
	float angle = 0;
	bool xy_swap = false;
	// 現在のサイズを取得
	switch ( current->GetType() )
	{
		case AME_MEASURE_CUBE:
			size.X = static_cast<AmeMeasureCube*>(current)->m_Size.X;
			size.Y = static_cast<AmeMeasureCube*>(current)->m_Size.Y;
			angle = static_cast<AmeMeasureCube*>(current)->m_fAngle;
			break;
		case AME_MEASURE_ELLIPSE:
			size.X = static_cast<AmeMeasureEllipse*>(current)->m_Radius.X;
			size.Y = static_cast<AmeMeasureEllipse*>(current)->m_Radius.Y;
			angle = static_cast<AmeMeasureEllipse*>(current)->m_fAngle;
			break;
		case AME_MEASURE_BOX:
			size = static_cast<AmeMeasureBox*>(current)->m_Size;
			break;
		case AME_MEASURE_SPHERE:
			size.X = static_cast<AmeMeasureSphere*>(current)->m_fRadius;
			break;
		default:
			return 1;
	}

	size.X *= min_scale;
	size.Y *= min_scale;
	size.Z *= min_scale;

	if ( size.X < 0 )
	{
		size.X = -size.X;
		sign.X = -1;
	}
	if ( size.Y < 0 )
	{
		size.Y = -size.Y;
		sign.Y = -1;
	}

	// 角度に応じて縦横を反転
	if ( angle < 0 )
	{
		angle = 360 - fmod( fabs( angle ), 360 );
	}
	angle = fmod( angle, 360 );
	if ( (angle > 45 && angle < 135) || (angle > 225 && angle < 315) )
	{
		xy_swap = true;
		std::swap( size.X, size.Y );
	}

	// サイズ入力ダイアログ表示
	AmeMeasureSizeDialog dialog( GetTaskManager(), size, current->GetType() );
	if ( dialog.ShowDialog() != DIALOG_RESULT_OK )
	{
		return 1;
	}

	// サイズ設定
	size = dialog.GetSize();
	size.X /= min_scale;
	size.Y /= min_scale;
	size.Z /= min_scale;

	if ( xy_swap )
	{
		std::swap( size.X, size.Y );
	}

	if ( sign.X < 0 )
	{
		size.X = -size.X;
	}
	if ( sign.Y < 0 )
	{
		size.Y = -size.Y;
	}

	switch ( current->GetType() )
	{
		case AME_MEASURE_CUBE:
			static_cast<AmeMeasureCube*>(current)->m_Size.X = size.X;
			static_cast<AmeMeasureCube*>(current)->m_Size.Y = size.Y;
			break;
		case AME_MEASURE_ELLIPSE:
			static_cast<AmeMeasureEllipse*>(current)->m_Radius.X = size.X;
			static_cast<AmeMeasureEllipse*>(current)->m_Radius.Y = size.Y;
			break;
		case AME_MEASURE_BOX:
			static_cast<AmeMeasureBox*>(current)->m_Size = size;
			break;
		case AME_MEASURE_SPHERE:
			static_cast<AmeMeasureSphere*>(current)->m_fRadius = size.X;
			break;
	}

	// 再描画
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	return 1;
}


// サイズ入力
long AmeMeasurePlugInGUI::onCmdInputAngle( PnwObject*, PnwEventType, PnwEventArgs* )
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL ) return 1;

	switch ( current->GetType() )
	{
		case AME_MEASURE_ANGLE:
		{
			// サイズ入力ダイアログ表示
			float angle = static_cast<AmeMeasureAngle*>(current)->GetDegree();
			AmeMeasureAngleDialog dialog( GetTaskManager(), angle, current->GetType() );
			if ( dialog.ShowDialog() != DIALOG_RESULT_OK )
			{
				return 1;
			}

			// 角度を設定する
			angle = dialog.GetDegree();
			static_cast<AmeMeasureAngle*>(current)->SetDegree( angle );

			// 再描画
			GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
			return 1;
		}
		case AME_MEASURE_PROJ_ANGLE:
		{
			// サイズ入力ダイアログ表示
			float angle = static_cast<AmeMeasureProjAngle*>(current)->GetDegree( NULL );
			AmeMeasureAngleDialog dialog( GetTaskManager(), angle, current->GetType() );
			if ( dialog.ShowDialog() != DIALOG_RESULT_OK )
			{
				return 1;
			}

			// 角度を設定する
			AmeImageViewer* pViewer = m_pViewerAttr->GetViewer();
			angle = dialog.GetDegree();
			static_cast<AmeMeasureProjAngle*>(current)->SetDegree( pViewer, angle );

			// 再描画
			GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
			return 1;
		}
		default:
			break;
	}

	return 1;
}

// Input commnet 
void AmeMeasurePlugInGUI::SlotCmdComment( const int index, const AmeString comment )
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL )
	{
		return;
	}

	current->SetComment( comment );

	UpdateMeasurementList();

	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
}

// 詳細ダイアログ
long AmeMeasurePlugInGUI::onCmdResult( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pViewerAttr == NULL ) return 1;

	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current != NULL )
	{
		// 全体計測があれば消してカレント切り替え
		m_pEngine->DeleteImageMeasure();

		if ( (current->GetType() == AME_MEASURE_ANGLE) || (current->GetType() == AME_MEASURE_PROJ_ANGLE) || (current->GetType() == AME_MEASURE_TWO_LINE_ANGLE) || (current->GetType() == AME_MEASURE_POINT) )
		{
			GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NO_SUPPORT_GRAPH, AppAttr->GetResStr( "AME_MEASURE_NOT_GRAPH_MEASURE" ), L"This measure not show a graph." );
			return 1;
		}

		if ( m_bNowCreating && ((current->GetType() == AME_MEASURE_ROI) || (current->GetType() == AME_MEASURE_FREEHAND)) )
		{
			GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NO_FINISH, AppAttr->GetResStr( "AME_MEASURE_NOT_FINISHEDED_MEASURE" ), L"Not yet finished measure." );
			return 1;
		}
	}

	// ダイアログ表示
	ShowResultDialog( false );

	return 1;
}

//詳細ダイアログの表示
void AmeMeasurePlugInGUI::ShowResultDialog( bool bReposition )
{
	if ( m_pResultDialog == NULL )
	{
		m_pResultDialog = new AmeMeasureResultDialog( GetTaskManager(), this );
		bReposition = true;
	}
	if ( m_pResultDialog != NULL && !m_pResultDialog->GetVisible() )
	{
		// フレーム表示位置が変わったか
		if ( m_iDialogFramePositionID != AppAttr->m_MainWidget.pMainController->FindTaskMainFramePosition( GetTaskManager()->GetTaskGroupID(), false ) )
		{
			bReposition = true;
		}
	}

	if ( bReposition )
	{
		int x, y, width, height;
		GetPositionOnMainFrame( GetTaskManager(), x, y, width, height );
		m_pResultDialog->SetPosition( x + PnwApplication::GetScaledSize( 50 ), y + PnwApplication::GetScaledSize( 50 ), PnwApplication::GetScaledSize( 840 ), PnwApplication::GetScaledSize( 740 ) );

		m_iDialogFramePositionID = AppAttr->m_MainWidget.pMainController->FindTaskMainFramePosition( GetTaskManager()->GetTaskGroupID(), false );
	}

	m_pResultDialog->Show();

	if ( m_pViewerAttr != NULL && m_pViewerAttr->GetViewer() != NULL )
	{
		m_pViewerAttr->GetViewer()->Update();
	}

	UpdateHistogram();
	UpdateInterface();
}

// 全体計測
long AmeMeasurePlugInGUI::onCmdMeasureAll( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pViewerAttr == NULL )
	{
		return 1;
	}
	AmeImageViewer* pViewer = m_pViewerAttr->GetViewer();

	m_pEngine->DeleteImageMeasure();

	//計測可能な画像かどうか調べる
	bool bMeasuring = false;
	if ( m_pViewerAttr->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
	{
		bMeasuring = true;
	}
	if ( pViewer != NULL && pViewer->GetOverlayViewerAttr() != NULL &&
		pViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
	{
		bMeasuring = true;
	}
	if ( !bMeasuring )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NOT_SUPPORT_VIEWER, AppAttr->GetResStr( "AME_MEASURE_NOT_MEASURABLE_IMAGE" ), L"Not support viewer for this measure." );
		return 1;
	}

	AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Measuring all." );

	//マスクＯＮじゃなかったらＯＮにする
	if ( !m_pViewerAttr->m_bMaskOverlay )
	{
		AmeNotifyViewerAttributeParameter parameter;
		parameter.m_bMaskOverlay = true;
		GetTaskManagerEngine()->NotifyViewerAttributeChangeToAllPlugIn( m_pViewerAttr, AME_NOTIFY_MESSAGE_MASK_OVERLAY, parameter );
	}

	//マスク領域のみの計測であることをメッセージ表示
	AmeDontAskAgainInformationMessage( GetTaskManager(), GetPlugInName(), L"MeasureCalculateInMask", AppAttr->GetResStr( "AME_COMMON_INFORMATION" ), AppAttr->GetResStr( "AME_MEASURE_CALCULATE_IN_MASK" ) );

	AmeWaitCursorChanger changer;

	AmeMeasureAll* all = new AmeMeasureAll( GetTaskManager(), GetPlugInID(), m_pViewerAttr->GetImageAttr() );
	all->m_iMeasuredViewerAttrID[0] = m_pViewerAttr->GetViewerAttrID();
	all->m_iMeasuredImageAttrID[0] = m_pViewerAttr->GetImageAttr()->GetImageAttrID();
	all->Calculate( all->m_Result, m_pViewerAttr->GetActiveMask(), m_pViewerAttr->GetImageAttr() );
	if ( pViewer != NULL && pViewer->GetOverlayViewerAttr() != NULL )
	{
		AmeViewerAttr* ov_attr = pViewer->GetOverlayViewerAttr();
		all->m_iMeasuredViewerAttrID[1] = ov_attr->GetViewerAttrID();
		all->m_iMeasuredImageAttrID[1] = ov_attr->GetImageAttr()->GetImageAttrID();
		all->Calculate( all->m_Result2, ov_attr->GetActiveMask(), ov_attr->GetImageAttr() );
	}
	m_pEngine->m_pImageMeasure = all;

	ShowResultDialog( false );
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

	AppAttr->ShowQuickHelp( GetPlugInName(), L"MeasureMask" );

	return 1;
}

// 単一断面計測
long AmeMeasurePlugInGUI::onCmdMeasureSingle( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pViewerAttr == nullptr )
	{
		return 1;
	}
	AmeImageViewer* pViewer = m_pViewerAttr->GetViewer();
	if ( pViewer == nullptr )
	{
		return 1;
	}

	m_pEngine->DeleteImageMeasure();

	// MPRのみ
	switch ( pViewer->GetViewerType() )
	{
		case AME_VIEWER_XY:
		case AME_VIEWER_XZ:
		case AME_VIEWER_YZ:
			break;
		default:
			PnwMessageBox::Warning( GetTaskManager(), MESSAGE_BUTTON_OK, AppAttr->GetResStr( "AME_COMMON_WARNING" ), AppAttr->GetResStr( "AME_MEASURE_NOT_MPR" ) );
			return 1;
	}

	AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Measuring one slice." );

	//計測可能な画像かどうか調べる
	bool bMeasuring = false;
	if ( m_pViewerAttr->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
	{
		bMeasuring = true;
	}
	if ( pViewer->GetOverlayViewerAttr() != NULL &&
		pViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
	{
		bMeasuring = true;
	}
	if ( !bMeasuring )
	{
		GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NOT_SUPPORT_VIEWER, AppAttr->GetResStr( "AME_MEASURE_NOT_MEASURABLE_IMAGE" ), L"Not support viewer for this measure." );
		return 1;
	}

	//マスクＯＮじゃなかったらＯＮにする
	if ( !m_pViewerAttr->m_bMaskOverlay )
	{
		AmeNotifyViewerAttributeParameter parameter;
		parameter.m_bMaskOverlay = true;
		GetTaskManagerEngine()->NotifyViewerAttributeChangeToAllPlugIn( m_pViewerAttr, AME_NOTIFY_MESSAGE_MASK_OVERLAY, parameter );
	}

	// マスク領域のみの計測であることをメッセージ表示
	AmeDontAskAgainInformationMessage( GetTaskManager(), GetPlugInName(), L"MeasureCalculateInOneSliceMask", AppAttr->GetResStr( "AME_COMMON_INFORMATION" ), AppAttr->GetResStr( "AME_MEASURE_CALCULATE_IN_MASK" ) );

	// 表示
	AmeFloat3D point = m_pViewerAttr->m_CurrentPosition.ToFloat();

	AmeMeasureSingleSlice* single = new AmeMeasureSingleSlice( GetTaskManager(), GetPlugInID(), m_pViewerAttr->GetImageAttr() );
	single->m_iOrgViewerType = pViewer->GetViewerType();
	single->m_iMeasuredViewerAttrID[0] = m_pViewerAttr->GetViewerAttrID();
	single->m_iMeasuredImageAttrID[0] = m_pViewerAttr->GetImageAttr()->GetImageAttrID();
	single->Calculate( single->m_Result, point, m_pViewerAttr->GetActiveMask(), m_pViewerAttr->GetImageAttr() );
	m_pEngine->m_pImageMeasure = single;

	ShowResultDialog( false );
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

	AppAttr->ShowQuickHelp( GetPlugInName(), L"MeasureMask" );

	return 1;
}

// リスケールの表示切替
long AmeMeasurePlugInGUI::onCmdRescale( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pViewerAttr == NULL )
	{
		return 1;
	}

	//SUVじゃないＰＥＴや、リスケールスロープがあるＭＲで正しく動作しない。

	bool enabled = false;
	bool suv_enabled = false;
	bool valid_rescale = false;
	bool valid_suv = false;

	//現在の表示状態を取得
	if ( AmeGetModalityString( m_pViewerAttr->GetImageAttr()->GetDICOMHeader( 0 ) ) == L"PT" )
	{
		if ( m_pViewerAttr->GetImageAttr()->GetSUVCalculator()->IsValidSUV() )
		{
			valid_suv = true;
			enabled = m_pViewerAttr->GetImageAttr()->GetSUVCalculator()->IsEnabled();
			suv_enabled = enabled;
		}
	}
	AmeImageViewer* pViewer = m_pViewerAttr->GetViewer();
	if ( !enabled &&
		pViewer != NULL &&
		pViewer->GetOverlayViewerAttr() != NULL &&
		pViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
	{
		AmeImageAttr* imga2 = pViewer->GetOverlayViewerAttr()->GetImageAttr();
		if ( AmeGetModalityString( imga2->GetDICOMHeader( 0 ) ) == L"PT" )
		{
			if ( imga2->GetSUVCalculator()->IsValidSUV() )
			{
				valid_suv = true;
				enabled = imga2->GetSUVCalculator()->IsEnabled();
				suv_enabled = enabled;
			}
		}
	}
	if ( !enabled )
	{
		if ( m_pViewerAttr->GetImageAttr()->GetRescaleCalculator()->IsValidRescale() )
		{
			valid_rescale = true;
			enabled = m_pViewerAttr->GetImageAttr()->GetRescaleCalculator()->IsEnabled();
		}
	}
	if ( !enabled &&
		pViewer != NULL &&
		pViewer->GetOverlayViewerAttr() != NULL &&
		pViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
	{
		if ( pViewer->GetOverlayViewerAttr()->GetImageAttr()->GetRescaleCalculator()->IsValidRescale() )
		{
			valid_rescale = true;
			enabled = pViewer->GetOverlayViewerAttr()->GetImageAttr()->GetRescaleCalculator()->IsEnabled();
			suv_enabled = enabled;
		}
	}

	//  (Rescale適用あり)  --> (Rescale適用なし)  -->  (SUV適用あり)  -->　(Rescale適用あり) ....

	//反転した状態をセット
	if ( valid_rescale && valid_suv )
	{
		if ( enabled && !suv_enabled )
		{
			//(Rescale適用あり)
			enabled = false;
			suv_enabled = false;
		}
		else if ( !enabled && !suv_enabled )
		{
			//(Rescale適用なし)
			enabled = false;
			suv_enabled = true;
		}
		else
		{
			// (SUV適用あり) 
			enabled = true;
			suv_enabled = false;
		}
	}
	else if ( !valid_rescale && valid_suv )
	{
		suv_enabled = !suv_enabled;
	}
	else if ( valid_rescale && !valid_suv )
	{
		enabled = !enabled;
	}
	else
	{
		//この場合はボタンが押せない。
	}


	if ( AmeGetModalityString( m_pViewerAttr->GetImageAttr()->GetDICOMHeader( 0 ) ) == L"PT" )
	{
		if ( m_pViewerAttr->GetImageAttr()->GetSUVCalculator()->IsCalculatableMethod( m_pViewerAttr->GetImageAttr()->GetSUVCalculator()->GetCalculationMethod() ) )
		{
			m_pViewerAttr->GetImageAttr()->GetSUVCalculator()->SetEnabled( suv_enabled );
			m_pViewerAttr->GetImageAttr()->GetSUVCalculator()->UpdateParameter();
		}
	}

	if ( pViewer != NULL &&
		pViewer->GetOverlayViewerAttr() != NULL &&
		pViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
	{
		AmeImageAttr* imga2 = pViewer->GetOverlayViewerAttr()->GetImageAttr();
		if ( AmeGetModalityString( imga2->GetDICOMHeader( 0 ) ) == L"PT" )
		{
			if ( imga2->GetSUVCalculator()->IsCalculatableMethod( imga2->GetSUVCalculator()->GetCalculationMethod() ) )
			{
				imga2->GetSUVCalculator()->SetEnabled( suv_enabled );
				imga2->GetSUVCalculator()->UpdateParameter();
			}
		}
	}

	if ( m_pViewerAttr->GetImageAttr()->GetRescaleCalculator()->IsValidRescale() )
	{
		m_pViewerAttr->GetImageAttr()->GetRescaleCalculator()->SetEnabled( enabled );
	}
	if ( !enabled &&
		pViewer != NULL &&
		pViewer->GetOverlayViewerAttr() != NULL &&
		pViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) &&
		pViewer->GetOverlayViewerAttr()->GetImageAttr()->GetRescaleCalculator()->IsValidRescale() )
	{
		pViewer->GetOverlayViewerAttr()->GetImageAttr()->GetRescaleCalculator()->SetEnabled( enabled );
	}

	// テンプレートフレーム更新用にカレントをセットし直す
	GetTaskManager()->SetCurrentViewerAttrID( m_pViewerAttr->GetViewerAttrID() );


	//画面更新
	UpdateInterface();
	if ( m_pResultDialog != NULL )
	{
		m_pResultDialog->ClearMiddleRange();
		OnChangeMiddleLine( 0, 0, false );

		m_pResultDialog->UpdateHistogram();
	}
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

	return 1;
}

long AmeMeasurePlugInGUI::onCmdRescaleType( PnwObject*, PnwEventType, PnwEventArgs* )
{
	int index = m_pnwRescaleCombo->GetCurrentItem();

	AmeSUVCalculator::CalculationMethod method = AmeSUVCalculator::CALC_RESCALE;
	switch ( index )
	{
		case 0:
			method = AmeSUVCalculator::CALC_BODY_WEIGHT;
			break;
		case 1:
			method = AmeSUVCalculator::CALC_LEAN_BODY_MASS;
			break;
		case 2:
			method = AmeSUVCalculator::CALC_BODY_SURFACE_AREA;
			break;
		case 3:
			method = AmeSUVCalculator::CALC_IDEAL_BODY_WEIGHT;
			break;
		default:
			return 1;
	}

	for ( int i = 0; i < 2; i++ )
	{
		AmeImageAttr* imga = NULL;
		if ( i == 0 )
		{
			imga = m_pViewerAttr->GetImageAttr();
		}
		else
		{
			AmeImageViewer* pViewer = m_pViewerAttr->GetViewer();
			if ( pViewer != nullptr && pViewer->GetOverlayViewerAttr() != nullptr )
			{
				imga = pViewer->GetOverlayViewerAttr()->GetImageAttr();
			}
		}

		if ( imga == NULL ) continue;

		if ( imga->GetSUVCalculator()->IsValidSUV() )
		{
			if ( imga->GetSUVCalculator()->IsCalculatableMethod( method ) )
			{
				imga->GetSUVCalculator()->SetCalculationMethod( method );
			}
			else
			{
				GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_INVALID_PARAMETER, AppAttr->GetResStr( "AME_SUV_EVALUATE_NO_CALC_METHOD" ), L"Cannot apply this method (%d).", method );
			}
		}
	}

	UpdateInterface();
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	return 1;
}

// タイムインテンシティカーブの表示切替
long AmeMeasurePlugInGUI::onCmdTimeCurve( PnwObject*, PnwEventType, PnwEventArgs* )
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
	if ( current == NULL )
	{
		return 1;
	}

	AmeViewerAttr* attr = GetTaskManagerEngine()->GetViewerAttr( current->m_iMeasuredViewerAttrID[0] );
	if ( attr == NULL )
	{
		return 1;
	}

	switch ( current->GetType() )
	{
		case AME_MEASURE_CUBE:
		case AME_MEASURE_ELLIPSE:
		case AME_MEASURE_ROI:
		case AME_MEASURE_FREEHAND:
		case AME_MEASURE_BOX:
		case AME_MEASURE_SPHERE:
		{
			AmeMeasureRegionBase* region = dynamic_cast<AmeMeasureRegionBase*>(current);
			if ( region != NULL )
			{
				// 時間情報が有効かどうか
				if ( !region->m_bTimeCurve && !attr->GetTimeTable()->GetUnitValid() )
				{
					GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, AME_ERROR_WS_MEASURE_NO_VALID_TIME, AppAttr->GetResStr( "AME_MEASURE_NOT_TIME_UNIT_VALID" ), L"Time unit is not valid." );
					return 1;
				}
				region->m_bTimeCurve = !region->m_bTimeCurve;
				region->m_bMiddleValid = false; // X値が変わるので無効化


			}
		}
		break;
		default:
			return 1;
	}

	// todo_old: 該当ビューアだけでよい
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	UpdateInterface();

	//UIの更新
	// <---画面を更新した際（AmeTaskManager::AME_DISPLAY_UPDATE）に、内部的にヒストグラムが再計算される仕様。
	//構造上よろしくないが、現段階では構造的には手を入れれない。さらに、計測オブジェクト側からグラフＵＩ更新をしているが、カレントViewerAttr以外だと更新されない。
	//現段階で大きく手をいれることは困難。いつか根本的に書き直す必要あり。
	//UpdateHistogram();

	return 1;
}


// 設定変更を解析結果に反映
void AmeMeasurePlugInGUI::ApplySetting( void )
{
	// 設定が変更されれば作りかけのは確定してしまう
	if ( m_bNowCreating )
	{
		FinishCreatingMeasure();
	}

	//☆　四隅情報の削除
	if ( !m_pEngine->m_UserParams.m_bShowCornerInfo )
	{
		std::vector<AmeMeasureDrawObject*>::iterator it;
		for ( it = m_pEngine->m_AllObjects.begin(); it != m_pEngine->m_AllObjects.end(); ++it )
		{
			if ( (*it)->GetType() == AME_MEASURE_LINE || (*it)->GetType() == AME_MEASURE_TTTG ||
			   (*it)->GetType() == AME_MEASURE_POLYLINE || (*it)->GetType() == AME_MEASURE_CURVE )
			{
				m_pEngine->DeleteSelectedMeasureCornerInfo( *it );
			}
		}
		m_pEngine->m_map_measuredLength.clear();
	}
	//目盛り間隔の更新
	ChgStepLength();
}

// 作成途中の計測を確定
void AmeMeasurePlugInGUI::FinishCreatingMeasure()
{
	if ( !m_bNowCreating )
	{
		return;
	}

	m_bNowCreating = false;
	m_LiveWire.Counter.clear();
	m_LiveWire.SeedList.clear();

	ResetLiveWire();

	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( true );
	if ( current != NULL && current->GetType() == AME_MEASURE_ANGLE )
	{
		// 角度計測で３点に満たなければ追加しておく
		AmeMeasureAngle* angle = static_cast<AmeMeasureAngle*>(current);
		if ( !angle->m_vecPoint.empty() )
		{
			while ( angle->m_vecPoint.size() < 3 )
			{
				angle->m_vecPoint.push_back( angle->m_vecPoint[angle->m_vecPoint.size() - 1] );
			}
		}
	}
	if ( current != NULL && current->GetType() == AME_MEASURE_PROJ_ANGLE )
	{
		// 角度計測で３点に満たなければ追加しておく
		AmeMeasureProjAngle* angle = static_cast<AmeMeasureProjAngle*>(current);
		if ( !angle->m_vecProjectedPoint.empty() )
		{
			while ( angle->m_vecProjectedPoint.size() < 3 )
			{
				angle->m_vecProjectedPoint.push_back( angle->m_vecProjectedPoint[angle->m_vecProjectedPoint.size() - 1] );
			}
			//angle->m_vecProjectedPoint.resize(angle->m_vecProjectedPoint.size());
			//std::copy(angle->m_vecProjectedPoint.begin(), angle->m_vecProjectedPoint.end(), angle->m_vecProjectedPoint.begin());
		}
	}
	if ( current != NULL && current->GetType() == AME_MEASURE_TWO_LINE_ANGLE )
	{
		// ２線間角度計測で４点に満たなければ追加しておく
		AmeMeasureTwoLineAngle* angle = static_cast<AmeMeasureTwoLineAngle*>(current);
		if ( !angle->m_vecPoint.empty() )
		{
			while ( angle->m_vecPoint.size() < 4 )
			{
				angle->m_vecPoint.push_back( angle->m_vecPoint[angle->m_vecPoint.size() - 1] );
			}
		}
	}
	if ( current != NULL && current->GetType() == AME_MEASURE_POLYLINE )
	{
		AmeMeasurePolyLine* poly = static_cast<AmeMeasurePolyLine*>(current);
		int size = (int)poly->m_vecPoint.size();
		if ( size > 2 && (poly->m_vecPoint[size - 1] - poly->m_vecPoint[size - 2]).Norm() < 1e-5 )
		{
			poly->m_vecPoint.pop_back();
		}
	}
	if ( current != NULL && current->GetType() == AME_MEASURE_CURVE )
	{
		AmeMeasureCurve* curve = static_cast<AmeMeasureCurve*>(current);
		int size = (int)curve->m_vecPoint.size();
		if ( size > 2 && (curve->m_vecPoint[size - 1] - curve->m_vecPoint[size - 2]).Norm() < 1e-5 )
		{
			curve->m_vecPoint.pop_back();
		}
	}

	UpdateInterface();

	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
}

// UI表示の更新
void AmeMeasurePlugInGUI::UpdateInterface()
{
	//0番目以外計測させない。
	bool b_all_hide = false;
	if ( m_pViewerAttr != NULL && m_pViewerAttr->GetViewer() != NULL && m_pViewerAttr->GetViewer()->GetCurrentViewerAttrIndex() != 0 )
	{
		b_all_hide = true;
	}

	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		if ( m_pnwMeasureMethodBtn[i] != NULL )
		{
			m_pnwMeasureMethodBtn[i]->SetButtonState( m_CurrentType == i );

			bool bValidScale;
			if ( !b_all_hide && m_pViewerAttr != NULL && m_pViewerAttr->GetViewer() != NULL && IsMeasureCreatable( (AmeMeasureType)i, m_pViewerAttr->GetViewer(), bValidScale ) )
			{
				m_pnwMeasureMethodBtn[i]->SetEnabled( true );
			}
			else
			{
				m_pnwMeasureMethodBtn[i]->SetEnabled( false );
			}
		}
	}

	if ( m_pViewerAttr != NULL )
	{
		if ( !m_pViewerAttr->GetImageAttr()->IsEnableOperation( AmeImageAttr::ENABLE_TILT_MEASUREMENT ) )
		{
			if ( m_pnwMeasureMethodBtn[AME_MEASURE_BOX] != NULL )
			{
				m_pnwMeasureMethodBtn[AME_MEASURE_BOX]->SetEnabled( false );
			}
			if ( m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE] != NULL )
			{
				m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE]->SetEnabled( false );
			}
		}
	}

	//直線計測ボタン
	if ( m_pnwMeasureLineBtn != NULL )
	{

		std::vector<int>v_measure_line;
		v_measure_line.push_back( AME_MEASURE_LINE );
		v_measure_line.push_back( AME_MEASURE_PROJ_LINE );
		v_measure_line.push_back( AME_MEASURE_TTTG );
		v_measure_line.push_back( AME_MEASURE_CLOSEST );
		v_measure_line.push_back( AME_MEASURE_DIAMETER );
		bool enable_flag = false;
		bool status_flag = false;


		if ( !b_all_hide && m_pViewerAttr != NULL && m_pViewerAttr->GetViewer() != NULL )
		{
			for ( std::vector<int>::iterator it = v_measure_line.begin(); it != v_measure_line.end(); ++it )
			{
				bool bValidScale;
				enable_flag |= IsMeasureCreatable( (AmeMeasureType)(*it), m_pViewerAttr->GetViewer(), bValidScale );
				status_flag |= (m_CurrentType == (*it));
			}

			if ( enable_flag )
			{
				m_pnwMeasureLineBtn->SetButtonState( status_flag );
				if ( status_flag )
				{
					m_pnwMeasureLineBtn->SetIcon( GetIcon( m_CurrentType ) );
					m_pnwMeasureLineBtn->SetUserData( IntToPtr( m_CurrentType ) );
				}
			}
		}

		m_pnwMeasureLineBtn->SetEnabled( enable_flag );
	}

	//折れ線計測ボタン
	if ( m_pnwMeasurePolylineBtn != NULL )
	{
		std::vector<int>v_measure_polyline;
		v_measure_polyline.push_back( AME_MEASURE_POLYLINE );
		v_measure_polyline.push_back( AME_MEASURE_CURVE );

		bool enable_flag = false;
		bool status_flag = false;


		if ( !b_all_hide && m_pViewerAttr != NULL && m_pViewerAttr->GetViewer() != NULL )
		{
			for ( std::vector<int>::iterator it = v_measure_polyline.begin(); it != v_measure_polyline.end(); ++it )
			{
				bool bValidScale;
				enable_flag |= IsMeasureCreatable( (AmeMeasureType)(*it), m_pViewerAttr->GetViewer(), bValidScale );
				status_flag |= (m_CurrentType == (*it));
			}

			if ( enable_flag )
			{
				m_pnwMeasurePolylineBtn->SetButtonState( status_flag );
				if ( status_flag )
				{
					m_pnwMeasurePolylineBtn->SetIcon( GetIcon( m_CurrentType ) );
					m_pnwMeasurePolylineBtn->SetUserData( IntToPtr( m_CurrentType ) );
				}
			}
		}

		m_pnwMeasurePolylineBtn->SetEnabled( enable_flag );
	}

	//角度計測ボタン
	if ( m_pnwMeasureAngleBtn != NULL )
	{
		std::vector<int>v_measure_angle;
		v_measure_angle.push_back( AME_MEASURE_ANGLE );
		v_measure_angle.push_back( AME_MEASURE_PROJ_ANGLE );
		v_measure_angle.push_back( AME_MEASURE_TWO_LINE_ANGLE );

		bool enable_flag = false;
		bool status_flag = false;
		if ( !b_all_hide && m_pViewerAttr != NULL && m_pViewerAttr->GetViewer() != NULL )
		{
			for ( std::vector<int>::iterator it = v_measure_angle.begin(); it != v_measure_angle.end(); ++it )
			{
				bool bValidScale;
				enable_flag |= IsMeasureCreatable( (AmeMeasureType)(*it), m_pViewerAttr->GetViewer(), bValidScale );
				status_flag |= (m_CurrentType == (*it));
			}

			if ( enable_flag )
			{
				m_pnwMeasureAngleBtn->SetButtonState( status_flag );
				if ( status_flag )
				{
					m_pnwMeasureAngleBtn->SetIcon( GetIcon( m_CurrentType ) );
					m_pnwMeasureAngleBtn->SetUserData( IntToPtr( m_CurrentType ) );
				}
			}
		}
		m_pnwMeasureAngleBtn->SetEnabled( enable_flag );
	}

	//常に表示しているため、ここでenableにすると、手前の条件が無視される。
	//else{
	//	m_pnwMeasureMethodBtn[AME_MEASURE_BOX]->SetEnabled(true);
	//	m_pnwMeasureMethodBtn[AME_MEASURE_SPHERE]->SetEnabled(true);
	//}


	// グラフ表示
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( true );


	if ( m_frameMeasureList != NULL )
	{
		m_frameMeasureList->SetEnabledDeleteAll( !b_all_hide && m_pEngine->GetMeasureNum( NULL ) > 0 );
	}

	//色設定パネルの表示／非表示を追加
	for ( size_t i = FRAME_INI; i < m_SettingFrame.size(); i++ )
	{
		if ( m_SettingFrame[i] != NULL )
		{
			m_SettingFrame[i]->Hide();
		}
	}

	AmeMeasureType _type = GetCurrentType();

	//全部非表示時
	if ( b_all_hide )
	{
		_type = AME_MEASURE_NONE;
	}
	if ( m_pnwMeasureOperationFrame != nullptr && /*m_pnwOperationFrame != nullptr && */m_pnwClosestOperationFrame != nullptr && m_pnwDiameterOperationFrame != nullptr )
	{
		if ( _type == AME_MEASURE_CLOSEST )
		{
			m_pnwMeasureOperationFrame->SetHelpID(L"Operation_Closest");
			m_pnwMeasureOperationFrame->Show();
			//m_pnwOperationFrame->Hide();
			m_pnwClosestOperationFrame->Show();
			m_pnwDiameterOperationFrame->Hide();
		}
		else if ( _type == AME_MEASURE_DIAMETER )
		{
			m_pnwMeasureOperationFrame->SetHelpID( L"Operation_Diameter" );
			m_pnwMeasureOperationFrame->Show();
			//m_pnwOperationFrame->Hide();
			m_pnwClosestOperationFrame->Hide();
			m_pnwDiameterOperationFrame->Show();
		}
		else
		{
			m_pnwMeasureOperationFrame->SetHelpID(L"Operation");
			m_pnwMeasureOperationFrame->Hide();
			//m_pnwOperationFrame->Show();
			m_pnwClosestOperationFrame->Hide();
			m_pnwDiameterOperationFrame->Hide();
		}
	}
	if ( m_pnwMeasureSettingFrame != NULL )
	{
		if ( _type != AME_MEASURE_NONE && _type != AME_MEASURE_CLOSEST && _type != AME_MEASURE_DIAMETER )
		{
			m_pnwMeasureSettingFrame->Show();
		}
		else
		{
			m_pnwMeasureSettingFrame->Hide();
		}
	}
	if ( m_pnwMeasureDetailsFrame != nullptr )
	{
		if ( _type != AME_MEASURE_CLOSEST && _type != AME_MEASURE_DIAMETER )
		{
			m_pnwMeasureDetailsFrame->Show();
		}
		else
		{
			m_pnwMeasureDetailsFrame->Hide();
		}
	}
	if ( m_pnwMeasureLineTypeFrame != NULL && GetTaskManager()->GetUIConceptMode() == AME_UI_CONCEPT_MODE::DEEP )
	{
		switch ( _type )
		{
			case AME_MEASURE_LINE:
			case AME_MEASURE_PROJ_LINE:
			case AME_MEASURE_TTTG:
			case AME_MEASURE_CLOSEST:
			case AME_MEASURE_DIAMETER:
				m_pnwMeasureLineTypeFrame->Show();
				break;
			default:
				m_pnwMeasureLineTypeFrame->Hide();
				break;
		}
	}
	if ( m_pnwMeasurePolylineTypeFrame != NULL && GetTaskManager()->GetUIConceptMode() == AME_UI_CONCEPT_MODE::DEEP )
	{
		switch ( _type )
		{
			case AME_MEASURE_POLYLINE:
			case AME_MEASURE_CURVE:
				m_pnwMeasurePolylineTypeFrame->Show();
				break;
			default:
				m_pnwMeasurePolylineTypeFrame->Hide();
				break;
		}
	}
	if ( m_pnwMeasureAngleTypeFrame != NULL && GetTaskManager()->GetUIConceptMode() == AME_UI_CONCEPT_MODE::DEEP )
	{
		switch ( _type )
		{
			case AME_MEASURE_ANGLE:
			case AME_MEASURE_TWO_LINE_ANGLE:
			case AME_MEASURE_PROJ_ANGLE:
				m_pnwMeasureAngleTypeFrame->Show();
				break;
			default:
				m_pnwMeasureAngleTypeFrame->Hide();
				break;
		}
	}

	bool bExistSettingFrame = true;
	if ( m_SettingFrame.size() < FRAME_NUM )
	{
		bExistSettingFrame = false;
	}
	else
	{
		for ( int i = FRAME_INI; i < FRAME_NUM; i++ )
		{
			if ( m_SettingFrame[i] == NULL )
			{
				bExistSettingFrame = false;
				break;
			}
		}
	}
	if ( bExistSettingFrame )
	{
		// 設定パネル変更
		switch ( _type )
		{
			case AME_MEASURE_ANGLE:
			case AME_MEASURE_ROI:
			case AME_MEASURE_FREEHAND:
				m_SettingFrame[FRAME_FONT]->Show();
				m_SettingFrame[FRAME_FORECOLOR]->Show();
				m_SettingFrame[FRAME_BACKCOLOR]->Show();
				m_SettingFrame[FRAME_CPCOLOR]->Show();
				break;
			case AME_MEASURE_TWO_LINE_ANGLE:
				m_SettingFrame[FRAME_FONT]->Show();
				m_SettingFrame[FRAME_FORECOLOR]->Show();
				m_SettingFrame[FRAME_BACKCOLOR]->Show();
				m_SettingFrame[FRAME_CPCOLOR]->Show();
				m_SettingFrame[FRAME_4POINTWARNING]->Show();
				break;
			case AME_MEASURE_PROJ_ANGLE:
				m_SettingFrame[FRAME_FONT]->Show();
				m_SettingFrame[FRAME_FORECOLOR]->Show();
				m_SettingFrame[FRAME_BACKCOLOR]->Show();
				m_SettingFrame[FRAME_CPCOLOR]->Show();
				m_SettingFrame[FRAME_PROJWARNING]->Show();
				break;
			case AME_MEASURE_CUBE:
			case AME_MEASURE_ELLIPSE:
			case AME_MEASURE_BOX:
			case AME_MEASURE_SPHERE:
				m_SettingFrame[FRAME_FONT]->Show();
				m_SettingFrame[FRAME_FORECOLOR]->Show();
				m_SettingFrame[FRAME_BACKCOLOR]->Show();
				m_SettingFrame[FRAME_CPCOLOR]->Show();
				m_SettingFrame[FRAME_SIZEBUTTON]->Show();
				break;
			case AME_MEASURE_POINT:
				m_SettingFrame[FRAME_FONT]->Show();
				m_SettingFrame[FRAME_FORECOLOR]->Show();
				m_SettingFrame[FRAME_CURSORSLIDER]->Show();
				break;
			case AME_MEASURE_LINE:
			case AME_MEASURE_PROJ_LINE:
			case AME_MEASURE_TTTG:
				m_SettingFrame[FRAME_FONT]->Show();
				m_SettingFrame[FRAME_FORECOLOR]->Show();
				m_SettingFrame[FRAME_BACKCOLOR]->Show();
				m_SettingFrame[FRAME_CPCOLOR]->Show();
				ChangeLineOptionItemText( AME_MEASURE_LINE );
				if ( _type != AME_MEASURE_TTTG )
				{
					m_SettingFrame[FRAME_LINEOPTION]->Show();
				}
				if ( _type == AME_MEASURE_PROJ_LINE )
				{
					m_SettingFrame[FRAME_PROJWARNING]->Show();
				}
				m_pnwLineOption->Show();
				m_pnwCurveOption->Hide();
				break;
			case AME_MEASURE_POLYLINE:
				m_SettingFrame[FRAME_FONT]->Show();
				m_SettingFrame[FRAME_FORECOLOR]->Show();
				m_SettingFrame[FRAME_BACKCOLOR]->Show();
				m_SettingFrame[FRAME_CPCOLOR]->Show();
				ChangeLineOptionItemText( AME_MEASURE_POLYLINE );
				m_SettingFrame[FRAME_LINEOPTION]->Show();
				m_pnwLineOption->Show();
				m_pnwCurveOption->Hide();
				break;
			case AME_MEASURE_CURVE:
				m_SettingFrame[FRAME_FONT]->Show();
				m_SettingFrame[FRAME_FORECOLOR]->Show();
				m_SettingFrame[FRAME_BACKCOLOR]->Show();
				m_SettingFrame[FRAME_CPCOLOR]->Show();
				m_SettingFrame[FRAME_LINEOPTION]->Show();
				m_pnwLineOption->Hide();
				m_pnwCurveOption->Show();
				break;
		}
	}


	if ( !b_all_hide && current != NULL )
	{
		if ( m_frameMeasureList != nullptr )
		{
			m_frameMeasureList->SetEnabledItemOperation( true );
		}
		if ( m_pnwGraphButton != NULL )
		{
			bool bGraph = false;
			for ( int i = 0; i < 2; i++ )
			{
				AmeViewerAttr* attr = GetTaskManagerEngine()->GetViewerAttr( current->m_iMeasuredViewerAttrID[i] );
				if ( nullptr == attr || !attr->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
				{
				}
				else
				{
					bGraph = true;
					break;
				}
			}
			if ( bGraph )
			{
				m_pnwGraphButton->SetEnabled( current->GetType() != AME_MEASURE_PROJ_LINE );
			}
			else
			{
				// 体積計測が不可能の場合はグラフに何も表示されないため、グラフ表示ボタン自体を無効化する
				m_pnwGraphButton->SetEnabled( false );
			}
		}

		if ( m_pnwCopyToAllButton != NULL )
		{
			if ( m_pViewerAttr == nullptr || m_pViewerAttr->GetViewer() == nullptr || m_pViewerAttr->GetViewer()->GetViewerType() == AME_VIEWER_VOLUME )
			{
				m_pnwCopyToAllButton->SetEnabled( false );
			}
			else
			{
				AmeBaseAnalysisPlugInDesktopGUI* plugin = dynamic_cast<AmeBaseAnalysisPlugInDesktopGUI*>(GetTaskManager()->GetTaskCardPlugInGUI().m_pPlugIn);
				switch ( current->GetType() )
				{
					case AME_MEASURE_CUBE:
					case AME_MEASURE_ELLIPSE:
						if ( plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) != L"" &&
							 current->m_iMeasuredViewerAttrID[0] == m_pViewerAttr->GetViewerAttrID() )
						{
							m_pnwCopyToAllButton->SetEnabled( true );
						}
						else
						{
							m_pnwCopyToAllButton->SetEnabled( false );
						}
						break;
					case AME_MEASURE_ROI:
					case AME_MEASURE_FREEHAND:
						// Do not enable during creation.
						if ( !m_bNowCreating && plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) != L"" &&
							  current->m_iMeasuredViewerAttrID[0] == m_pViewerAttr->GetViewerAttrID() )
						{
							m_pnwCopyToAllButton->SetEnabled( true );
						}
						else
						{
							m_pnwCopyToAllButton->SetEnabled( false );
						}
						break;
					case AME_MEASURE_POLYLINE:
					case AME_MEASURE_CURVE:
					case AME_MEASURE_ANGLE:
						if ( !m_bNowCreating && plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) != L"" )
						{
							m_pnwCopyToAllButton->SetEnabled( true );
						}
						else
						{
							m_pnwCopyToAllButton->SetEnabled( false );
						}
						break;
					case AME_MEASURE_TWO_LINE_ANGLE:
					case AME_MEASURE_TTTG:
						if ( !m_bNowCreating && plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) != L"" &&
							  current->m_iMeasuredViewerAttrID[0] == m_pViewerAttr->GetViewerAttrID() )
						{
							m_pnwCopyToAllButton->SetEnabled( true );
						}
						else
						{
							m_pnwCopyToAllButton->SetEnabled( false );
						}
						break;
					case AME_MEASURE_PROJ_LINE:
					case AME_MEASURE_PROJ_ANGLE:
						m_pnwCopyToAllButton->SetEnabled( false );
						break;
					default:
						if ( plugin->RequestMessage( L"MEASURE_COPY_TO_ALL", this ) != L"" )
						{
							m_pnwCopyToAllButton->SetEnabled( true );
						}
						else
						{
							m_pnwCopyToAllButton->SetEnabled( false );
						}
						break;
				}
			}
		}

		if ( m_pnwInputSizeButton != NULL )
		{
			switch ( current->GetType() )
			{
				case AME_MEASURE_CUBE:
				case AME_MEASURE_ELLIPSE:
				case AME_MEASURE_BOX:
				case AME_MEASURE_SPHERE:
					m_pnwInputSizeButton->SetEnabled( true );
					break;
				default:
					m_pnwInputSizeButton->SetEnabled( false );
					break;
			}
		}

	}
	else
	{
		if ( m_frameMeasureList != nullptr )
		{
			m_frameMeasureList->SetEnabledItemOperation( false );
		}
		if ( m_pnwGraphButton != NULL )
		{
			m_pnwGraphButton->SetEnabled( false );
		}
		if ( m_pnwCopyToAllButton != NULL )
		{
			m_pnwCopyToAllButton->SetEnabled( false );
		}
		if ( m_pnwInputSizeButton != NULL )
		{
			m_pnwInputSizeButton->SetEnabled( false );
		}
	}

	if ( m_pResultDialog != NULL )
	{
		if ( !b_all_hide && current != NULL )
		{
			switch ( current->GetType() )
			{
				case AME_MEASURE_CUBE:
				case AME_MEASURE_ELLIPSE:
				case AME_MEASURE_ROI:
				case AME_MEASURE_FREEHAND:
				case AME_MEASURE_BOX:
				case AME_MEASURE_SPHERE:
				{
					bool bTime = false;
					std::vector<AmeViewerAttr*> attrlist = GetTaskManager()->GetAllVisibleViewerAttrs();
					std::vector<AmeViewerAttr*>::iterator it;
					for ( it = attrlist.begin(); it != attrlist.end(); ++it )
					{
						if ( current->m_ViewerIdentity == (*it)->GetViewerAttrIDString() )
						{
							bTime = ((*it)->GetTimeTable()->GetNumOfImageAttr() > 1);
							break;
						}
					}
					if ( bTime )
					{
						m_pResultDialog->m_pnwTimeCurveButton->Show();
						m_pResultDialog->m_pnwTimeCurveButton->SetEnabled( true );
						m_pResultDialog->m_pnwTimeCurveButton->SetButtonState( ((AmeMeasureRegionBase*)current)->m_bTimeCurve );
					}
					else
					{
						m_pResultDialog->m_pnwTimeCurveButton->Hide();
						m_pResultDialog->m_pnwTimeCurveButton->SetEnabled( false );
					}
				}
				break;
				default:
					m_pResultDialog->m_pnwTimeCurveButton->SetEnabled( false );
					break;
			}
		}
		else
		{
			m_pResultDialog->m_pnwTimeCurveButton->SetEnabled( false );
		}
	}

	bool bUseRescale = true;
	if ( current != NULL )
	{
		switch ( current->GetType() )
		{
			case AME_MEASURE_PROJ_LINE:
				bUseRescale = false;
				break;
		}
	}

	// リスケール状態
	if ( m_pnwRescaleFrame != NULL && m_pnwRescaleButton != NULL && m_pnwRescaleCombo != NULL )
	{
		if ( !b_all_hide && m_pViewerAttr != NULL && bUseRescale )
		{
			int target_num = 0;
			bool bCalcable = false;
			bool bSUVCalcable = false;
			bool bGY = false;
			AmeString scaling_str[2];
			AmeString scaling_short_str[2];
			std::vector<AmeString> suv_types[2];
			int suv_method[2] = {0, 0};

			// SUV・リスケール計算可能かチェック
			if ( GetValueUnitString( m_pViewerAttr, scaling_str[0], scaling_short_str[0], suv_types[0], bCalcable, bSUVCalcable, bGY, NULL, NULL, &suv_method[0] ) )
			{
				target_num++;
			}

			if ( m_pViewerAttr->GetViewer() != NULL && m_pViewerAttr->GetViewer()->GetOverlayViewerAttr() != NULL )
			{
				if ( GetValueUnitString( m_pViewerAttr->GetViewer()->GetOverlayViewerAttr(), scaling_str[1], scaling_short_str[1], suv_types[1], bCalcable, bSUVCalcable, bGY, NULL, NULL, &suv_method[1] ) )
				{
					target_num++;
				}
			}

			// UI更新
			if ( bCalcable || bSUVCalcable )
			{
				m_pnwRescaleButton->Show();
			}
			else
			{
				m_pnwRescaleButton->Hide();
			}

			AmeString label_str;
			for ( int i = 0; i < 2; i++ )
			{
				if ( scaling_str[i] != L"" )
				{
					if ( label_str != L"" )
					{
						label_str += L" | ";
					}
					if ( target_num > 1 )
					{
						label_str += scaling_short_str[i];
					}
					else
					{
						label_str += scaling_str[i];
					}
				}
			}

			m_pnwRescaleCombo->ClearItems();
			bool suv_valid = false;
			if ( AppAttr->GetDynamicSetting()->bSUVCorrectionMethod )
			{
				for ( int n = 0; n < 2; n++ )
				{
					if ( suv_types[n].size() > 0 )
					{
						for ( size_t i = 0; i < suv_types[n].size(); i++ )
						{
							m_pnwRescaleCombo->AddItem( suv_types[n][i] );
						}
						switch ( suv_method[n] )
						{
							case AmeSUVCalculator::CALC_BODY_WEIGHT:		m_pnwRescaleCombo->SetCurrentItem( 0 ); break;
							case AmeSUVCalculator::CALC_LEAN_BODY_MASS:		m_pnwRescaleCombo->SetCurrentItem( 1 ); break;
							case AmeSUVCalculator::CALC_BODY_SURFACE_AREA:	m_pnwRescaleCombo->SetCurrentItem( 2 ); break;
							case AmeSUVCalculator::CALC_IDEAL_BODY_WEIGHT:	m_pnwRescaleCombo->SetCurrentItem( 3 ); break;
						}
						m_pnwRescaleCombo->SetEnabled( true );
						suv_valid = true;
						break;
					}
				}
			}
			if ( !suv_valid )
			{
				m_pnwRescaleCombo->AddItem( label_str );
				m_pnwRescaleCombo->SetEnabled( false );
			}
			m_pnwRescaleCombo->SetText( label_str );

			m_pnwRescaleFrame->Show();
		}
		else
		{
			m_pnwRescaleFrame->Hide();
		}
	}

	UpdateMeasurementList();

	AmeString helpID;
	switch ( _type )
	{
		case AME_MEASURE_LINE:
		case AME_MEASURE_PROJ_LINE:
		case AME_MEASURE_TTTG:
			helpID = L"MeasureLine";
			break;
		case AME_MEASURE_POLYLINE:
		case AME_MEASURE_CURVE:
			helpID = L"MeasurePolyLine";
			break;
		case AME_MEASURE_ANGLE:
		case AME_MEASURE_PROJ_ANGLE:
		case AME_MEASURE_TWO_LINE_ANGLE:
			helpID = L"MeasureAngle";
			break;
		case AME_MEASURE_CUBE:
			helpID = L"MeasureCube";
			break;
		case AME_MEASURE_ELLIPSE:
			helpID = L"MeasureEllipse";
			break;
		case AME_MEASURE_ROI:
			helpID = L"MeasureROI";
			break;
		case AME_MEASURE_FREEHAND:
			helpID = L"MeasureFreeHand";
			break;
		case AME_MEASURE_POINT:
			helpID = L"MeasurePoint";
			break;
		case AME_MEASURE_BOX:
			helpID = L"MeasureBox";
			break;
		case AME_MEASURE_SPHERE:
			helpID = L"MeasureSphere";
			break;
		case AME_MEASURE_CLOSEST:
			helpID = L"ClosestPoint";
			break;
		case AME_MEASURE_DIAMETER:
			helpID = L"Diameter";
			break;
	}
	if ( helpID != L"" )
	{
		if ( m_pnwMeasureMethodGroup != nullptr )
		{
			((AmeManagedGroupBox*)m_pnwMeasureMethodGroup)->SetHelpID( helpID );
		}
		if ( IsCurrentMouseMode() )
		{
			AppAttr->ShowQuickHelp( GetPlugInName(), helpID );
		}
	}
	else
	{
		if ( m_pnwMeasureMethodGroup != nullptr )
		{
			((AmeManagedGroupBox*)m_pnwMeasureMethodGroup)->SetHelpID( L"MeasureMethod" );
		}
	}

}

void AmeMeasurePlugInGUI::UpdateInterfaceForShortcutBar( AmeViewerAttr* attr )
{

	//0番目以外計測させない。
	bool b_all_hide = false;
	if ( attr != nullptr )
	{
		AmeImageViewer* pViewer = attr->GetViewer();
		if ( pViewer != nullptr && pViewer->GetCurrentViewerAttrIndex() != 0 )
		{
			b_all_hide = true;
		}
	}

	bool bMacroRegistration = GetTaskManagerEngine()->IsMacroRegistration();

	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		if ( m_pnwShortCutBtn[i] != NULL )
		{
			m_pnwShortCutBtn[i]->SetButtonState( m_CurrentType == i );

			if ( attr == nullptr )
			{
				m_pnwShortCutBtn[i]->SetEnabled( false );
				continue;
			}
			AmeImageViewer* pViewer = attr->GetViewer();

			bool bValidScale;
			if ( !b_all_hide && pViewer != nullptr && !bMacroRegistration && IsMeasureCreatable( (AmeMeasureType)i, pViewer, bValidScale ) )
			{
				m_pnwShortCutBtn[i]->SetEnabled( true );
			}
			else
			{
				m_pnwShortCutBtn[i]->SetEnabled( false );
			}
		}
	}

	if ( attr != nullptr )
	{
		if ( !attr->GetImageAttr()->IsEnableOperation( AmeImageAttr::ENABLE_TILT_MEASUREMENT ) || bMacroRegistration )
		{
			if ( m_pnwShortCutBtn[AME_MEASURE_BOX] != NULL )
			{
				m_pnwShortCutBtn[AME_MEASURE_BOX]->SetEnabled( false );
			}
			if ( m_pnwShortCutBtn[AME_MEASURE_SPHERE] != NULL )
			{
				m_pnwShortCutBtn[AME_MEASURE_SPHERE]->SetEnabled( false );
			}
		}
	}
}

void AmeMeasurePlugInGUI::UpdateMeasurementList()
{
	// リストが空 or アノテーションが配列に保存されていないならリストの更新は不要
	if ( m_frameMeasureList == nullptr )
	{
		return;
	}

	Ame::ListWidget* list = m_frameMeasureList->GetMeasurementList();
	if ( m_pEngine->m_AllObjects.empty() )
	{
		list->ClearItems();
		return;
	}

	auto current_obj = m_pEngine->GetCurrentMeasure( false );

	list->BeginUpdate(); // <!-- お作法：更新のタイミングを制御

	list->ClearItems();
	for ( auto obj_itr = m_pEngine->m_AllObjects.begin(); obj_itr != m_pEngine->m_AllObjects.end(); obj_itr++ )
	{
		AmeString label = L"\t";
		switch ( (*obj_itr)->GetType() )
		{
			case AME_MEASURE_LINE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_LINE" );
				break;
			case AME_MEASURE_POLYLINE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_POLYLINE" );
				break;
			case AME_MEASURE_ANGLE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_ANGLE" );
				break;
			case AME_MEASURE_CUBE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_CUBE" );
				break;
			case AME_MEASURE_ELLIPSE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_ELLI" );
				break;
			case AME_MEASURE_ROI:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_ROI" );
				break;
			case AME_MEASURE_FREEHAND:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_FREE" );
				break;
			case AME_MEASURE_POINT:
				label += AppAttr->GetResStr( "AME_MEASURE_POINT" );
				break;
			case AME_MEASURE_BOX:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_BOX" );
				break;
			case AME_MEASURE_SPHERE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_SPHERE" );
				break;
			case AME_MEASURE_PROJ_ANGLE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_PROJ_ANGLE" );
				break;
			case AME_MEASURE_TWO_LINE_ANGLE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_TWO_LINE_ANGLE" );
				break;
			case AME_MEASURE_PROJ_LINE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_PROJ_LINE" );
				break;
			case AME_MEASURE_CURVE:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_CURVE" );
				break;
			case AME_MEASURE_TTTG:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_TTTG" );
				break;
			case AME_MEASURE_CLOSEST:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_CLOSEST" );
				break;
			case AME_MEASURE_DIAMETER:
				label += AppAttr->GetResStr( "AME_MEASURE_MES_DIAMETER" );
				break;
		}
		label += L"\t" + (*obj_itr)->GetComment();

		list->AddItem( label );
		if ( (*obj_itr)->m_bVisible )
		{
			list->SetItemDetailSubIcon( list->GetNumItems() - 1, 0, 0, AppAttr->GetAppIcon( AME_ICON_CHECK ) );
		}
		else
		{
			list->SetItemDetailSubIcon( list->GetNumItems() - 1, 0, 0, AppAttr->GetAppIcon( AME_ICON_BLANK ) );
		}
		list->SetItemData( list->GetNumItems() - 1, (*obj_itr)->GetObjectID() );
		if ( (*obj_itr) == current_obj )
		{
			list->SetCurrentItem( list->GetNumItems() - 1 );
		}
	}

	if ( list->GetNumItems() > 0 )
	{
		// リスト上のカレントを最新に維持
		list->MakeItemVisible( list->GetCurrentItem() );
		list->EndUpdate();
	}
}


void AmeMeasurePlugInGUI::ChangeLineOptionItemText( int type )
{
	if ( m_pnwLineOption == NULL )
	{
		return;
	}

	if ( type == AME_MEASURE_LINE )
	{
		m_pnwLineOption->SetItemText( 2, AppAttr->GetResStr( "AME_MEASURE_LINE_OPTION_CS_SCALE" ) );
	}
	else if ( type == AME_MEASURE_POLYLINE )
	{
		m_pnwLineOption->SetItemText( 2, AppAttr->GetResStr( "AME_MEASURE_LINE_OPTION_SECTION_LENGTH" ) );
	}
}

// 値の単位取得
bool AmeMeasurePlugInGUI::GetValueUnitString( AmeViewerAttr* attr, AmeString& unit, AmeString& short_unit, std::vector<AmeString>& suv_types, bool& bCalcable, bool& bSUVCalcable, bool& bGYCalcable, bool* pRescaled, bool* pSUV, int* pSUVMethod )
{
	// 計算不能なものは×
	if ( attr == NULL || !attr->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) )
	{
		return false;
	}

	AmeImageAttr* imga = attr->GetImageAttr();

	// SUV・リスケール計算可能かチェック
	bool bRescaled = false;
	bool bSUV = false;
	bool bNormalized = false;
	bool bRescaledShift = false;
	bool bShift = false;
	bool bOriginal = false;
	AmeSUVCalculator::CalculationMethod suv_method = AmeSUVCalculator::CALC_RESCALE;

	// PETなら
	if ( AmeGetModalityString( imga->GetDICOMHeader( 0 ) ) == L"PT" )
	{
		if ( imga->GetSUVCalculator()->IsCalculatableMethod( imga->GetSUVCalculator()->GetCalculationMethod() ) )
		{
			if ( !imga->GetSUVCalculator()->IsValidSUV() && imga->GetDICOMLoadParam().bPixelValueRescaled )
			{
				// ※SUV計算不可かつリスケール適用済みの場合、切替不可
			}
			else
			{
				bSUVCalcable = true;
			}
		}

		if ( imga->GetSUVCalculator()->IsEnabled() )
		{
			bRescaled = true;
			if ( imga->GetSUVCalculator()->IsValidSUV() )
			{
				bSUV = true;
				suv_method = imga->GetSUVCalculator()->GetCalculationMethod();
			}
			if ( imga->GetDICOMLoadParam().bPixelValueOverflow )
			{
				bRescaledShift = true;
			}
		}
	}

	bool bGY = false;

	if ( L"RTDOSE" == AmeGetModalityStringFromSOPClass( imga->GetDICOMHeader( 0 ) ) && IsSupportedDOSEImage( imga ) )
	{
		bGY = true;
		bGYCalcable = true;
	}

	// 信号値がPET計算できない場合
	if ( !bRescaled )
	{
		if ( imga->GetRescaleCalculator()->IsValidRescale() )
		{
			if ( imga->GetDICOMLoadParam().bPixelValueRescaled )
			{
				// 画素値がリスケール適用済みなら変更しても意味がないのでfalseのままにしておく
			}
			else
			{
				bCalcable = true;
			}
		}

		if ( imga->GetRescaleCalculator()->IsEnabled() )
		{
			// リスケール計算可能
			bRescaled = true;
			if ( imga->GetDICOMLoadParam().bPixelValueOverflow )
			{
				if ( imga->GetDICOMLoadParam().bPixelValueOverflowAdjusted )
				{
					// denominator,shiftで調整されているのでリスケール値が計算される
				}
				else
				{
					bRescaledShift = true;	// 切捨てが行われているのでシフト扱いが必要
				}
			}
		}
		else if ( imga->GetDICOMLoadParam().bPixelValueRescaled )
		{
			// リスケール適用済み
			bRescaled = true;
			if ( imga->GetDICOMLoadParam().bPixelValueOverflow )
			{
				bRescaledShift = true;
			}
		}
		else
		{
			// リスケール非適用
			if ( imga->GetDICOMLoadParam().bPixelValueRescaleNormalizedOnly )
			{
				bNormalized = true;
			}
			else
			{
				bOriginal = true;
			}
			if ( imga->GetDICOMLoadParam().bPixelValueOverflow )
			{
				bShift = true; // シフト格納値
			}
		}
	}

	if ( bSUV )
	{
		if ( AppAttr->GetDynamicSetting()->bSUVCorrectionMethod )
		{
			bool types = true;
			switch ( suv_method )
			{
				case AmeSUVCalculator::CALC_BODY_WEIGHT:
					unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_BW" );
					short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_BW" );
					break;
				case AmeSUVCalculator::CALC_LEAN_BODY_MASS:
					unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_LBM" );
					short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_LBM" );
					break;
				case AmeSUVCalculator::CALC_BODY_SURFACE_AREA:
					unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_BSA" );
					short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_BSA" );
					break;
				case AmeSUVCalculator::CALC_IDEAL_BODY_WEIGHT:
					unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_IBW" );
					short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_IBW" );
					break;
				default:
					unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV" );
					short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_S" );
					types = false;
					break;
			}
			if ( types )
			{
				suv_types.push_back( AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_BW" ) );
				suv_types.push_back( AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_LBM" ) );
				suv_types.push_back( AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_BSA" ) );
				suv_types.push_back( AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_IBW" ) );
			}
		}
		else
		{
			unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV" );
			short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SUV_S" );
		}
	}
	else if ( bGY )
	{
		switch ( AppAttr->GetStaticSetting()->DisplayGyUnit )
		{
			case GY_UNITS_GY:
			{
				unit = AppAttr->GetResStr( "AME_COMMON_UNIT_GRAY" );
				short_unit = AppAttr->GetResStr( "AME_COMMON_UNIT_GRAY" );
			}
			break;
			case GY_UNITS_CGY:
			{
				unit = AppAttr->GetResStr( "AME_COMMON_UNIT_CENTI_GRAY" );
				short_unit = AppAttr->GetResStr( "AME_COMMON_UNIT_CENTI_GRAY" );
			}
			break;
		}
	}
	else if ( bShift )
	{
		unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SHIFT_ORIGINAL" );
		short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_SHIFT_ORIGINAL_S" );
	}
	else if ( bRescaledShift )
	{
		unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_NO_ORIGINAL" );
		short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_NO_ORIGINAL_S" );
	}
	else if ( bRescaled )
	{
		unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_RESCALE" );
		short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_RESCALE_S" );
	}
	else if ( bNormalized )
	{
		unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_NORMALIZE" );
		short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_NORMALIZE_S" );
	}
	else
	{
		unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_ORIGINAL" );
		short_unit = AppAttr->GetResStr( "AME_COMMON_PIXEL_VALUE_ORIGINAL_S" );
	}

	if ( pRescaled != NULL )
	{
		*pRescaled = bRescaled;
	}
	if ( pSUV != NULL )
	{
		*pSUV = bSUV;
	}
	if ( pSUVMethod != NULL )
	{
		*pSUVMethod = suv_method;
	}

	return true;
}


//! カレントの変更
void AmeMeasurePlugInGUI::ChangeCurrentByID( int objectID )
{
	m_pEngine->ChangeCurrentByID( objectID );

	AmeMeasureDrawObject* obj = m_pEngine->GetMeasureByID( objectID );
	if ( obj != NULL )
	{
		if ( m_CurrentType != AME_MEASURE_NONE )
		{
			m_CurrentType = obj->GetType();
		}

		// 計測ダイアログを閉じる
		switch ( obj->GetType() )
		{
			case AME_MEASURE_ANGLE:
			case AME_MEASURE_PROJ_ANGLE:
			case AME_MEASURE_TWO_LINE_ANGLE:
			case AME_MEASURE_POINT:
				if ( m_pResultDialog != NULL && m_pResultDialog->GetVisible() )
				{
					m_pResultDialog->Hide();
				}
				break;
		}
	}

	UpdateHistogram();
	UpdateInterface();
	UpdateInterfaceForShortcutBar( m_pViewerAttr );

}

// CurrentPositionのnotify
void AmeMeasurePlugInGUI::ChangeCurrentPosition( AmeViewerAttr* attr, const AmeFloat3D& currentpos )
{
	AmeNotifyViewerAttributeParameter parameter;
	parameter.m_CurrentPosition = currentpos;
	// 他のプラグインに通知
	GetTaskManagerEngine()->NotifyViewerAttributeChangeToAllPlugIn( attr, AME_NOTIFY_MESSAGE_CP, parameter );
}

// Get measure list
Ame::MeasureFrameList* AmeMeasurePlugInGUI::GetMeasureFrameList()
{
	return m_frameMeasureList;
}

// 途中修正
void AmeMeasurePlugInGUI::OnChangeMiddleLine( float start, float end, bool valid )
{
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( true );
	if ( current == NULL )
	{
		return;
	}

	switch ( current->GetType() )
	{
		case AME_MEASURE_LINE:
		case AME_MEASURE_PROJ_LINE:
		case AME_MEASURE_TTTG:
		case AME_MEASURE_POLYLINE:
		case AME_MEASURE_CURVE:
		case AME_MEASURE_CUBE:
		case AME_MEASURE_ELLIPSE:
		case AME_MEASURE_ROI:
		case AME_MEASURE_FREEHAND:
		case AME_MEASURE_BOX:
		case AME_MEASURE_SPHERE:
		case AME_MEASURE_ALL:
		case AME_MEASURE_SINGLE:
			current->m_bMiddleValid = valid;
			current->m_fMiddleStart = start;
			current->m_fMiddleEnd = end;
			break;
	}
}


// ヒストグラムの更新
void AmeMeasurePlugInGUI::UpdateHistogram( void )
{
	if ( m_pResultDialog != NULL && m_pResultDialog->GetVisible() )
	{
		m_pResultDialog->UpdateHistogram();
	}
}

// 描画コールバック
AmeDrawCallbackFunc AmeMeasurePlugInGUI::GetDrawCallbackFunction()
{
	return DrawCallbackFunc;
}

void AmeMeasurePlugInGUI::NotifySyncSnapshotForGUI( const AmeSyncSnapshotNotifyType notify_type )
{
	switch ( notify_type )
	{
		case AME_SYNC_SNAPSHOT_NOTIFY_MESSAGE_START_COMPARE:
		case AME_SYNC_SNAPSHOT_NOTIFY_MESSAGE_CHANGE_ENGINE_LIST:
		{
			// 描画オブジェクトをTaskManagerに登録しておく
			for ( size_t i = 0; i < m_pEngine->m_AllObjects.size(); i++ )
			{
				AmeDrawObject* obj = m_pEngine->m_AllObjects[i];
				if ( obj->GetObjectID() == -1 )
				{
					obj->SetOwner( GetTaskManager(), GetPlugInID() );
					GetTaskManager()->AddDrawObject( obj );
					// objがAmeMeasureDrawObjectであることが前提。snapshot読み込み用だが要見直し。
					dynamic_cast<AmeMeasureDrawObject*>(obj)->GetCurrentColorSetting().Init();
				}
			}
		}
		break;
	}
}

// 描画コールバック
void AmeMeasurePlugInGUI::DrawCallbackFunc( AmeImageCanvas* viewer, PnwGraphicsBase* dc, const AmeCaptureArea&, const AmeInt2D& drawSize, void* pFuncArg )
{
	AmeViewerAttr* attr = viewer->GetViewerAttr();
	if ( attr == NULL )
	{
		return;
	}

	// モードチェック
	AmeMeasurePlugInGUI* plugin = (AmeMeasurePlugInGUI*)pFuncArg;

	//他のマウスモードでも描写する(右クリックメニューから)
	if ( plugin->GetTaskManager()->GetCurrentViewerAttr() == viewer->GetViewerAttr() )
	{
		if ( plugin->m_pEngine->m_pImageMeasure != NULL )
		{
			// 指定範囲のオーバーレイ表示
			plugin->m_pEngine->m_pImageMeasure->Draw( dc, viewer, drawSize );
		}
	}

	if ( !plugin->IsCurrentMouseMode() )
	{
		return;
	}

	if ( plugin->m_pCopiedMeasure != NULL )
	{
		// コピー中の計測のゴースト表示
		if ( plugin->m_pCopiedMeasure->IsVisible( viewer ) &&
			plugin->m_iCopiedImageAttrID == attr->GetImageAttr()->GetImageAttrID() )
		{
			plugin->m_pCopiedMeasure->Draw( dc, viewer, drawSize );
		}
	}
}

bool AmeMeasurePlugInGUI::DisplayToVoxelForRaysum( AmeImageViewer* viewer, const AmeFloat2D& disp_pos, AmeFloat3D& ret_pos )
{
	AmeViewerAttr* attr = viewer->GetViewerAttr();
	AmeFloat3D scale = attr->GetImageAttr()->GetVoxelSpacing();

	AmeFloat3D center = attr->GetRenderingInfo()->GetRotationCenter();
	AmeFloat2D disp_center;
	viewer->VoxelToDisplay( center, disp_center );

	AmeFloat3D axisX = viewer->GetViewerAxisX();
	AmeFloat3D axisY = viewer->GetViewerAxisY();
	float draw_rate = viewer->GetDrawScale();

	AmeFloat2D diff = (disp_pos - disp_center) / draw_rate;
	ret_pos = center + ConvertIsoToVoxel( diff.X * axisX, scale ) + ConvertIsoToVoxel( diff.Y * axisY, scale );

	return true;
}


// 左ボタンプレス
long AmeMeasurePlugInGUI::onLeftBtnPress( AmeImageViewer* viewer, PnwEventArgs* event, void* own_plugin )
{
	AmeViewerAttr* attr = viewer->GetViewerAttr();
	if ( attr == nullptr )
	{
		return 0;
	}

	AmeMeasurePlugInGUI* base_plugin = (AmeMeasurePlugInGUI*)own_plugin;

	AmeFloat3D pos;
	bool valid = false;
	if ( (viewer->GetViewerType() == AME_VIEWER_VOLUME) || (viewer->GetViewerType() == AME_VIEWER_MULTI_3D) )
	{
		valid = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos, AME_CALCULATION_TARGET_1, -1, -1, AmeImageEngineBase::AdditionalInfo( 0.999999f, 0.0f ) );
	}
	else
	{
		valid = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
	}

	bool valid2 = false;
	if ( viewer->GetOverlayViewerAttr() != nullptr && viewer->GetViewerType() != AME_VIEWER_VOLUME )
	{
		AmeFloat3D pos2;
		valid2 = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos2, AME_CALCULATION_TARGET_2 );
	}

	// コピーを生成
	if ( base_plugin->m_bNowCopying )
	{
		base_plugin->FinishCopy( viewer );
		base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
		return 1;
	}

	// 作成中の計測の処理
	if ( base_plugin->m_bNowCreating )
	{
		AmeMeasureDrawObject* draw = base_plugin->m_pEngine->GetCurrentMeasure( false );
		if ( draw != nullptr )
		{
			switch ( draw->GetType() )
			{
				case AME_MEASURE_ANGLE:
					if ( base_plugin->m_iCreatingImageAttrID == attr->GetImageAttr()->GetImageAttrID() )
					{
						if ( (valid || valid2) || IsPlaneViewer( viewer ) )
						{
							AmeMeasureAngle* angle = static_cast<AmeMeasureAngle*>(draw);
							if ( angle->m_vecPoint.size() >= 3 )
							{
								// 作成終了
								base_plugin->m_bNowCreating = false;
								base_plugin->UpdateInterface();
							}
							else
							{
								// 点を追加
								if ( angle->m_vecPoint.size() != 0 )
								{
									angle->m_vecPoint[angle->m_vecPoint.size() - 1] = pos;
								}
								angle->m_vecPoint.push_back( pos );
							}

							if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
							{
								base_plugin->ChangeCurrentPosition( attr, pos );
							}
							base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
						}
					}
					else
					{
						// 違うビューアがクリックされたら作成を中止する
						base_plugin->DeleteSelectedMeasure();

						if ( base_plugin->m_pResultDialog != NULL )
						{
							base_plugin->m_pResultDialog->Hide();
							base_plugin->m_pResultDialog->InitDisplayRange();
						}
						base_plugin->UpdateInterface();
						base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
					}
					return 1;
				case AME_MEASURE_PROJ_ANGLE:
					if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
					{

						AmeFloat2D _pos = AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y );
						AmeMeasureProjAngle* angle = static_cast<AmeMeasureProjAngle*>(draw);
						if ( angle->m_vecProjectedPoint.size() >= 3 )
						{
							// 作成終了
							base_plugin->m_bNowCreating = false;
							angle->StorePoints( attr );
							base_plugin->UpdateInterface();
						}
						else
						{
							// 点を追加
							if ( angle->m_vecProjectedPoint.size() != 0 )
							{
								angle->m_vecProjectedPoint[angle->m_vecProjectedPoint.size() - 1] = _pos;
							}
							angle->m_vecProjectedPoint.push_back( _pos );
						}
						base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
					}
					else
					{
						// 違うビューアがクリックされたら作成を中止する
						base_plugin->DeleteSelectedMeasure();

						if ( base_plugin->m_pResultDialog != nullptr )
						{
							base_plugin->m_pResultDialog->Hide();
							base_plugin->m_pResultDialog->InitDisplayRange();
						}
						base_plugin->UpdateInterface();
						base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
					}
					return 1;
				case AME_MEASURE_TWO_LINE_ANGLE:
					if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
					{
						if ( viewer->GetViewerType() == AME_VIEWER_VOLUME && attr->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE )
						{
							valid = DisplayToVoxelForRaysum( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
						}
						if ( (valid || valid2) || IsPlaneViewer( viewer ) )
						{
							AmeMeasureTwoLineAngle* angle = static_cast<AmeMeasureTwoLineAngle*>(draw);
							if ( angle->m_vecPoint.size() >= 4 )
							{
								// 作成終了
								base_plugin->m_bNowCreating = false;
								base_plugin->UpdateInterface();
							}
							else
							{
								// 点を追加
								if ( angle->m_vecPoint.size() != 0 )
								{
									angle->m_vecPoint[angle->m_vecPoint.size() - 1] = pos;
								}
								angle->m_vecPoint.push_back( pos );
							}

							if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
							{
								base_plugin->ChangeCurrentPosition( attr, pos );
							}
							base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
						}
					}
					else
					{
						// 違うビューアがクリックされたら作成を中止する
						base_plugin->DeleteSelectedMeasure();

						if ( base_plugin->m_pResultDialog != nullptr )
						{
							base_plugin->m_pResultDialog->Hide();
							base_plugin->m_pResultDialog->InitDisplayRange();
						}
						base_plugin->UpdateInterface();
						base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
					}
					return 1;
				case AME_MEASURE_POLYLINE:
					if ( base_plugin->m_iCreatingImageAttrID == attr->GetImageAttr()->GetImageAttrID() )
					{
						if ( (valid || valid2) || IsPlaneViewer( viewer ) )
						{
							// 点を追加
							AmeMeasurePolyLine* line = static_cast<AmeMeasurePolyLine*>(draw);
							if ( line->m_vecPoint.size() != 0 )
							{
								line->m_vecPoint[line->m_vecPoint.size() - 1] = pos;
							}
							line->m_vecPoint.push_back( pos );

							if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
							{
								base_plugin->ChangeCurrentPosition( attr, pos );
							}
							base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
						}
					}
					else
					{
						// 違う画像がクリックされたら確定する
						base_plugin->FinishCreatingMeasure();
					}

					return 1;
				case AME_MEASURE_CURVE:
					if ( base_plugin->m_iCreatingImageAttrID == attr->GetImageAttr()->GetImageAttrID() )
					{
						if ( (valid || valid2) || IsPlaneViewer( viewer ) )
						{
							// 点を追加
							AmeMeasureCurve* curve = static_cast<AmeMeasureCurve*>(draw);
							if ( curve->m_vecPoint.size() != 0 )
							{
								curve->m_vecPoint[curve->m_vecPoint.size() - 1] = pos;
							}
							curve->m_vecPoint.push_back( pos );

							if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
							{
								base_plugin->ChangeCurrentPosition( attr, pos );
							}
							base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
						}
					}
					else
					{
						// 違う画像がクリックされたら確定する
						base_plugin->FinishCreatingMeasure();
					}
					return 1;
				case AME_MEASURE_ROI:
					// 作成開始したビューアかどうかチェック
					if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
					{
						if ( (valid || valid2) || IsPlaneViewer( viewer ) )
						{
							// 点を追加
							AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(draw);
							AmeFloat3D axisX = viewer->GetViewerAxisX();
							AmeFloat3D axisY = viewer->GetViewerAxisY();
							AmeFloat3D disp_vec = ConvertVoxelToIso( pos - roi->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );
							AmeFloat2D pos2d( axisX.InnerProduct( disp_vec ), axisY.InnerProduct( disp_vec ) );
							if ( roi->m_vecPoints.size() != 0 )
							{
								roi->m_vecPoints[roi->m_vecPoints.size() - 1] = pos2d;
							}
							roi->m_vecPoints.push_back( pos2d );
							roi->AdjustGravity( viewer );

							if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
							{
								base_plugin->ChangeCurrentPosition( attr, pos );
							}
							base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
						}
					}
					else
					{
						// 違うビューアがクリックされたら確定する
						base_plugin->FinishCreatingMeasure();
					}
					return 1;
				case AME_MEASURE_FREEHAND:
					// 作成開始したビューアかどうかチェック
					if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
					{
						if ( IsPlaneViewer( viewer ) )
						{
							// LiveWireを確定して点を追加
							base_plugin->FinishFreehandLiveWire( attr, static_cast<AmeMeasureRoi*>(draw) );
							if ( valid2 )
							{
								AmeMeasureFreehand* freehand = static_cast<AmeMeasureFreehand*>(draw);
								if ( freehand->m_vecPoints.size() >= 2 )
								{
									AmeFloat2D previewsPt = freehand->m_vecPoints.back();

									AmeFloat3D client_pos;
									viewer->DisplayToVoxel( event->ClientPosition.ToFloat(), client_pos );
									AmeFloat3D offset = ConvertVoxelToIso( client_pos - freehand->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );

									AmeFloat2D currentPt = AmeFloat2D(
										offset.InnerProduct( viewer->GetViewerAxisX() ),
										offset.InnerProduct( viewer->GetViewerAxisY() ) );

									if ( (currentPt - previewsPt).Norm() > (double)AppAttr->m_WidgetSize.AME_POINT_GRAB_MARGIN )
									{
										freehand->FillControlPoints( previewsPt, currentPt );
									}
								}
							}
							base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
						}
					}
					return 1;
			}
		}
	}

	base_plugin->m_iGrab = -1;

	bool bCreatable = false;
	if ( base_plugin->m_CurrentType != AME_MEASURE_NONE )
	{
		// 計測を作成可能なビューアかどうか調べる
		bool validScale;
		if ( !IsMeasureCreatable( base_plugin->m_CurrentType, viewer, validScale ) )
		{
			viewer->Ungrab();
			base_plugin->m_bFailedToCreate = true;
			if ( validScale )
			{
				PnwMessageBox::Warning( base_plugin->GetTaskManager(), MESSAGE_BUTTON_OK, AppAttr->GetResStr( "AME_COMMON_WARNING" ), AppAttr->GetResStr( "AME_MEASURE_NOT_PLANE" ) );
			}
			else
			{
				PnwMessageBox::Warning( base_plugin->GetTaskManager(), MESSAGE_BUTTON_OK, AppAttr->GetResStr( "AME_COMMON_WARNING" ), AppAttr->GetResStr( "AME_MEASURE_NOT_PIXEL_SCALE" ) );
			}
			return 1;
		}

		bCreatable = true;
	}

	if ( base_plugin->m_CurrentType == AME_MEASURE_TWO_LINE_ANGLE )
	{
		if ( viewer->GetViewerType() == AME_VIEWER_VOLUME && attr->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE )
		{
			valid = DisplayToVoxelForRaysum( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
		}
	}

	if ( base_plugin->m_CurrentType == AME_MEASURE_PROJ_ANGLE || base_plugin->m_CurrentType == AME_MEASURE_PROJ_LINE )
	{
		if ( (viewer->GetViewerType() == AME_VIEWER_VOLUME || viewer->GetViewerType() == AME_VIEWER_MULTI_3D || viewer->GetViewerType() == AME_VIEWER_SURFACE) && viewer->GetViewerAttr()->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE )
		{
			valid = true;
		}
	}
	if ( (valid || valid2) && (base_plugin->m_CurrentType == AME_MEASURE_CLOSEST || base_plugin->m_CurrentType == AME_MEASURE_DIAMETER) )
	{ // 最近点計算用ROIを表示
		switch ( viewer->GetViewerType() )
		{
			case AME_VIEWER_VOLUME:
				if ( base_plugin->m_ROIData.m_BoxROIDrawer != nullptr )
				{
					base_plugin->m_ROIData.m_BoxROIDrawer->SetImageAttr( attr->GetImageAttr() ); // 画像を入れ替える(入れ替えないと描画されなくなる場合がある)
					base_plugin->m_ROIData.m_BoxROIDrawer->SetBox(pos, AmeFloat3D::Zero());
					base_plugin->m_ROIData.m_BoxROIDrawer->EnableDisplay(true);
					base_plugin->m_ROIData.m_BoxROIDrawer->SetVertexColor(AppAttr->GetAppColor(AME_COLOR_FRAMESELECTCOLOR));
					base_plugin->m_ROIData.m_BoxROIDrawer->SetEdgeColor(AppAttr->GetAppColor(AME_COLOR_FRAMESELECTCOLOR));
					base_plugin->m_ROIData.m_BoxROIDrawer->SetHighlightWidth(base_plugin->m_pEngine->m_UserParams.m_fHighlightPlaneWidth);
					base_plugin->m_ROIData.m_targetROIViewerAttribute = attr;
					base_plugin->m_bNowCreating = true;
					base_plugin->m_iCreatingViewerAttrID = attr->GetViewerAttrID();
					base_plugin->m_iCreatingImageAttrID = attr->GetImageAttr()->GetImageAttrID();
				}
				if ( base_plugin->m_ROIData.m_RectROIDrawer != nullptr )
				{
					base_plugin->m_ROIData.m_RectROIDrawer->EnableDisplay( false );
				}
				break;
			case AME_VIEWER_XY:
			case AME_VIEWER_XZ:
			case AME_VIEWER_YZ:
			case AME_VIEWER_OBLIQUE:
				if (base_plugin->m_ROIData.m_BoxROIDrawer != nullptr)
				{
					PnwCursor* cursor;
					if (base_plugin->m_ROIData.m_BoxROIDrawer->IsDisplay() && base_plugin->m_ROIData.m_BoxROIDrawer->HitTest(viewer, event, cursor) >= 0) {
						// 直方体ROIが表示されていて、かつ何かをハンドルしている場合
						break;
					}
					base_plugin->m_ROIData.m_BoxROIDrawer->EnableDisplay(false);
				}
				if ( base_plugin->m_ROIData.m_RectROIDrawer != nullptr )
				{
					base_plugin->m_ROIData.m_RectROIDrawer->SetImageAttr( attr->GetImageAttr() ); // 画像を入れ替える(入れ替えないと描画されなくなる場合がある)
					base_plugin->m_ROIData.m_RectROIDrawer->SetTargetCanvas(*viewer);
					base_plugin->m_ROIData.m_RectROIDrawer->SetRect(pos, AmeFloat2D::Zero());
					base_plugin->m_ROIData.m_RectROIDrawer->EnableDisplay(true);
					base_plugin->m_ROIData.m_RectROIDrawer->SetVertexColor(AppAttr->GetAppColor(AME_COLOR_FRAMESELECTCOLOR));
					base_plugin->m_ROIData.m_RectROIDrawer->SetEdgeColor(AppAttr->GetAppColor(AME_COLOR_FRAMESELECTCOLOR));
					base_plugin->m_ROIData.m_targetROIViewerAttribute = attr;
					base_plugin->m_bNowCreating = true;
					base_plugin->m_iCreatingViewerAttrID = attr->GetViewerAttrID();
					base_plugin->m_iCreatingImageAttrID = attr->GetImageAttr()->GetImageAttrID();
				}
				break;
		}
	}
	// 計測を新規に作成
	if ( bCreatable && (valid || valid2) )
	{
		// 計測を作成可能なビューアかどうか調べる
		bool validScale;
		if ( !IsMeasureCreatable( base_plugin->m_CurrentType, viewer, validScale ) )
		{
			viewer->Ungrab();
			base_plugin->m_bFailedToCreate = true;
			if ( validScale )
			{
				PnwMessageBox::Warning( base_plugin->GetTaskManager(), MESSAGE_BUTTON_OK, AppAttr->GetResStr( "AME_COMMON_WARNING" ), AppAttr->GetResStr( "AME_MEASURE_NOT_PLANE" ) );
			}
			else
			{
				PnwMessageBox::Warning( base_plugin->GetTaskManager(), MESSAGE_BUTTON_OK, AppAttr->GetResStr( "AME_COMMON_WARNING" ), AppAttr->GetResStr( "AME_MEASURE_NOT_PIXEL_SCALE" ) );
			}
			return 1;
		}

		AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Creating measurement. (type: %d)", base_plugin->m_CurrentType );

		AmeMeasureDrawObject* base = nullptr;
		bool bCPR = (viewer->GetViewerType() == AME_VIEWER_CPR);

		switch ( base_plugin->m_CurrentType )
		{
			// 直線計測
			case AME_MEASURE_LINE:
			{
				AmeMeasureLine* line = new AmeMeasureLine( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				line->m_vecPoint[0] = pos;
				line->m_vecPoint[1] = pos;
				if ( bCPR )
				{
					AmeFloat2D coord;
					line->m_bValidCPR = viewer->DisplayToPath( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), coord );
					line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
					line->m_vecCPRPoint[0] = coord;
					line->m_vecCPRPoint[1] = coord;
				}
				base = line;
			}
			break;

			// 直線計測
			case AME_MEASURE_PROJ_LINE:
			{
				AmeMeasureProjectedLine* proj_line = new AmeMeasureProjectedLine( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				AmeFloat2D _pos = AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y );
				proj_line->m_vecProjectedPoint.push_back( _pos );
				proj_line->m_vecProjectedPoint.push_back( _pos );
				base = proj_line;
			}
			break;

			// TT-TG
			case AME_MEASURE_TTTG:
			{
				AmeMeasureTTTG* tttg = new AmeMeasureTTTG( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				for ( int i = 0; i < AmeMeasureTTTG::CTRL_PT_NUM; i++ )
				{
					tttg->m_vecPoint[i] = pos;
				}
				tttg->UpdateControlPoints( viewer );
				base = tttg;
			}
			break;

			// 折れ線計測
			case AME_MEASURE_POLYLINE:
			{
				AmeMeasurePolyLine* line = new AmeMeasurePolyLine( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				line->m_vecPoint.push_back( pos );
				line->m_vecPoint.push_back( pos );
				base = line;
			}
			break;
			// 曲線計測
			case AME_MEASURE_CURVE:
			{
				AmeMeasureCurve* curve = new AmeMeasureCurve( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				curve->m_vecPoint.push_back( pos );
				curve->m_vecPoint.push_back( pos );
				base = curve;
			}
			break;
			// 角度計測
			case AME_MEASURE_ANGLE:
			{
				AmeMeasureAngle* angle = new AmeMeasureAngle( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				angle->m_vecPoint.push_back( pos );
				angle->m_vecPoint.push_back( pos );
				base = angle;
			}
			break;

			// 投影面３点角度計測
			case AME_MEASURE_PROJ_ANGLE:
			{
				AmeMeasureProjAngle* angle = new AmeMeasureProjAngle( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				AmeFloat2D _pos = AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y );
				angle->m_vecProjectedPoint.push_back( _pos );
				angle->m_vecProjectedPoint.push_back( _pos );
				base = angle;
			}
			break;

			// ２線間角度計測
			case AME_MEASURE_TWO_LINE_ANGLE:
			{
				AmeMeasureTwoLineAngle* angle = new AmeMeasureTwoLineAngle( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				angle->m_vecPoint.push_back( pos );
				angle->m_vecPoint.push_back( pos );
				base = angle;
			}
			break;

			// 矩形計測
			case AME_MEASURE_CUBE:
			{
				AmeMeasureCube* cube = new AmeMeasureCube( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				cube->m_BasePoint = pos;
				base = cube;
			}
			break;

			// 楕円計測
			case AME_MEASURE_ELLIPSE:
			{
				AmeMeasureEllipse* ellipse = new AmeMeasureEllipse( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				ellipse->m_BasePoint = pos;
				base = ellipse;
			}
			break;

			// 多角形計測
			case AME_MEASURE_ROI:
			{
				AmeMeasureRoi* roi = new AmeMeasureRoi( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				roi->m_BasePoint = pos;
				roi->m_vecPoints.push_back( AmeFloat2D( 0, 0 ) );
				roi->m_vecPoints.push_back( AmeFloat2D( 0, 0 ) );
				base = roi;
			}
			break;

			// フリーハンド計測
			case AME_MEASURE_FREEHAND:
			{
				AmeMeasureFreehand* roi = new AmeMeasureFreehand( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				roi->m_BasePoint = pos;
				roi->m_vecPoints.push_back( AmeFloat2D( 0, 0 ) );
				base_plugin->m_LiveWire.Counter.clear();
				base_plugin->m_LiveWire.Counter.push_back( 1 );
				base_plugin->m_LiveWire.SeedList.clear();
				base_plugin->m_LiveWire.SeedList.push_back( pos.ToInt() );
				base = roi;
			}
			break;

			// 直方体計測
			case AME_MEASURE_BOX:
			{
				AmeMeasureBox* box = new AmeMeasureBox( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				box->m_BasePoint = pos;
				base = box;
			}
			break;

			// 球体計測
			case AME_MEASURE_SPHERE:
			{
				AmeMeasureSphere* sphere = new AmeMeasureSphere( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				sphere->m_BasePoint = pos;
				base = sphere;
			}
			break;

			// 信号値計測
			case AME_MEASURE_POINT:
			{
				AmeMeasurePoint* line = new AmeMeasurePoint( base_plugin->GetTaskManager(), base_plugin->GetPlugInID(), attr->GetImageAttr() );
				//必ずボクセル中心
				line->m_Point = pos.ToRoundInt().ToFloat();
				if ( bCPR )
				{
					line->m_bValidCPR = viewer->DisplayToPath( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), line->m_CPRPoint );
					line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();

					AmePath* path = attr->GetCPRPathObject();
					if ( path != nullptr )
					{
						line->m_iCPRUpdateCounter = path->GetUpdatePointsCounter();
					}
				}
				base = line;
			}
			break;
		}

		// 登録
		if ( base != nullptr )
		{
			base_plugin->m_bNowCreating = true; // 作成モードに入る
			std::vector<AmeMeasureDrawObject*> measures = base_plugin->m_pEngine->GetAllMeasure();
			base->AdjustResultBoxPosition( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), (int)measures.size() );
			base->m_iOrgViewerType = viewer->GetViewerType();
			base->m_FrameSerialID = viewer->GetViewerFrame()->GetSerialID();
			base->m_ViewerIdentity = attr->GetViewerAttrIDString();
			base->m_ImageAttrIDString = attr->GetImageAttr()->GetImageAttrIDString();

			if ( viewer->GetOverlayViewerAttr() != nullptr &&
			   (viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
				viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
			{
				base->m_OverlayImageIdentity = viewer->GetOverlayViewerAttr()->GetImageAttr()->GetImageAttrIDString();
			}
			else
			{
				base->m_OverlayImageIdentity = L"";
			}

			int index = 0;
			if ( base_plugin->m_pEngine->m_AllObjects.size() != 0 )
			{
				index = base_plugin->m_pEngine->m_AllObjects.back()->GetIndex() + 1;
			}
			base->SetIndex( index );

			base_plugin->m_pEngine->m_AllObjects.push_back( base ); // Adds a new measuring object
			base_plugin->GetTaskManager()->AddDrawObject( base );

			base_plugin->ChangeCurrentByID( base->GetObjectID() );

			base_plugin->m_iGrab = (int)base_plugin->m_pEngine->m_AllObjects.size() - 1;
			base_plugin->m_iCreatingViewerAttrID = attr->GetViewerAttrID();
			base_plugin->m_iCreatingImageAttrID = attr->GetImageAttr()->GetImageAttrID();

			if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
			{
				base_plugin->ChangeCurrentPosition( attr, pos );
			}
			base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

			if ( !base_plugin->m_pEngine->m_UserParams.m_bContinual && !(event->PressedKeyState & AmeGetControlKeyMask()) )
			{

				base_plugin->ClearCurrentSelection();

			}
		}
		//色設定GUI更新
		base_plugin->UpdateInterface();
		base_plugin->UpdateInterfaceForShortcutBar( attr );
		base_plugin->UpdateToolBarShortCutBtn();

		return 1;
	}
	return 0;
}

// モードを外す
void AmeMeasurePlugInGUI::ClearCurrentSelection()
{
	if ( m_CurrentType == AME_MEASURE_NONE )
	{
		return;
	}

	m_CurrentType = AME_MEASURE_NONE;
	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		if ( m_pnwMeasureMethodBtn[i] != nullptr )
		{
			m_pnwMeasureMethodBtn[i]->SetButtonState( false );
		}
		if ( m_pnwShortCutBtn[i] != nullptr )
		{
			m_pnwShortCutBtn[i]->SetButtonState( false );
		}
	}
	if ( m_pnwMeasureLineBtn != nullptr )
	{
		m_pnwMeasureLineBtn->SetButtonState( false );
	}
	if ( m_pnwMeasurePolylineBtn != nullptr )
	{
		m_pnwMeasurePolylineBtn->SetButtonState( false );
	}
	if ( m_pnwMeasureAngleBtn != nullptr )
	{
		m_pnwMeasureAngleBtn->SetButtonState( false );
	}
}


// 左ボタンリリース
long AmeMeasurePlugInGUI::onLeftBtnRelease( AmeImageViewer* viewer, PnwEventArgs* event, void* own_plugin )
{
	AmeMeasurePlugInGUI* base_plugin = (AmeMeasurePlugInGUI*)own_plugin;

	if ( base_plugin != nullptr )
	{
		base_plugin->m_bFailedToCreate = false;
	}

	AmeViewerAttr* attr = viewer->GetViewerAttr();
	if ( attr == nullptr )
	{
		return 0;
	}

	// 作成中の計測の処理
	if ( base_plugin != nullptr && base_plugin->m_bNowCreating )
	{
		if ( base_plugin->GetCurrentType() == AME_MEASURE_CLOSEST || base_plugin->GetCurrentType() == AME_MEASURE_DIAMETER )
		{
			if (base_plugin->m_ROIData.m_BoxROIDrawer != nullptr && base_plugin->m_ROIData.m_BoxROIDrawer->IsDisplay())
			{
				base_plugin->m_ROIData.m_BoxROIDrawer->Normalize();
			}
			if (base_plugin->m_ROIData.m_RectROIDrawer != nullptr && base_plugin->m_ROIData.m_RectROIDrawer->IsDisplay())
			{
				base_plugin->m_ROIData.m_RectROIDrawer->Normalize();
			}
			base_plugin->FinishCreatingMeasure();
		}
		AmeMeasureDrawObject* draw = base_plugin->m_pEngine->GetCurrentMeasure( false );
		if ( draw != nullptr )
		{
			switch ( draw->GetType() )
			{
				case AME_MEASURE_LINE:
				case AME_MEASURE_PROJ_LINE:
				case AME_MEASURE_TTTG:
				case AME_MEASURE_CUBE:
				case AME_MEASURE_ELLIPSE:
				case AME_MEASURE_POINT:
				case AME_MEASURE_BOX:
				case AME_MEASURE_SPHERE:
					// 作成モードを抜ける
					base_plugin->m_bNowCreating = false;
					break;
				case AME_MEASURE_POLYLINE:
				case AME_MEASURE_CURVE:
				case AME_MEASURE_PROJ_ANGLE:
				case AME_MEASURE_ANGLE:
				case AME_MEASURE_TWO_LINE_ANGLE:
				case AME_MEASURE_ROI:
					// 何もしない
					base_plugin->UpdateInterface();
					return 1;
				case AME_MEASURE_FREEHAND:
				{
					AmeMeasureFreehand* roi = static_cast<AmeMeasureFreehand*>(draw);
					AmeFloat3D pos;
					bool valid = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
					if ( valid && IsPlaneViewer( viewer ) && base_plugin->m_pEngine->m_UserParams.m_bUseLiveWire )
					{
						// 作成開始したビューアかどうかチェック
						if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
						{
							// LiveWire処理の開始
							if ( base_plugin->BeginLiveWire( attr, pos.ToInt() ) )
							{
								base_plugin->UpdateInterface();
								return 1;
							}
						}
						else
						{
							// 違っていれば何もしない
							base_plugin->UpdateInterface();
							return 1;
						}
					}
					else if ( valid )
					{
						// フリーハンド作成の確定
						base_plugin->m_bNowCreating = false;
						roi->AdjustGravity( viewer );
						viewer->Update();
					}
					else if ( !valid )
					{
						base_plugin->FinishCreatingMeasure();
						base_plugin->DeleteSelectedMeasure();
					}
				}
				break;
			}
		}
		base_plugin->UpdateInterface();
	}

	if ( base_plugin != nullptr && base_plugin->m_iGrab != -1 )
	{
		base_plugin->m_iGrab = -1;
		return 1;
	}

	return 0;
}


//モーション中
long AmeMeasurePlugInGUI::onMotion( AmeImageViewer* viewer, PnwEventArgs* event, void* own_plugin )
{
	AmeViewerAttr* attr = viewer->GetViewerAttr();
	if ( attr == nullptr )
	{
		return 0;
	}

	AmeMeasurePlugInGUI* base_plugin = (AmeMeasurePlugInGUI*)own_plugin;

	base_plugin->m_pnwCurrentCursor = nullptr;

	AmeFloat3D pos;
	bool valid = false;
	bool b_changed_pos = false;
	if ( (viewer->GetViewerType() == AME_VIEWER_VOLUME) || (viewer->GetViewerType() == AME_VIEWER_MULTI_3D) )
	{
		if ( base_plugin->m_bNowCreating )
		{
			AmeMeasureDrawObject* draw = base_plugin->m_pEngine->GetCurrentMeasure( false );
			if ( draw != nullptr )
			{
				switch ( draw->GetType() )
				{
					case AME_MEASURE_LINE:
					case AME_MEASURE_POLYLINE:
					case AME_MEASURE_CURVE:
					case AME_MEASURE_ANGLE:
					case AME_MEASURE_POINT:
					{
						valid = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos, AME_CALCULATION_TARGET_1, -1, -1, AmeImageEngineBase::AdditionalInfo( 0.999999f, 0.0f ) );
						b_changed_pos = true;
					}
					break;
					default:
						break;
				}
			}
		}
	}

	if ( !b_changed_pos )
	{
		valid = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
	}

	bool inside = IsInsideViewer( viewer, event );

	// コピー中の計測のゴースト移動
	if ( base_plugin->m_bNowCopying )
	{
		// 作成可能なビューアかチェック
		bool validScale;
		if ( !IsMeasureCreatable( base_plugin->m_pCopiedMeasure->GetType(), viewer, validScale ) )
		{
			base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
			return 1;
		}
		if ( viewer->GetViewerType() == AME_VIEWER_CPR )
		{
			if ( !base_plugin->m_CopiedValidCPR )
			{
				base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
				return 1;
			}

			AmeFloat2D coord;
			if ( !viewer->DisplayToPath( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), coord ) )
			{
				base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
				return 1;
			}
		}

		bool viewer_changed = false;

		if ( base_plugin->m_pCopiedMeasure->GetType() == AME_MEASURE_TWO_LINE_ANGLE )
		{
			if ( viewer->GetViewerType() == AME_VIEWER_VOLUME && attr->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE )
			{
				valid = DisplayToVoxelForRaysum( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
			}
		}

		if ( base_plugin->m_pCopiedMeasure->GetType() == AME_MEASURE_PROJ_ANGLE || base_plugin->m_pCopiedMeasure->GetType() == AME_MEASURE_PROJ_LINE )
		{
			if ( (viewer->GetViewerType() == AME_VIEWER_VOLUME || viewer->GetViewerType() == AME_VIEWER_MULTI_3D || viewer->GetViewerType() == AME_VIEWER_SURFACE) && viewer->GetViewerAttr()->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE )
			{
				valid = true;
			}
		}

		if ( valid )
		{
			if ( base_plugin->m_pCopiedMeasure->m_ViewerIdentity != attr->GetViewerAttrIDString() )
			{
				viewer_changed = true;
			}

			base_plugin->m_pCopiedMeasure->SetImageAttr( attr->GetImageAttr() );
			base_plugin->m_pCopiedMeasure->m_iOrgViewerType = viewer->GetViewerType();
			base_plugin->m_pCopiedMeasure->m_FrameSerialID = viewer->GetViewerFrame()->GetSerialID();
			base_plugin->m_pCopiedMeasure->m_ViewerIdentity = attr->GetViewerAttrIDString();
			base_plugin->m_pCopiedMeasure->m_ImageAttrIDString = attr->GetImageAttr()->GetImageAttrIDString();
			if ( viewer->GetOverlayViewerAttr() != nullptr &&
			   (viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
				viewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
			{
				base_plugin->m_pCopiedMeasure->m_OverlayImageIdentity = viewer->GetOverlayViewerAttr()->GetImageAttr()->GetImageAttrIDString();
			}
			else
			{
				base_plugin->m_pCopiedMeasure->m_OverlayImageIdentity = L"";
			}

			AmeFloatMatrix matrix = attr->m_RotationMatrix;
			AmeFloat3D scale = attr->GetImageAttr()->GetVoxelSpacing();

			switch ( base_plugin->m_pCopiedMeasure->GetType() )
			{
				case AME_MEASURE_LINE:
				{
					AmeMeasureLine* line = static_cast<AmeMeasureLine*>(base_plugin->m_pCopiedMeasure);
					line->m_bValidCPR = false;
					if ( viewer->GetViewerType() == AME_VIEWER_CPR )
					{
						AmeFloat2D center = (base_plugin->m_CopiedCPRPoint[0] + base_plugin->m_CopiedCPRPoint[1]) / 2;
						AmeFloat2D coord;
						if ( viewer->DisplayToPath( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), coord ) )
						{
							AmeFloat2D p1 = base_plugin->m_CopiedCPRPoint[0] + (coord - center);
							AmeFloat2D p2 = base_plugin->m_CopiedCPRPoint[1] + (coord - center);
							if ( base_plugin->m_CopiedCPRPoint[0].X == 0.0f &&
								base_plugin->m_CopiedCPRPoint[1].X == 0.0f )
							{
								// ストレートCPRの方向制限
								p1.X = 0.0f;
								p2.X = 0.0f;
							}
							if ( p1.Y >= 0 && p1.Y <= 1 && p2.Y >= 0 && p2.Y <= 1 )
							{
								viewer->DisplayToVoxel( p1, line->m_vecPoint[0] );
								viewer->DisplayToVoxel( p2, line->m_vecPoint[1] );
								line->m_vecCPRPoint[0] = p1;
								line->m_vecCPRPoint[1] = p2;
								line->m_bValidCPR = true;
								line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
								line->ConstructCPRPath( viewer );
							}
						}
					}
					else
					{
						line->m_vecPoint[0] = (matrix * base_plugin->m_CopiedPoints[0]) / scale + pos;
						line->m_vecPoint[1] = (matrix * base_plugin->m_CopiedPoints[1]) / scale + pos;
					}
				}
				break;
				case AME_MEASURE_PROJ_LINE:
				{
					AmeFloat2D _pos( event->ClientPosition.X, event->ClientPosition.Y );
					AmeFloat2D _center = (base_plugin->m_CopiedProjPoints[0] + base_plugin->m_CopiedProjPoints[1]) * 0.5f;
					AmeMeasureProjectedLine* line = static_cast<AmeMeasureProjectedLine*>(base_plugin->m_pCopiedMeasure);
					line->m_vecProjectedPoint[0] = base_plugin->m_CopiedProjPoints[0] + (_pos - _center);
					line->m_vecProjectedPoint[1] = base_plugin->m_CopiedProjPoints[1] + (_pos - _center);
					line->StorePoints( attr );
				}
				break;
				case AME_MEASURE_TTTG:
				{
					AmeMeasureTTTG* tttg = static_cast<AmeMeasureTTTG*>(base_plugin->m_pCopiedMeasure);
					for ( int i = 0; i < AmeMeasureTTTG::CTRL_PT_NUM; i++ )
						tttg->m_vecPoint[i] = (matrix * base_plugin->m_CopiedPoints[i]) / scale + pos;
				}
				break;
				case AME_MEASURE_ANGLE:
				{
					AmeMeasureAngle* angle = static_cast<AmeMeasureAngle*>(base_plugin->m_pCopiedMeasure);
					for ( size_t i = 0; i < angle->m_vecPoint.size(); i++ )
					{
						angle->m_vecPoint[i] = (matrix * base_plugin->m_CopiedPoints[i]) / scale + pos;
					}
				}
				break;
				case AME_MEASURE_PROJ_ANGLE:
				{
					AmeFloat2D _pos( event->ClientPosition.X, event->ClientPosition.Y );
					AmeFloat2D _center = (base_plugin->m_CopiedProjPoints[0] + base_plugin->m_CopiedProjPoints[1] + base_plugin->m_CopiedProjPoints[2]) / 3.0f;
					AmeMeasureProjAngle* angle = static_cast<AmeMeasureProjAngle*>(base_plugin->m_pCopiedMeasure);
					for ( size_t i = 0; i < angle->m_vecProjectedPoint_Stored.size(); i++ )
					{
						angle->m_vecProjectedPoint[i] = base_plugin->m_CopiedProjPoints[i] + (_pos - _center);
						angle->StorePoints( attr );
					}
				}
				break;
				case AME_MEASURE_TWO_LINE_ANGLE:
				{
					AmeMeasureTwoLineAngle* angle = static_cast<AmeMeasureTwoLineAngle*>(base_plugin->m_pCopiedMeasure);
					for ( size_t i = 0; i < angle->m_vecPoint.size(); i++ )
					{
						angle->m_vecPoint[i] = (matrix * base_plugin->m_CopiedPoints[i]) / scale + pos;
					}
				}
				break;
				case AME_MEASURE_POLYLINE:
				{
					AmeMeasurePolyLine* poly = static_cast<AmeMeasurePolyLine*>(base_plugin->m_pCopiedMeasure);
					for ( size_t i = 0; i < poly->m_vecPoint.size(); i++ )
					{
						poly->m_vecPoint[i] = (matrix * base_plugin->m_CopiedPoints[i]) / scale + pos;
					}
				}
				break;
				case AME_MEASURE_CURVE:
				{
					AmeMeasureCurve* curve = static_cast<AmeMeasureCurve*>(base_plugin->m_pCopiedMeasure);
					for ( size_t i = 0; i < curve->m_vecPoint.size(); i++ )
					{
						curve->m_vecPoint[i] = (matrix * base_plugin->m_CopiedPoints[i]) / scale + pos;
					}
				}
				break;
				case AME_MEASURE_POINT:
				{
					AmeMeasurePoint* point = static_cast<AmeMeasurePoint*>(base_plugin->m_pCopiedMeasure);
					//必ずボクセル中心
					point->m_Point = pos.ToRoundInt().ToFloat();
					point->m_bValidCPR = false;
					if ( viewer->GetViewerType() == AME_VIEWER_CPR )
					{
						point->m_bValidCPR = viewer->DisplayToPath( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), point->m_CPRPoint );
						point->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
					}
				}
				break;
				case AME_MEASURE_CUBE:
				{
					AmeMeasureCube* cube = static_cast<AmeMeasureCube*>(base_plugin->m_pCopiedMeasure);
					cube->m_BasePoint = pos;

					AmeFloat3D size = base_plugin->m_CopiedSize / attr->GetImageAttr()->GetMinVoxelSpacing();
					cube->m_Size = AmeFloat2D( size.X, size.Y );
				}
				break;
				case AME_MEASURE_ELLIPSE:
				{
					AmeMeasureEllipse* elli = static_cast<AmeMeasureEllipse*>(base_plugin->m_pCopiedMeasure);
					elli->m_BasePoint = pos;

					AmeFloat3D size = base_plugin->m_CopiedSize / attr->GetImageAttr()->GetMinVoxelSpacing();
					elli->m_Radius = AmeFloat2D( size.X, size.Y );
				}
				break;
				case AME_MEASURE_ROI:
				case AME_MEASURE_FREEHAND:
				{
					AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(base_plugin->m_pCopiedMeasure);
					roi->m_BasePoint = pos;
					for ( size_t i = 0; i < roi->m_vecPoints.size(); i++ )
					{
						roi->m_vecPoints[i].X = base_plugin->m_CopiedPoints[i].X / attr->GetImageAttr()->GetMinVoxelSpacing();
						roi->m_vecPoints[i].Y = base_plugin->m_CopiedPoints[i].Y / attr->GetImageAttr()->GetMinVoxelSpacing();
					}
				}
				break;
				case AME_MEASURE_BOX:
				{
					AmeMeasureBox* cube = static_cast<AmeMeasureBox*>(base_plugin->m_pCopiedMeasure);
					cube->m_BasePoint = pos;
					cube->m_Size = base_plugin->m_CopiedSize / attr->GetImageAttr()->GetMinVoxelSpacing();
				}
				break;
				case AME_MEASURE_SPHERE:
				{
					AmeMeasureSphere* sphere = static_cast<AmeMeasureSphere*>(base_plugin->m_pCopiedMeasure);
					sphere->m_BasePoint = pos;
					sphere->m_fRadius = base_plugin->m_CopiedSize.X / attr->GetImageAttr()->GetMinVoxelSpacing();
				}
				break;
			}
		}

		base_plugin->m_iCopiedImageAttrID = attr->GetImageAttr()->GetImageAttrID();
		base_plugin->m_pnwCurrentCursor = PnwApplication::GetDefaultCursor(); // ※明示的に標準カーソルを指定
		if ( viewer_changed )
		{
			base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
		}
		else
		{
			viewer->Update();
		}
		return 1;
	}

	// 作成中の計測の処理
	if ( base_plugin->m_bNowCreating )
	{
		AmeMeasureDrawObject* draw = base_plugin->m_pEngine->GetCurrentMeasure( false );
		if ( draw != nullptr )
		{
			switch ( draw->GetType() )
			{
				case AME_MEASURE_PROJ_ANGLE:
					if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
					{
						if ( inside )
						{
							// 点を移動
							AmeMeasureProjAngle* angle = static_cast<AmeMeasureProjAngle*>(draw);
							if ( angle->m_vecProjectedPoint.size() != 0 )
							{
								AmeFloat2D _pos = AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y );
								angle->m_vecProjectedPoint[angle->m_vecProjectedPoint.size() - 1] = _pos;
								angle->StorePoints( base_plugin->GetTaskManager()->GetCurrentViewerAttr() );
								viewer->Update();
							}
						}
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
					}
					else
					{
						// 作成開始したビューア以外では何もできない
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
					}
					return 1;
				case AME_MEASURE_PROJ_LINE:
					if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
					{
						if ( inside )
						{
							// 点を移動
							AmeMeasureProjectedLine* line = static_cast<AmeMeasureProjectedLine*>(draw);
							if ( line->m_vecProjectedPoint.size() != 0 )
							{
								AmeFloat2D _pos = AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y );
								line->m_vecProjectedPoint[line->m_vecProjectedPoint.size() - 1] = _pos;
								line->StorePoints( base_plugin->GetTaskManager()->GetCurrentViewerAttr() );
								viewer->Update();
							}
						}
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
					}
					else
					{
						// 作成開始したビューア以外では何もできない
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
					}
					return 1;

				case AME_MEASURE_POLYLINE:
				case AME_MEASURE_CURVE:
				case AME_MEASURE_ANGLE:
				case AME_MEASURE_TWO_LINE_ANGLE:
					if ( base_plugin->m_iCreatingImageAttrID == attr->GetImageAttr()->GetImageAttrID() )
					{
						if ( draw->GetType() == AME_MEASURE_TWO_LINE_ANGLE && viewer->GetViewerType() == AME_VIEWER_VOLUME && attr->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE )
						{
							valid = DisplayToVoxelForRaysum( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
						}
						if ( draw->GetType() == AME_MEASURE_PROJ_ANGLE && viewer->GetViewerType() == AME_VIEWER_VOLUME && attr->GetRenderingInfo()->GetRenderingMode() == AME_VR_RENDERING_RAYSUM )
						{
							valid = DisplayToVoxelForRaysum( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
						}
						if ( (valid || IsPlaneViewer( viewer )) && inside )
						{
							// 点を移動
							AmeMeasurePointArrayBase* line = static_cast<AmeMeasurePointArrayBase*>(draw);
							if ( line->m_vecPoint.size() != 0 )
							{
								line->m_vecPoint[line->m_vecPoint.size() - 1] = pos;
								viewer->Update();
							}
						}
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
					}
					else
					{
						// 作成開始したビューア以外では何もできない
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
					}
					//左ボタンドラッグ時はデフォルトに飛ばさない
					if ( event->PressedKeyState & KEYSTATE_LEFTBUTTON )
					{
						return 1;
					}
					else
					{
						return 0;
					}

				case AME_MEASURE_ROI:
					if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
					{
						if ( (valid || IsPlaneViewer( viewer )) && inside )
						{
							// 点を移動
							AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(draw);
							if ( roi->m_vecPoints.size() != 0 )
							{
								AmeFloat3D axisX = viewer->GetViewerAxisX();
								AmeFloat3D axisY = viewer->GetViewerAxisY();
								AmeFloat3D disp_vec = ConvertVoxelToIso( pos - roi->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );
								roi->m_vecPoints[roi->m_vecPoints.size() - 1].X = axisX.InnerProduct( disp_vec );
								roi->m_vecPoints[roi->m_vecPoints.size() - 1].Y = axisY.InnerProduct( disp_vec );
								roi->AdjustGravity( viewer ); // 重心の調整

								viewer->Update();
							}
						}
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
					}
					else
					{
						// 作成開始したビューア以外では何もできない
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
					}
					return 1;

				case AME_MEASURE_FREEHAND:
					if ( base_plugin->m_iCreatingViewerAttrID == attr->GetViewerAttrID() )
					{
						if ( event->PressedKeyState & KEYSTATE_LEFTBUTTON && valid )
						{
							// 点の追加
							AmeMeasureFreehand* roi = static_cast<AmeMeasureFreehand*>(draw);
							AmeFloat3D offset = ConvertVoxelToIso( pos - roi->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );

							AmeFloat2D pt = AmeFloat2D(
								offset.InnerProduct( viewer->GetViewerAxisX() ),
								offset.InnerProduct( viewer->GetViewerAxisY() ) );
							if ( (pt - roi->m_vecPoints.back()).Norm() > (double)AppAttr->m_WidgetSize.AME_POINT_GRAB_MARGIN )
							{
								roi->FillControlPoints( roi->m_vecPoints.back(), pt );
							}
							roi->m_vecPoints.push_back( pt );
							if ( base_plugin->m_LiveWire.Counter.size() > 0 )
							{
								base_plugin->m_LiveWire.Counter[base_plugin->m_LiveWire.Counter.size() - 1]++;
								base_plugin->m_LiveWire.SeedList[base_plugin->m_LiveWire.SeedList.size() - 1] = pos.ToInt();
							}
							viewer->Update();
						}
						else
						{
							// LiveWireの経路更新
							if ( valid && IsPlaneViewer( viewer ) )
							{
								AmeSize3D endpos = pos.ToSize();
								if ( base_plugin->m_LiveWire.ViewerAttrID == attr->GetViewerAttrID() )
								{
									if ( viewer->GetViewerType() == AME_VIEWER_OBLIQUE )
									{
										base_plugin->m_pObliqueLiveWire->TraseLiveWire( pos );

										std::vector<AmeFloat3D> path = base_plugin->m_pObliqueLiveWire->GetTracePath();
										if ( path.size() != 0 )
										{
											base_plugin->m_LiveWire.ObliquePath = path;
										}
									}
									else
									{
										if ( base_plugin->m_LiveWire.Proc.IsInsideImage( endpos ) )
										{
											std::vector<AmeSize2D> ret_pts;
											base_plugin->m_LiveWire.Proc.GetPath( endpos, ret_pts, true );

											base_plugin->m_LiveWire.Path.clear();
											for ( size_t n = 0; n < ret_pts.size(); n++ )
											{
												base_plugin->m_LiveWire.Path.push_back( ret_pts[n].ToInt() );
											}
										}
									}
									viewer->Update();
								}
							}
						}
						if ( valid )
						{
							base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
						}
						else
						{
							base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
						}
					}
					else
					{
						// 作成開始したビューア以外では何もできない
						base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
					}
					return 1;

				default:
					break;
			}
		}
	}


	// ドラッグ中の点移動
	if ( (event->PressedKeyState & KEYSTATE_LEFTBUTTON) && (base_plugin->m_iGrab != -1) )
	{
		if ( base_plugin->m_iGrab >= (int)base_plugin->m_pEngine->m_AllObjects.size() )
		{
			return 0;
		}

		AmeMeasureDrawObject* draw = base_plugin->m_pEngine->m_AllObjects[base_plugin->m_iGrab];
		switch ( draw->GetType() )
		{
			case AME_MEASURE_LINE:
			{
				AmeMeasureLine* line = static_cast<AmeMeasureLine*>(draw);
				if ( viewer->GetViewerType() == AME_VIEWER_CPR )
				{
					if ( event->ClientPosition.X >= 0 && event->ClientPosition.X < viewer->GetWidth() &&
						event->ClientPosition.Y >= 0 && event->ClientPosition.Y < viewer->GetHeight() )
					{
						AmeFloat3D pos1, pos2;
						AmeFloat2D path1, path2;
						switch ( viewer->GetViewerAttr()->m_CPRImageType )
						{
							case AME_CPR_STRAIGHT_V:
							case AME_CPR_STRETCH_V:
							case AME_CPR_FIXED_AXIS_V:
								if ( abs( event->ClientPosition.X - event->ClickClientPosition.X ) > abs( event->ClientPosition.Y - event->ClickClientPosition.Y ) )
								{
									if ( viewer->DisplayToPath( AmeFloat2D( event->ClickClientPosition.X, event->ClickClientPosition.Y ), path1 ) &&
										viewer->DisplayToPath( AmeFloat2D( event->ClientPosition.X, event->ClickClientPosition.Y ), path2 ) &&
										viewer->DisplayToVoxel( AmeFloat2D( event->ClickClientPosition.X, event->ClickClientPosition.Y ), pos1 ) &&
										viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClickClientPosition.Y ), pos2 ) )
									{
										line->m_vecPoint[0] = pos1;
										line->m_vecPoint[1] = pos2;
										line->m_vecCPRPoint[0] = path1;
										line->m_vecCPRPoint[1] = path2;
										line->m_bValidCPR = true;
										line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
									}
								}
								else
								{
									if ( viewer->DisplayToPath( AmeFloat2D( viewer->GetWidth() / 2, event->ClickClientPosition.Y ), path1 ) &&
										viewer->DisplayToPath( AmeFloat2D( viewer->GetWidth() / 2, event->ClientPosition.Y ), path2 ) &&
										viewer->DisplayToVoxel( AmeFloat2D( viewer->GetWidth() / 2, event->ClickClientPosition.Y ), pos1 ) &&
										viewer->DisplayToVoxel( AmeFloat2D( viewer->GetWidth() / 2, event->ClientPosition.Y ), pos2 ) )
									{
										line->m_vecPoint[0] = pos1;
										line->m_vecPoint[1] = pos2;
										line->m_vecCPRPoint[0] = AmeFloat2D( 0.0f, path1.Y );
										line->m_vecCPRPoint[1] = AmeFloat2D( 0.0f, path2.Y );
										line->m_bValidCPR = true;
										line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
									}
								}
								break;

							default:
								if ( abs( event->ClientPosition.X - event->ClickClientPosition.X ) < abs( event->ClientPosition.Y - event->ClickClientPosition.Y ) )
								{
									if ( viewer->DisplayToPath( AmeFloat2D( event->ClickClientPosition.X, event->ClickClientPosition.Y ), path1 ) &&
										viewer->DisplayToPath( AmeFloat2D( event->ClickClientPosition.X, event->ClientPosition.Y ), path2 ) &&
										viewer->DisplayToVoxel( AmeFloat2D( event->ClickClientPosition.X, event->ClickClientPosition.Y ), pos1 ) &&
										viewer->DisplayToVoxel( AmeFloat2D( event->ClickClientPosition.X, event->ClientPosition.Y ), pos2 ) )
									{
										line->m_vecPoint[0] = pos1;
										line->m_vecPoint[1] = pos2;
										line->m_vecCPRPoint[0] = path1;
										line->m_vecCPRPoint[1] = path2;
										line->m_bValidCPR = true;
										line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
									}
								}
								else
								{
									if ( viewer->DisplayToPath( AmeFloat2D( event->ClickClientPosition.X, viewer->GetHeight() / 2 ), path1 ) &&
										viewer->DisplayToPath( AmeFloat2D( event->ClientPosition.X, viewer->GetHeight() / 2 ), path2 ) &&
										viewer->DisplayToVoxel( AmeFloat2D( event->ClickClientPosition.X, viewer->GetHeight() / 2 ), pos1 ) &&
										viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, viewer->GetHeight() / 2 ), pos2 ) )
									{
										line->m_vecPoint[0] = pos1;
										line->m_vecPoint[1] = pos2;
										line->m_vecCPRPoint[0] = AmeFloat2D( 0.0f, path1.Y );
										line->m_vecCPRPoint[1] = AmeFloat2D( 0.0f, path2.Y );
										line->m_bValidCPR = true;
										line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
									}
								}
								break;
						}

						if ( line->m_bValidCPR )
						{
							line->ConstructCPRPath( viewer );

							if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
							{
								base_plugin->ChangeCurrentPosition( attr, pos );
							}
						}
					}
				}
				else if ( (valid || IsPlaneViewer( viewer )) && inside )
				{
					auto clientPosition = AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y );
					if ( viewer->GetViewerType() != AME_VIEWER_CPR )
					{
						if ( event->PressedKeyState & KEYSTATE_SHIFT )
						{ // 画面に直交するよう補正する
							AmeFloat2D referencePosition;
							if ( viewer->VoxelToDisplay( line->m_vecPoint[0], referencePosition ) )
							{
								auto direction = clientPosition - referencePosition;
								if ( fabs( direction.X ) > fabs( direction.Y ) )
								{
									clientPosition.Y = referencePosition.Y;
								}
								else
								{
									clientPosition.X = referencePosition.X;
								}
								viewer->DisplayToVoxel( clientPosition, pos );
							}
						}
						line->m_vecPoint[1] = pos;
					}
					else
					{
						line->m_vecPoint[1] = pos;
						line->m_bValidCPR = viewer->DisplayToPath( clientPosition, line->m_vecCPRPoint[1] );
						line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();
						line->ConstructCPRPath( viewer );
					}
					if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
					{
						base_plugin->ChangeCurrentPosition( attr, pos );
					}
				}
			}
			break;
			case AME_MEASURE_PROJ_LINE:
			{
				AmeMeasureProjectedLine* line = static_cast<AmeMeasureProjectedLine*>(draw);
				if ( (valid || IsPlaneViewer( viewer )) && inside )
				{
					line->m_vecProjectedPoint[1] = event->ClientPosition.ToFloat();
					line->StorePoints( attr );

				}
			}
			break;
			case AME_MEASURE_TTTG:
			{
				AmeMeasureTTTG* tttg = static_cast<AmeMeasureTTTG*>(draw);
				if ( (valid || IsPlaneViewer( viewer )) && inside )
				{
					tttg->m_vecPoint[AmeMeasureTTTG::CTRL_PT_END] = pos;
					tttg->UpdateControlPoints( viewer );

					if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
					{
						base_plugin->ChangeCurrentPosition( attr, pos );
					}
				}
			}
			break;
			case AME_MEASURE_CUBE:
			{
				AmeMeasureCube* cube = static_cast<AmeMeasureCube*>(draw);
				AmeFloat3D offset = ConvertVoxelToIso( pos - cube->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );
				cube->m_Size.X = offset.InnerProduct( viewer->GetViewerAxisX() );
				cube->m_Size.Y = offset.InnerProduct( viewer->GetViewerAxisY() );

			}
			break;
			case AME_MEASURE_ELLIPSE:
			{
				AmeMeasureEllipse* ellipse = static_cast<AmeMeasureEllipse*>(draw);
				AmeFloat3D offset = ConvertVoxelToIso( pos - ellipse->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );
				ellipse->m_Radius.X = fabs( offset.InnerProduct( viewer->GetViewerAxisX() ) );
				ellipse->m_Radius.Y = fabs( offset.InnerProduct( viewer->GetViewerAxisY() ) );
			}
			break;
			case AME_MEASURE_ROI:
			{
				//AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(draw);
			}
			break;
			case AME_MEASURE_FREEHAND:
			{
				AmeMeasureFreehand* roi = static_cast<AmeMeasureFreehand*>(draw);
				AmeFloat3D offset = ConvertVoxelToIso( pos - roi->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );
				roi->m_vecPoints.push_back( AmeFloat2D(
					offset.InnerProduct( viewer->GetViewerAxisX() ),
					offset.InnerProduct( viewer->GetViewerAxisY() ) ) );
			}
			break;
			case AME_MEASURE_BOX:
			{
				AmeMeasureBox* box = static_cast<AmeMeasureBox*>(draw);
				AmeFloat3D offset = ConvertVoxelToIso( pos - box->m_BasePoint, attr->GetImageAttr()->GetVoxelSpacing() );
				offset.X = fabs( offset.X );
				offset.Y = fabs( offset.Y );
				offset.Z = fabs( offset.Z );
				if ( event->PressedKeyState & AmeGetControlKeyMask() )
				{
					box->m_Size.X = box->m_Size.Y = box->m_Size.Z = max( offset.X, max( offset.Y, offset.Z ) ) * 2.0f;
				}
				else
				{
					box->m_Size.X = max( offset.X, min( offset.Y, offset.Z ) ) * 2.0f;
					box->m_Size.Y = max( offset.Y, min( offset.Z, offset.X ) ) * 2.0f;
					box->m_Size.Z = max( offset.Z, min( offset.X, offset.Y ) ) * 2.0f;
				}
			}
			break;
			case AME_MEASURE_SPHERE:
			{
				AmeMeasureSphere* sphere = static_cast<AmeMeasureSphere*>(draw);

				AmeFloat2D projected_rad[2];
				viewer->VoxelToDisplay( sphere->m_BasePoint, projected_rad[0] );
				projected_rad[1] = AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y );
				sphere->m_fRadius = (projected_rad[0] - projected_rad[1]).Norm() / viewer->GetDrawScale();
			}
			break;
			case AME_MEASURE_POINT:
			{
				AmeMeasurePoint* line = static_cast<AmeMeasurePoint*>(draw);
				if ( (valid || IsPlaneViewer( viewer )) && inside )
				{
					//必ずボクセル中心
					line->m_Point = pos.ToRoundInt().ToFloat();
					if ( viewer->GetViewerType() == AME_VIEWER_CPR )
					{
						line->m_bValidCPR = viewer->DisplayToPath( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), line->m_CPRPoint );
						line->m_CPRPathID = viewer->GetViewerAttr()->GetCPRPathID();

						AmePath* path = viewer->GetViewerAttr()->GetCPRPathObject();
						if ( path != nullptr )
						{
							line->m_iCPRUpdateCounter = path->GetUpdatePointsCounter();
						}
					}
					if ( valid && base_plugin->m_pEngine->m_UserParams.m_bSyncCurrentPosition )
					{
						base_plugin->ChangeCurrentPosition( attr, pos );
					}
				}
			}
			break;
		}

		base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
		base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

		return 1;
	}
	if ( (event->PressedKeyState & KEYSTATE_LEFTBUTTON) && (base_plugin->GetCurrentType() == AME_MEASURE_CLOSEST || base_plugin->GetCurrentType() == AME_MEASURE_DIAMETER) && base_plugin->m_bNowCreating )
	{
		if ( base_plugin->m_ROIData.m_RectROIDrawer != nullptr && base_plugin->m_ROIData.m_RectROIDrawer->IsDisplay() )
		{
			auto& rectROI = *base_plugin->m_ROIData.m_RectROIDrawer;
			rectROI.SetRectMin(event->ClientPosition);
			base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
			base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

			return 1;
		}
		if ( base_plugin->m_ROIData.m_BoxROIDrawer != nullptr && base_plugin->m_ROIData.m_BoxROIDrawer->IsDisplay() )
		{
			auto& boxROI = *base_plugin->m_ROIData.m_BoxROIDrawer;
			// 視線方向に中心点とドラッグ点を投影しサイズを計算、設定する(完全な正方形である必要はないので大体で決める)
			AmeFloat3D center = boxROI.GetBoxCenter();
			AmeFloat3D gazeDirection = viewer->GetViewerAxisZ();
			AmeFloat3D direction = pos - center;
			AmeFloat3D spacing = attr->GetImageAttr()->GetVoxelSpacing();
			// 視線方向投影するため等方座標系に変換
			direction = ConvertVoxelToIso(direction, spacing);
			direction -= direction.InnerProduct(gazeDirection) * gazeDirection;
			float norm = direction.Norm();
			// ボックスのサイズを更新する。
			boxROI.SetBox(center, AmeFloat3D(norm / spacing[0], norm / spacing[1], norm / spacing[2]));
			base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
			base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

			return 1;
		}
	}
	// カーソルの設定
	if ( !(event->PressedKeyState & KEYSTATE_LEFTBUTTON) )
	{
		if ( base_plugin->m_CurrentType != AME_MEASURE_NONE )
		{
			// 作成モード
			bool validScale;
			if ( IsMeasureCreatable( base_plugin->m_CurrentType, viewer, validScale ) )
			{
				bool valid1 = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
				bool valid2 = false;
				if ( viewer->GetOverlayViewerAttr() != nullptr && viewer->GetViewerType() != AME_VIEWER_VOLUME )
				{
					AmeFloat3D pos2;
					valid2 = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos2, AME_CALCULATION_TARGET_2 );
				}
				if ( base_plugin->m_CurrentType == AME_MEASURE_TWO_LINE_ANGLE && viewer->GetViewerType() == AME_VIEWER_VOLUME && attr->GetRenderingInfo()->GetRenderingMode() != AME_VR_RENDERING_VE )
				{
					valid1 = DisplayToVoxelForRaysum( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
				}
				if ( base_plugin->m_CurrentType == AME_MEASURE_PROJ_ANGLE && viewer->GetViewerType() == AME_VIEWER_VOLUME && attr->GetRenderingInfo()->GetRenderingMode() == AME_VR_RENDERING_RAYSUM )
				{
					valid1 = DisplayToVoxelForRaysum( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
				}
				if ( base_plugin->m_CurrentType == AME_MEASURE_PROJ_LINE && viewer->GetViewerType() == AME_VIEWER_VOLUME && attr->GetRenderingInfo()->GetRenderingMode() == AME_VR_RENDERING_RAYSUM )
				{
					valid1 = DisplayToVoxelForRaysum( viewer, AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );
				}

				if ( valid1 || valid2 )
				{
					base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_CROSSHAIR );
				}
				else
				{
					base_plugin->m_pnwCurrentCursor = nullptr;
				}
			}
			else
			{
				base_plugin->m_pnwCurrentCursor = AppAttr->GetAppCursor( AME_CURSOR_PROHIBIT );
			}
		}
		else
		{
			base_plugin->m_pnwCurrentCursor = nullptr;
		}
	}

	if ( base_plugin->m_bFailedToCreate )
	{
		return 1;
	}

	return 0;
}


// ダブルクリック
long AmeMeasurePlugInGUI::onDoubleClicked( AmeImageViewer* viewer, PnwEventArgs*, void* own_plugin )
{
	AmeViewerAttr* attr = viewer->GetViewerAttr();
	if ( attr == nullptr )
	{
		return 0;
	}

	AmeMeasurePlugInGUI* base_plugin = (AmeMeasurePlugInGUI*)own_plugin;

	AmeMeasureDrawObject* current = base_plugin->m_pEngine->GetCurrentMeasure( false );
	if ( current == nullptr )
	{
		return 0;
	}

	// 作成中の計測を確定する
	if ( base_plugin->m_bNowCreating )
	{
		switch ( current->GetType() )
		{
			case AME_MEASURE_ANGLE:
			{
				AmeMeasureAngle* angle = static_cast<AmeMeasureAngle*>(current);
				int size = (int)angle->m_vecPoint.size();
				if ( size < 3 || (angle->m_vecPoint[size - 1] - angle->m_vecPoint[size - 2]).Norm() < 1e-5 )
				{
					// ３点ないときは確定しない
					return 1;
				}
			}
			case AME_MEASURE_PROJ_ANGLE:
			{
				AmeMeasureProjAngle* angle = static_cast<AmeMeasureProjAngle*>(current);
				int size = (int)angle->m_vecPoint.size();
				if ( size < 3 || (angle->m_vecPoint[size - 1] - angle->m_vecPoint[size - 2]).Norm() < 1e-5 )
				{
					// ３点ないときは確定しない
					return 1;
				}
			}
			case AME_MEASURE_TWO_LINE_ANGLE:
			{
				AmeMeasureTwoLineAngle* angle = static_cast<AmeMeasureTwoLineAngle*>(current);
				int size = (int)angle->m_vecPoint.size();
				if ( size < 4/* || (angle->m_vecPoint[size-1]-angle->m_vecPoint[size-2]).Norm() < 1e-5*/ )
				{
					// ４点ないときは確定しない
					return 1;
				}
			}
			case AME_MEASURE_POLYLINE:
			{
				AmeMeasurePolyLine* poly = static_cast<AmeMeasurePolyLine*>(current);
				int size = (int)poly->m_vecPoint.size();
				double norm = (poly->m_vecPoint[size - 1] - poly->m_vecPoint[size - 2]).Norm();
				if ( size < 2 || (size == 2 && norm < 1e-5) )
				{
					//2点ないとき、あっても2点間の距離が近すぎる場合には確定しない
					return 1;
				}
				if ( size > 2 && norm < 1e-5 )
				{
					//確定するとき、直前に制御点が置かれた場所にカーソルがある場合、取り消す。
					poly->m_vecPoint.pop_back();
				}
			}
			break;
			case AME_MEASURE_CURVE:
			{
				AmeMeasureCurve* curve = static_cast<AmeMeasureCurve*>(current);
				int size = (int)curve->m_vecPoint.size();
				double norm = (curve->m_vecPoint[size - 1] - curve->m_vecPoint[size - 2]).Norm();
				if ( size < 2 || (size == 2 && norm < 1e-5) )
				{
					// 2点ないとき、あっても2点間の距離が近すぎる場合には確定しない
					return 1;
				}
				if ( size > 2 && norm < 1e-5 )
				{
					//確定するとき、直前に制御点が置かれた場所にカーソルがある場合、取り消す。
					curve->m_vecPoint.pop_back();
				}
			}
			break;
			case AME_MEASURE_ROI:
			{
				AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(current);
				int size = (int)roi->m_vecPoints.size();
				double norm = (roi->m_vecPoints[size - 1] - roi->m_vecPoints[size - 2]).Norm();
				if ( size < 3 || (size == 3 && norm < 1e-5) )
				{
					//3点ないとき、あっても直前の2点間の距離が近すぎる場合には確定しない
					return 1;
				}
				if ( size > 3 && norm < 1e-5 )
				{
					//確定するとき、直前に制御点が置かれた位置にカーソルがある場合、取り消す。
					roi->m_vecPoints.pop_back();
					roi->AdjustGravity( viewer );
				}
			}
			break;
			case AME_MEASURE_FREEHAND:
			{
				AmeMeasureFreehand* freehand = static_cast<AmeMeasureFreehand*>(current);
				freehand->FillControlPoints( freehand->m_vecPoints.back(), freehand->m_vecPoints.front() );
			}
			default:
				break;
		}

		base_plugin->FinishCreatingMeasure();

		base_plugin->UpdateInterface();

		base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

		return 1;
	}

	return 0;
}


// キープレス
long AmeMeasurePlugInGUI::onKeyPress( AmeImageViewer* viewer, PnwEventArgs* event, void* own_plugin )
{
	AmeMeasurePlugInGUI* base_plugin = (AmeMeasurePlugInGUI*)own_plugin;
	AmeViewerAttr* attr = viewer->GetViewerAttr();
	if ( attr == NULL )
	{
		return 0;
	}

	if ( !base_plugin->IsCurrentMouseMode() )
	{
		return 0;
	}

	AmeMeasureDrawObject* current = base_plugin->m_pEngine->GetCurrentMeasure( false );

	switch ( event->KeyCharCode )
	{
		case KEY_Delete:
			// カレントの計測削除
			if ( current != NULL && current->IsVisible( viewer ) )
			{
				base_plugin->DeleteSelectedMeasure();
				base_plugin->UpdateInterface();
				base_plugin->UpdateInterfaceForShortcutBar( attr );
				base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );

				return 1;
			}
			break;

		case KEY_Enter:
			if ( (current != NULL) && base_plugin->m_bNowCreating )
			{
				if ( current->GetType() == AME_MEASURE_ANGLE )
				{
					AmeMeasureAngle* angle = static_cast<AmeMeasureAngle*>(current);
					if ( angle->m_vecPoint.size() < 3 )
					{
						// ３点ないときは確定しない
						return 1;
					}
				}
				else if ( current->GetType() == AME_MEASURE_PROJ_ANGLE )
				{
					AmeMeasureProjAngle* angle = static_cast<AmeMeasureProjAngle*>(current);
					if ( angle->m_vecProjectedPoint.size() < 3 )
					{
						// ３点ないときは確定しない
						return 1;
					}
				}
				else if ( current->GetType() == AME_MEASURE_TWO_LINE_ANGLE )
				{
					AmeMeasureTwoLineAngle* angle = static_cast<AmeMeasureTwoLineAngle*>(current);
					if ( angle->m_vecPoint.size() < 4 )
					{
						// ４点ないときは確定しない
						return 1;
					}
				}
				else if ( current->GetType() == AME_MEASURE_POLYLINE )
				{
					AmeMeasurePolyLine* poly = static_cast<AmeMeasurePolyLine*>(current);
					int size = (int)poly->m_vecPoint.size();
					double norm = (poly->m_vecPoint[size - 1] - poly->m_vecPoint[size - 2]).Norm();
					if ( size < 2 || (size == 2 && norm < 1e-5) )
					{
						//2点ないとき、あっても2点間の距離が近すぎる場合には確定しない
						return 1;
					}
					if ( size > 2 && norm < 1e-5 )
					{
						//確定するとき、直前に制御点が置かれた場所にカーソルがある場合、取り消す。
						poly->m_vecPoint.pop_back();
					}
				}
				else if ( current->GetType() == AME_MEASURE_CURVE )
				{
					AmeMeasureCurve* curve = static_cast<AmeMeasureCurve*>(current);
					int size = (int)curve->m_vecPoint.size();
					double norm = (curve->m_vecPoint[size - 1] - curve->m_vecPoint[size - 2]).Norm();
					if ( size < 2 || (size == 2 && norm < 1e-5) )
					{
						// 2点ないとき、あっても2点間の距離が近すぎる場合には確定しない
						return 1;
					}
					if ( size > 2 && norm < 1e-5 )
					{
						// 確定するとき、直前に制御点が置かれた場所にカーソルがある場合、取り消す。
						curve->m_vecPoint.pop_back();
					}
				}
				else if ( current->GetType() == AME_MEASURE_ROI )
				{
					AmeMeasureRoi* roi = static_cast<AmeMeasureRoi*>(current);
					int size = (int)roi->m_vecPoints.size();
					double norm = (roi->m_vecPoints[size - 1] - roi->m_vecPoints[size - 2]).Norm();
					if ( size < 3 || (size == 3 && norm < 1e-5) )
					{
						//3点ないとき、あっても直前の2点間の距離が近すぎる場合には確定しない
						return 1;
					}
					if ( size > 3 && norm < 1e-5 )
					{
						//確定するとき、直前に制御点が置かれた位置にカーソルがある場合、取り消す。
						roi->m_vecPoints.pop_back();
						roi->AdjustGravity( viewer );
					}
				}
				else if ( current->GetType() == AME_MEASURE_FREEHAND )
				{
					AmeMeasureFreehand* freehand = static_cast<AmeMeasureFreehand*>(current);
					AmeFloat3D pos;
					bool valid = false;
					valid = viewer->DisplayToVoxel( AmeFloat2D( event->ClientPosition.X, event->ClientPosition.Y ), pos );

					if ( !valid )
					{
						base_plugin->FinishCreatingMeasure();
						base_plugin->DeleteSelectedMeasure();
					}
					else if ( IsPlaneViewer( viewer ) )
					{
						// LiveWireを確定して点を追加
						base_plugin->FinishFreehandLiveWire( attr, freehand );
						freehand->FillControlPoints( freehand->m_vecPoints.back(), freehand->m_vecPoints.front() );
						base_plugin->FinishCreatingMeasure();
					}
					return 1;
				}

				//作成中の計測を確定させ、表示を更新する
				base_plugin->FinishCreatingMeasure();
				base_plugin->UpdateInterface();
				base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
				return 1;
			}
			if ( base_plugin->m_bNowCopying )
			{
				base_plugin->FinishCopy( viewer );
				base_plugin->GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
				return 1;
			}
			break;
	}

	return 0;
}

long AmeMeasurePlugInGUI::onLeave( AmeImageViewer* viewer, PnwEventArgs*, void* own_plugin )
{
	AmeMeasurePlugInGUI* base_plugin = (AmeMeasurePlugInGUI*)own_plugin;
	AmeViewerAttr* attr = viewer->GetViewerAttr();
	if ( attr == NULL )
	{
		return 0;
	}

	if ( base_plugin->m_pCopiedMeasure != NULL )
	{
		base_plugin->m_pCopiedMeasure->m_iOrgViewerType = AME_VIEWER_NONE;
		base_plugin->m_pCopiedMeasure->m_FrameSerialID = L"";
		base_plugin->m_pCopiedMeasure->m_ViewerIdentity = L"";
		base_plugin->m_pCopiedMeasure->m_OverlayImageIdentity = L"";
		base_plugin->m_pCopiedMeasure->m_ImageAttrIDString = L"";

		viewer->Update();
	}

	return 0;
}

long AmeMeasurePlugInGUI::onCmdForeColor( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwMeasureTextColor == NULL )
	{
		return 1;
	}

	AmeMeasureType _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		//計測の種類から現在の色を取得し、カラーダイアログにセットする
		m_pnwMeasureTextColor->SetColor( m_ColorSetting[_type]->getFgColor() );

		//カラーダイアログの表示
		m_pnwMeasureTextColor->ShowColorDialog();

		//カラーダイアログから色を取得する
		m_ColorSetting[_type]->setFgColor( m_pnwMeasureTextColor->GetColor() );

		UpdateColorSettingOfObj();
	}
	return 1;
}

long AmeMeasurePlugInGUI::onCmdBackColor( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwMeasureBackColor == NULL )
	{
		return 1;
	}

	AmeMeasureType  _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		//計測の種類から現在の色を取得し、カラーダイアログにセットする
		m_pnwMeasureBackColor->SetColor( m_ColorSetting[_type]->getBgColor() );

		//カラーダイアログの表示
		m_pnwMeasureBackColor->ShowColorDialog();

		//カラーダイアログから色を取得する
		m_ColorSetting[_type]->setBgColor( m_pnwMeasureBackColor->GetColor() );

		UpdateColorSettingOfObj();
	}
	return 1;
}

long AmeMeasurePlugInGUI::onCmdCpColor( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwMeasureCpColor == NULL )
	{
		return 1;
	}

	AmeMeasureType  _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		//計測の種類から現在の色を取得し、カラーダイアログにセットする
		m_pnwMeasureCpColor->SetColor( m_ColorSetting[_type]->getCpColor() );

		//カラーダイアログの表示
		m_pnwMeasureCpColor->ShowColorDialog();

		//カラーダイアログから色を取得する
		m_ColorSetting[_type]->setCpColor( m_pnwMeasureCpColor->GetColor() );

		UpdateColorSettingOfObj();
	}
	return 1;
}


long AmeMeasurePlugInGUI::onCmdFontSize( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwTextFontCombo == NULL )
	{
		return 1;
	}
	AmeMeasureType  _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		int font_size = m_pnwTextFontCombo->GetItemData( m_pnwTextFontCombo->GetCurrentItem() );
		m_ColorSetting[_type]->setTextFontRate( font_size );
		UpdateColorSettingOfObj();
	}

	return 1;
}

long AmeMeasurePlugInGUI::onCmdFont( PnwObject*, PnwEventType, PnwEventArgs* )
{
	AmeMeasureType  _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		//フォントダイアログの表示
		PnwFontDialog dialog( GetTaskManager(), AppAttr->GetResStr( "AME_COMMON_FONT" ) );
		dialog.SetFontName( m_ColorSetting[_type]->getTextFontName() );
		dialog.ShowFontSize( false );
		dialog.ShowFontStyle( false );
		if ( dialog.ShowDialog() != DIALOG_RESULT_OK )
		{
			return 1;
		}
		// フォントを再設定
		m_ColorSetting[_type]->setTextFontName( dialog.GetFontName() );
		if ( m_pnwTextFontName != NULL )
		{
			m_pnwTextFontName->SetText( dialog.GetFontName() );
		}
		UpdateColorSettingOfObj();
	}
	return 1;
}

long AmeMeasurePlugInGUI::onCmdTextBackCheck( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwTextBackCheck == NULL )
	{
		return 1;
	}

	AmeMeasureType  _type = GetCurrentType();
	if ( m_pnwMeasureBackColorButton != NULL )
	{
		m_pnwMeasureBackColorButton->SetEnabled( m_pnwTextBackCheck->GetCheck() );
	}
	if ( m_pnwMeasureBackColor != NULL )
	{
		m_pnwMeasureBackColor->SetEnabled( m_pnwTextBackCheck->GetCheck() );
	}

	if ( _type != AME_MEASURE_NONE )
	{
		m_ColorSetting[_type]->setTextBackCheck( m_pnwTextBackCheck->GetCheck() ? true : false );
		UpdateColorSettingOfObj();
	}
	return 1;
}

long AmeMeasurePlugInGUI::onCmdTextFrameCheck( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwTextFrameCheck == NULL )
	{
		return 1;
	}
	AmeMeasureType  _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		m_ColorSetting[_type]->setTextFrameCheck( m_pnwTextFrameCheck->GetCheck() ? true : false );
		UpdateColorSettingOfObj();
	}
	return 1;
}

void AmeMeasurePlugInGUI::ChgStepLength()
{
	int cur = m_pEngine->m_UserParams.m_iScaleDivLength;
	std::vector<AmeMeasureColorSetting*>::iterator it_color = m_ColorSetting.begin();
	for ( ; it_color != m_ColorSetting.end(); ++it_color )
	{
		(*it_color)->setStepLength( m_StepSize[cur] );
		(*it_color)->setStepCheck( cur != 0 );
	}

	//直線と折れ線の全オブジェクトで目盛り間隔を統一する
	std::vector<AmeMeasureDrawObject*>::const_iterator it_obj = m_pEngine->GetAllMeasure().begin();
	for ( ; it_obj != m_pEngine->GetAllMeasure().end(); ++it_obj )
	{
		AmeMeasureType _type = (*it_obj)->GetType();
		if ( _type == AME_MEASURE_LINE || _type == AME_MEASURE_PROJ_LINE || _type == AME_MEASURE_POLYLINE || _type == AME_MEASURE_CURVE )
		{
			AmeMeasureColorSetting* _setting = &(*it_obj)->GetCurrentColorSetting();
			_setting->setStepLength( m_StepSize[cur] );
			_setting->setStepCheck( cur != 0 );
		}
	}

	UpdateColorSettingOfObj();
	return;
}

long AmeMeasurePlugInGUI::onCmdCursorSize( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwCHCursorSlider == NULL )
	{
		return 1;
	}

	AmeMeasureType  _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		m_ColorSetting[_type]->setCHCursorSize( m_pnwCHCursorSlider->GetValue() );
		UpdateColorSettingOfObj();
	}
	return 1;
}

long AmeMeasurePlugInGUI::onCmdShowProjWarningCheck( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwShowProjWarningCheck == NULL )
	{
		return 1;
	}

	AmeMeasureType  _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		m_ColorSetting[_type]->setShowProjWarning( m_pnwShowProjWarningCheck->GetCheck() ? true : false );
		UpdateColorSettingOfObj();
	}
	return 1;
}

long AmeMeasurePlugInGUI::onCmdShow4PointWarningCheck( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_pnwShow4PointWarningCheck == NULL )
	{
		return 1;
	}

	AmeMeasureType  _type = GetCurrentType();
	if ( _type != AME_MEASURE_NONE )
	{
		m_ColorSetting[_type]->setShow4PointWarning( m_pnwShow4PointWarningCheck->GetCheck() ? true : false );
		UpdateColorSettingOfObj();
	}
	return 1;
}

long AmeMeasurePlugInGUI::onCmdLineOption( PnwObject* obj, PnwEventType, PnwEventArgs* )
{
	AmeMeasureType  _type = GetCurrentType();
	if ( _type == AME_MEASURE_LINE || _type == AME_MEASURE_PROJ_LINE || _type == AME_MEASURE_POLYLINE || _type == AME_MEASURE_CURVE )
	{
		PnwComboBox* combo = dynamic_cast<PnwComboBox*>(obj);
		int _id = combo->GetCurrentItem();

		if ( _id == 2 && _type == AME_MEASURE_PROJ_LINE )
		{
			combo->SetCurrentItem( m_ColorSetting[_type]->getLineOption() );
			PnwMessageBox::Warning( GetTaskManager(), MESSAGE_BUTTON_OK, AppAttr->GetResStr( "AME_COMMON_WARNING" ), AppAttr->GetResStr( "AME_MEASURE_CANT_USE_CS_SCALE_ON_PROJ_LINE" ) );
			return 0;
		}

		m_ColorSetting[_type]->setLineOption( _id );

		//直線または折れ線の全オブジェクトで表示方法を統一する
		std::vector<AmeMeasureDrawObject*>::const_iterator it_obj = m_pEngine->GetAllMeasure().begin();
		for ( ; it_obj != m_pEngine->GetAllMeasure().end(); ++it_obj )
		{
			AmeMeasureType type_ = (*it_obj)->GetType();
			if ( _type == type_ )
			{
				AmeMeasureColorSetting* _setting = &(*it_obj)->GetCurrentColorSetting();
				_setting->setLineOption( _id );
			}
		}

		UpdateColorSettingOfObj();
		UpdateColorSettingOfGui();
	}
	return 1;
}

long AmeMeasurePlugInGUI::onCmdLineOptionMenu( PnwObject* obj, PnwEventType, PnwEventArgs* )
{
	AmeMeasureType  _type = GetCurrentType();
	if ( _type == AME_MEASURE_LINE || _type == AME_MEASURE_PROJ_LINE || _type == AME_MEASURE_POLYLINE || _type == AME_MEASURE_CURVE )
	{
		int _id = PtrToInt( obj->GetUserData() );

		m_ColorSetting[_type]->setLineOption( _id );

		//直線または折れ線の全オブジェクトで表示方法を統一する
		std::vector<AmeMeasureDrawObject*>::const_iterator it_obj = m_pEngine->GetAllMeasure().begin();
		for ( ; it_obj != m_pEngine->GetAllMeasure().end(); ++it_obj )
		{
			AmeMeasureType type_ = (*it_obj)->GetType();
			if ( _type == type_ )
			{
				AmeMeasureColorSetting* _setting = &(*it_obj)->GetCurrentColorSetting();
				_setting->setLineOption( _id );
			}
		}

		UpdateColorSettingOfObj();
		UpdateColorSettingOfGui();
	}
	return 1;
}

void AmeMeasurePlugInGUI::UpdateColorSettingOfGui()
{
	//GUIの更新
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( true );
	if ( current != NULL )
	{
		AmeMeasureType _type = current->GetType();
		if ( _type < m_ColorSetting.size() )
		{
			*(m_ColorSetting[_type]) = current->GetCurrentColorSetting();
			SetCurrentColorSetting();
		}
	}
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
}

void AmeMeasurePlugInGUI::UpdateColorSettingOfObj()
{
	//計測オブジェクトの更新
	AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( true );
	if ( current != NULL )
	{
		AmeMeasureType _type = current->GetType();
		if ( _type < m_ColorSetting.size() )
		{
			current->SetCurrentColorSetting( m_ColorSetting[_type] );
		}
	}
	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
}

AmeMeasureColorSetting* AmeMeasurePlugInGUI::GetCurrentColorSetting()
{
	AmeMeasureType  _type = GetCurrentType();
	if ( (_type != AME_MEASURE_NONE) && (_type < m_ColorSetting.size()) )
	{
		return m_ColorSetting[_type];
	}
	return NULL;
}

void AmeMeasurePlugInGUI::SetCurrentColorSetting()
{
	if ( m_SettingFrame.size() > 0 && m_SettingFrame[0] != NULL && m_SettingFrame[0]->GetVisible() )
	{
		AmeMeasureType  _type = GetCurrentType();
		if ( (_type != AME_MEASURE_NONE) && (_type < m_ColorSetting.size()) )
		{
			//ここから、設定の追加・変更時は追加・変更すること
			if ( m_pnwTextFontName != NULL )
			{
				m_pnwTextFontName->SetText( m_ColorSetting[_type]->getTextFontName() );
			}
			UpdateFontSizeCombo( m_ColorSetting[_type]->getTextFontRate() );
			if ( m_pnwMeasureTextColor != NULL )
			{
				m_pnwMeasureTextColor->SetColor( m_ColorSetting[_type]->getFgColor() );
			}
			if ( m_pnwMeasureBackColor != NULL )
			{
				m_pnwMeasureBackColor->SetColor( m_ColorSetting[_type]->getBgColor() );
			}
			if ( m_pnwMeasureCpColor != NULL )
			{
				m_pnwMeasureCpColor->SetColor( m_ColorSetting[_type]->getCpColor() );
			}
			if ( m_pnwTextBackCheck != NULL )
			{
				m_pnwTextBackCheck->SetCheck( m_ColorSetting[_type]->getTextBackCheck() );
			}
			if ( m_pnwMeasureBackColorButton != NULL )
			{
				m_pnwMeasureBackColorButton->SetEnabled( m_ColorSetting[_type]->getTextBackCheck() );
			}
			if ( m_pnwMeasureBackColor != NULL )
			{
				m_pnwMeasureBackColor->SetEnabled( m_ColorSetting[_type]->getTextBackCheck() );
			}
			if ( m_pnwTextFrameCheck != NULL )
			{
				m_pnwTextFrameCheck->SetCheck( m_ColorSetting[_type]->getTextFrameCheck() );
			}
			//ポイント計測のみ
			if ( m_pnwCHCursorSlider != NULL )
			{
				if ( _type == AME_MEASURE_POINT )
				{
					m_pnwCHCursorSlider->SetValue( m_ColorSetting[_type]->getCHCursorSize() );
				}
			}
			//投影直線および投影三点計測のみ
			if ( m_pnwShowProjWarningCheck != NULL )
			{
				if ( _type == AME_MEASURE_PROJ_LINE || _type == AME_MEASURE_PROJ_ANGLE )
				{
					m_pnwShowProjWarningCheck->SetCheck( m_ColorSetting[_type]->getShowProjWarning() );
				}
			}
			//四点計測のみ
			if ( m_pnwShow4PointWarningCheck != NULL )
			{
				if ( _type == AME_MEASURE_TWO_LINE_ANGLE )
				{
					m_pnwShow4PointWarningCheck->SetCheck( m_ColorSetting[_type]->getShow4PointWarning() );
				}
			}
			if ( m_pnwLineOption != NULL )
			{
				m_pnwLineOption->SetCurrentItem( m_ColorSetting[_type]->getLineOption() );
			}
			if ( m_pnwCurveOption != NULL )
			{
				m_pnwCurveOption->SetCurrentItem( m_ColorSetting[_type]->getLineOption() );
			}
			//ここまで
		}
	}
}

bool AmeMeasurePlugInGUI::CheckBtnState()
{
	//計測ボタンの押下チェック
	bool _btnstate = false;

	//以下は状態を調べるだけなので、m_pnwShortCutBtnはみない。
	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		if ( m_pnwMeasureMethodBtn[i] != NULL )
		{
			if ( m_pnwMeasureMethodBtn[i]->GetButtonState() )
			{
				_btnstate = true;
				break;
			}
		}
	}
	//直線計測用ボタンの押下チェック
	if ( m_pnwMeasureLineBtn != NULL && m_pnwMeasureLineBtn->GetButtonState() )
	{
		_btnstate = true;
	}
	//折れ線計測用ボタンの押下チェック
	if ( m_pnwMeasurePolylineBtn != NULL && m_pnwMeasurePolylineBtn->GetButtonState() )
	{
		_btnstate = true;
	}
	//角度計測用ボタンの押下チェック
	if ( m_pnwMeasureAngleBtn != NULL && m_pnwMeasureAngleBtn->GetButtonState() )
	{
		_btnstate = true;
	}
	return _btnstate;
}

AmeMeasureType AmeMeasurePlugInGUI::GetCurrentType()
{
	AmeMeasureType _type = AME_MEASURE_NONE;
	if ( GetTaskManagerEngine()->IsConsole() )
	{
		AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
		if ( current != NULL )
		{
			_type = current->GetType();
		}
		else
		{
			_type = m_CurrentType;
		}
	}
	else
	{
		AmeMeasureDrawObject* current = m_pEngine->GetCurrentMeasure( false );
		bool _btnstate = CheckBtnState();
		if ( _btnstate )
		{
			_type = m_CurrentType;
		}
		else if ( current != NULL )
		{
			_type = current->GetType();
		}
	}
	return _type;
}


//! 更新通知メソッド
int AmeMeasurePlugInGUI::Update( int param, __int64 arg )
{
	switch ( param )
	{
		case AmeBasePlugInEngine::MACRO_CHANGE_INTERFACE:
			if ( arg == AmeMacroCommandCreatorInf::BEGIN_REGISTRATION )
			{
				if ( m_pResultDialog != nullptr )
				{
					m_pResultDialog->Hide();
				}
			}
			else if ( arg == AmeMacroCommandCreatorInf::END_REGISTRATION )
			{
			}
			break;
	}
	return 0;
}

bool AmeMeasurePlugInGUI::CreateConsoleShortCutMenu( PnwWidget* menu, AmeImageViewer* viewer, PnwEventArgs* )
{
	//0番目以外計測させない
	if ( viewer == NULL || viewer->GetCurrentViewerAttrIndex() != 0 )
	{
		return false;
	}

	bool bCascade = !(this->IsCurrentMouseMode());

	PnwMenuCommand* me[AME_MEASURE_NUM];

	for ( size_t i = AME_MEASURE_LINE; i < AME_MEASURE_NUM; i++ )
	{
		if ( m_pnwMeasureShortcutIcon[i] == NULL )
		{
			AmeString fname2 = AmeString::Format( L"AME_ICON_SHORTCUT_MEASURE%Id", i + 1 );
			m_pnwMeasureShortcutIcon[i] = AmeLoadPlugInIcon( GetPlugInName(), fname2 );
		}
	}

	for ( size_t i = 0; i < AME_MEASURE_NUM; i++ )
	{
		int id = AmeMeasureDisplayOrder[i];
		me[id] = new PnwMenuCommand( menu, AppAttr->GetResStr( AmeMeasureResourceString[id] ), NULL, this, ID_MEASURE_TYPE );
		switch ( id )
		{
			case AME_MEASURE_LINE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_LINE] ); break;
			case AME_MEASURE_POLYLINE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_POLYLINE] ); break;
			case AME_MEASURE_ANGLE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_ANGLE] ); break;
			case AME_MEASURE_CUBE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_CUBE] ); break;
			case AME_MEASURE_ELLIPSE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_ELLIPSE] ); break;
			case AME_MEASURE_ROI: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_ROI] ); break;
			case AME_MEASURE_FREEHAND: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_FREEHAND] ); break;
			case AME_MEASURE_POINT: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_POINT] ); break;
			case AME_MEASURE_BOX: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_BOX] ); break;
			case AME_MEASURE_SPHERE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_SPHERE] ); break;
			case AME_MEASURE_PROJ_ANGLE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_PROJ_ANGLE] ); break;
			case AME_MEASURE_TWO_LINE_ANGLE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_TWO_LINE_ANGLE] ); break;
			case AME_MEASURE_PROJ_LINE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_PROJ_LINE] ); break;
			case AME_MEASURE_CURVE: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_CURVE] ); break;
			case AME_MEASURE_TTTG: me[id]->SetIcon( m_pnwMeasureShortcutIcon[AME_MEASURE_TTTG] ); break;
		}
	}

	if ( !viewer->GetViewerAttr()->GetImageAttr()->IsEnableOperation( AmeImageAttr::ENABLE_TILT_MEASUREMENT ) )
	{
		me[AME_MEASURE_BOX]->Hide();
		me[AME_MEASURE_SPHERE]->Hide();
	}

	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		me[i]->SetUserData( IntToPtr( i ) );
	}

	if ( (!bCascade) && (m_CurrentType >= 0) && (m_CurrentType < AME_MEASURE_NUM) )
	{
		me[m_CurrentType]->SetActiveState( true );
	}

	for ( int i = 0; i < AME_MEASURE_NUM; i++ )
	{
		bool bValidScale;
		if ( !IsMeasureCreatable( (AmeMeasureType)i, viewer, bValidScale ) )
		{
			me[i]->SetEnabled( false );
			me[i]->Hide();
		}
	}

	/*
	// Consoleでは利用せず
	// 全体計測系は、3Dプラグインのみ使用可能とする。
	AmePlugInGUIInformation info = GetTaskManager()->GetTaskCardPlugInGUI();
	if (AmeIs3DViewer(info.m_pPlugInAttr->m_PlugInName) || info.m_pPlugInAttr->m_PlugInName == L"AME_PG_INTERPRETER3D"){
		new PnwMenuSeparator(menu);
		new PnwMenuCommand(menu, AppAttr->GetResStr("AME_MEASURE_MES_ALL"), NULL, this, ID_MEASURE_ALL);
		new PnwMenuCommand(menu, AppAttr->GetResStr("AME_MEASURE_MES_ONE"), NULL, this, ID_MEASURE_SINGLE);
	}
	*/

	return true;
}

/**
	@brief 最近点計算実行イベントハンドラ
 **/
long AmeMeasurePlugInGUI::onCmdExecuteClosest( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_ROIData.m_targetROIViewerAttribute == nullptr )
	{
		return 0;
	}

	AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Measuring the nearest distance." );

	int status = AME_ERROR_SUCCESS;
	Ame::Math::Pointf3 points[2];
	{
		auto progressManager = Ame::GUIProgressManager::Create(*GetTaskManager());
		if (progressManager == nullptr)
		{
			assert(false);
			return 0;
		}
		if (m_ROIData.m_RectROIDrawer != nullptr && m_ROIData.m_RectROIDrawer->IsDisplay())
		{ // 矩形ROIがある場合
			std::vector<Math::Pointf3>	regionPoints;
			if (!m_ROIData.m_RectROIDrawer->GetRegionPointsInVoxelCoordinates(regionPoints))
			{
				return 0;
			}
			status = m_pEngine->SearchClosestPoint(*m_ROIData.m_targetROIViewerAttribute, regionPoints, points, *progressManager);
		}
		else if (m_ROIData.m_BoxROIDrawer != nullptr && m_ROIData.m_BoxROIDrawer->IsDisplay())
		{ // 直方体ROIがある場合
			AABBf3 AABB;
			if (!m_ROIData.m_BoxROIDrawer->GetBoxInVoxelCoordinates(AABB))
			{
				return 0;
			}
			status = m_pEngine->SearchClosestPoint(*m_ROIData.m_targetROIViewerAttribute, AABB, points, *progressManager);
		}
		else
		{
			GetTaskManager()->Error(GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr("AME_MEASURE_CLOSEST_POINT_CALCULATION_FAILED"), L"Closest point calculation failed.");
			return 0;
		}
		progressManager->UpdateProgress(1.f);
	}
	if (status == AME_ERROR_SUCCESS)
	{
		if (m_ROIData.m_BoxROIDrawer != nullptr)
		{
			m_ROIData.m_BoxROIDrawer->EnableDisplay(false);
		}
		if (m_ROIData.m_RectROIDrawer != nullptr)
		{
			m_ROIData.m_RectROIDrawer->EnableDisplay(false);
		}
		// 最近点が見つかった場合、直線計測を作成
		auto imageViewer = m_ROIData.m_targetROIViewerAttribute->GetViewer();
		auto imageAttribute = m_ROIData.m_targetROIViewerAttribute->GetImageAttr();
		if (imageViewer == nullptr || imageAttribute == nullptr)
		{
			assert(false);
			return 0;
		}
		AmeFloat2D displayPosition;
		AmeMeasureLine* line = new AmeMeasureLine(GetTaskManager(), GetPlugInID(), imageAttribute);
		if (line == nullptr)
		{
			assert(false);
			return 0;
		}
		line->m_vecPoint[0].Set(points[0].data());
		line->m_vecPoint[1].Set(points[1].data());
		imageViewer->VoxelToDisplay(line->m_vecPoint[0], displayPosition);
		std::vector<AmeMeasureDrawObject*> measures = m_pEngine->GetAllMeasure();
		line->AdjustResultBoxPosition(imageViewer, AmeFloat2D(displayPosition.X, displayPosition.Y), (int)measures.size());
		line->m_iOrgViewerType = imageViewer->GetViewerType();
		line->m_FrameSerialID = imageViewer->GetViewerFrame()->GetSerialID();
		line->m_ViewerIdentity = m_ROIData.m_targetROIViewerAttribute->GetViewerAttrIDString();
		line->m_ImageAttrIDString = imageAttribute->GetImageAttrIDString();

		if (imageViewer->GetOverlayViewerAttr() != nullptr &&
		   (imageViewer->GetOverlayViewerAttr()->IsEnableOperation(AmeViewerAttr::ENABLE_VALUE_MEASUREMENT) ||
			imageViewer->GetOverlayViewerAttr()->IsEnableOperation(AmeViewerAttr::ENABLE_SCALE_MEASUREMENT)))
		{
			line->m_OverlayImageIdentity = imageViewer->GetOverlayViewerAttr()->GetImageAttr()->GetImageAttrIDString();
		}
		else
		{
			line->m_OverlayImageIdentity = L"";
		}

		int index = 0;
		if (m_pEngine->m_AllObjects.size() != 0)
		{
			index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
		}
		line->SetIndex(index);

		m_pEngine->m_AllObjects.push_back(line); // Adds a new measuring object
		GetTaskManager()->AddDrawObject(line);
		ChangeCurrentByID(line->GetObjectID());
		line->Recalculate( imageViewer ); // これをやらないとヒストグラム表示ボタンが有効にならない
		m_CurrentType = AME_MEASURE_NONE;
		UpdateInterface();
		UpdateInterfaceForShortcutBar(m_pViewerAttr);
		if (m_pEngine->m_UserParams.m_bSyncCurrentPosition)
		{ // 現在位置連動設定がされている場合は現在位置を更新
			Pointf3 center = (points[0] + points[1]) / 2;
			ChangeCurrentPosition(m_ROIData.m_targetROIViewerAttribute, AmeFloat3D(center[0], center[1], center[2]));
		}
		GetTaskManager()->UpdateDisplay(AmeTaskManager::AME_DISPLAY_UPDATE);
	}
	else
	{
		switch ( status )
		{
			case AME_ERROR_WS_NO_EXECUTION:
				GetTaskManager()->Warning( GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr( "AME_MEASURE_NO_REGION" ), L"No region." );
				break;
			case AME_ERROR_WS_ITEM_NOT_FOUND:
				GetTaskManager()->Error(GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr("AME_MEASURE_TOO_FEW_REGION"), L"Number of regions is too few.");
				break;
			case AME_ERROR_CANCELED:
				GetTaskManager()->Error(GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr("AME_MEASURE_CLOSEST_CANCELED"), L"Number of regions is too few.");
				break;
			default:
				GetTaskManager()->Error(GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr("AME_MEASURE_CLOSEST_POINT_CALCULATION_FAILED"), L"Closest point calculation failed.");
				break;

		}
	}
	return 1;
}

/**
	@brief 直径計算実行イベントハンドラ
 **/
long AmeMeasurePlugInGUI::onCmdExecuteDiameter( PnwObject*, PnwEventType, PnwEventArgs* )
{
	if ( m_ROIData.m_targetROIViewerAttribute == nullptr )
	{
		return 0;
	}

	AppAttr->WriteLog( __FUNCSIG__, amelogrank::BEHAVIOR, amelogcode::NO_CODE, L"Measuring the diameter." );

	int status = AME_ERROR_SUCCESS;
	vector<DiameterMeasurementResult> results;
	{
		auto progressManager = Ame::GUIProgressManager::Create( *GetTaskManager() );
		if ( progressManager == nullptr )
		{
			assert( false );
			return 0;
		}
		if ( m_ROIData.m_RectROIDrawer != nullptr && m_ROIData.m_RectROIDrawer->IsDisplay() )
		{ // 矩形ROIがある場合
			std::vector<Math::Pointf3>	regionPoints;
			if ( !m_ROIData.m_RectROIDrawer->GetRegionPointsInVoxelCoordinates( regionPoints ) )
			{
				return 0;
			}
			status = m_pEngine->CalculateDiameters( *m_ROIData.m_targetROIViewerAttribute, regionPoints, results, *progressManager );
		}
		else if ( m_ROIData.m_BoxROIDrawer != nullptr && m_ROIData.m_BoxROIDrawer->IsDisplay() )
		{ // 直方体ROIがある場合
			AABBf3 AABB;
			if ( !m_ROIData.m_BoxROIDrawer->GetBoxInVoxelCoordinates( AABB ) )
			{
				return 0;
			}
			status = m_pEngine->CalculateDiameters( *m_ROIData.m_targetROIViewerAttribute, AABB, results, *progressManager );
		}
		else
		{
			GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr( "AME_MEASURE_DIAMETER_CALCULATION_FAILED" ), L"Diameter calculation failed." );
			return 0;
		}
		progressManager->UpdateProgress( 1.f );
	}
	if ( status == AME_ERROR_SUCCESS )
	{
		// 最近点が見つかった場合、直線計測を作成
		auto imageViewer = m_ROIData.m_targetROIViewerAttribute->GetViewer();
		auto imageAttribute = m_ROIData.m_targetROIViewerAttribute->GetImageAttr();
		if ( imageViewer == nullptr || imageAttribute == nullptr )
		{
			assert( false );
			return 0;
		}
		for ( auto& result : results )
		{
			AmeFloat2D displayPosition;
			{
				AmeMeasureLine* line = new AmeMeasureLine( GetTaskManager(), GetPlugInID(), imageAttribute );
				if ( line == nullptr )
				{
					assert( false );
					return 0;
				}
				line->m_vecPoint[0].Set( result.m_maxDiameterSegment[0].data() );
				line->m_vecPoint[1].Set( result.m_maxDiameterSegment[1].data() );
				imageViewer->VoxelToDisplay( line->m_vecPoint[0], displayPosition );
				std::vector<AmeMeasureDrawObject*> measures = m_pEngine->GetAllMeasure();
				line->AdjustResultBoxPosition( imageViewer, AmeFloat2D( displayPosition.X, displayPosition.Y ), (int)measures.size() );
				line->m_iOrgViewerType = imageViewer->GetViewerType();
				line->m_FrameSerialID = imageViewer->GetViewerFrame()->GetSerialID();
				line->m_ViewerIdentity = m_ROIData.m_targetROIViewerAttribute->GetViewerAttrIDString();
				line->m_ImageAttrIDString = imageAttribute->GetImageAttrIDString();

				if ( imageViewer->GetOverlayViewerAttr() != nullptr &&
				   (imageViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
					imageViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
				{
					line->m_OverlayImageIdentity = imageViewer->GetOverlayViewerAttr()->GetImageAttr()->GetImageAttrIDString();
				}
				else
				{
					line->m_OverlayImageIdentity = L"";
				}

				int index = 0;
				if ( m_pEngine->m_AllObjects.size() != 0 )
				{
					index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
				}
				line->SetIndex( index );

				m_pEngine->m_AllObjects.push_back( line ); // Adds a new measuring object
				GetTaskManager()->AddDrawObject( line );
			}
			{
				AmeMeasureLine* line = new AmeMeasureLine( GetTaskManager(), GetPlugInID(), imageAttribute );
				if ( line == nullptr )
				{
					assert( false );
					return 0;
				}
				line->m_vecPoint[0].Set( result.m_minDiameterSegment[0].data() );
				line->m_vecPoint[1].Set( result.m_minDiameterSegment[1].data() );
				imageViewer->VoxelToDisplay( line->m_vecPoint[0], displayPosition );
				std::vector<AmeMeasureDrawObject*> measures = m_pEngine->GetAllMeasure();
				line->AdjustResultBoxPosition( imageViewer, AmeFloat2D( displayPosition.X, displayPosition.Y ), (int)measures.size() );
				line->m_iOrgViewerType = imageViewer->GetViewerType();
				line->m_FrameSerialID = imageViewer->GetViewerFrame()->GetSerialID();
				line->m_ViewerIdentity = m_ROIData.m_targetROIViewerAttribute->GetViewerAttrIDString();
				line->m_ImageAttrIDString = imageAttribute->GetImageAttrIDString();

				if ( imageViewer->GetOverlayViewerAttr() != nullptr &&
				   (imageViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_VALUE_MEASUREMENT ) ||
					imageViewer->GetOverlayViewerAttr()->IsEnableOperation( AmeViewerAttr::ENABLE_SCALE_MEASUREMENT )) )
				{
					line->m_OverlayImageIdentity = imageViewer->GetOverlayViewerAttr()->GetImageAttr()->GetImageAttrIDString();
				}
				else
				{
					line->m_OverlayImageIdentity = L"";
				}

				int index = 0;
				if ( m_pEngine->m_AllObjects.size() != 0 )
				{
					index = m_pEngine->m_AllObjects.back()->GetIndex() + 1;
				}
				line->SetIndex( index );

				m_pEngine->m_AllObjects.push_back( line ); // Adds a new measuring object
				GetTaskManager()->AddDrawObject( line );
			}
		}
		UpdateInterface();
		UpdateInterfaceForShortcutBar( m_pViewerAttr );
		GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
	}
	else
	{
		switch ( status )
		{
			case AME_ERROR_WS_NO_EXECUTION:
				GetTaskManager()->Warning( GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr( "AME_MEASURE_NO_TARGET_DIAMETER_CALCULATION" ), L"No area where the diameter can be measured." );
				break;
			case AME_ERROR_CANCELED:
				GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr( "AME_MEASURE_DIAMETER_CANCELED" ), L"Diameter calculation canceled." );
				break;
			default:
				GetTaskManager()->Error( GetTaskManager(), __FUNCSIG__, status, AppAttr->GetResStr( "AME_MEASURE_DIAMETER_CALCULATION_FAILED" ), L"Diameter calculation failed." );
				break;

		}
	}
	return 1;
}

// 最近点計算用直方体ROIが右クリックメニューで削除を選択された場合のイベントハンドラ
long AmeMeasurePlugInGUI::onCmdDeleteClosestBoxROI(PnwObject*, PnwEventType, PnwEventArgs*)
{
	if (m_ROIData.m_BoxROIDrawer != nullptr)
	{
		m_ROIData.m_BoxROIDrawer->EnableDisplay(false);
	}
	GetTaskManager()->UpdateDisplay(AmeTaskManager::AME_DISPLAY_UPDATE);
	return 1;
}

// 最近点計算用矩形ROIが右クリックメニューで削除を選択された場合のイベントハンドラ
long AmeMeasurePlugInGUI::onCmdDeleteClosestRectROI(PnwObject*, PnwEventType, PnwEventArgs*)
{
	if (m_ROIData.m_RectROIDrawer != nullptr)
	{
		m_ROIData.m_RectROIDrawer->EnableDisplay(false);
	}
	GetTaskManager()->UpdateDisplay(AmeTaskManager::AME_DISPLAY_UPDATE);
	return 1;
}

void AmeMeasurePlugInGUI::SlotCmdMeasurementList( const int row, const int column )
{
	if ( m_frameMeasureList == nullptr )
	{
		return;
	}

	Ame::ListWidget* list = m_frameMeasureList->GetMeasurementList();

	// ヒストグラム表示のON・OFFを操作された時
	if ( column == 0 )
	{
		int id = list->GetItemData( row );
		for ( auto it = m_pEngine->m_AllObjects.begin(); it != m_pEngine->m_AllObjects.end(); it++ )
		{
			if ( id == (*it)->GetObjectID() )
			{
				(*it)->m_bVisible = !(*it)->m_bVisible;
				if ( (*it)->m_bVisible )
				{
					list->SetItemDetailSubIcon( row, 0, 0, AppAttr->GetAppIcon( AME_ICON_CHECK ) );
				}
				else
				{
					list->SetItemDetailSubIcon( row, 0, 0, AppAttr->GetAppIcon( AME_ICON_BLANK ) );
				}

				GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
				break;
			}
		}
		return;
	}
	else
	{
		list->SetCurrentItem( row );
	}

	// コピー中の計測があれば取りやめ
	CancelCopy();

	//作成途中の計測があれば、確定させる。
	if ( m_bNowCreating )
	{
		FinishCreatingMeasure();
	}

	// カレントを参照するためのリスト上でのインデックスを取得
	int tempIdx = list->GetCurrentItem();
	if ( tempIdx < 0 || tempIdx >= m_pEngine->m_AllObjects.size() )
	{
		return;
	}
	ChangeCurrentByID( (int)list->GetItemData( tempIdx ) );

	// 画面更新
	UpdateMeasurementList();

	GetTaskManager()->UpdateDisplay( AmeTaskManager::AME_DISPLAY_UPDATE );
}

// 最近点計算用直方体ROIが更新された場合のコールバック
void AmeMeasurePlugInGUI::OnMotionClosestBoxROI(BoxROIGeometryType i_geometryType, int i_index)
{
	switch (i_geometryType)
	{
		case BoxROIGeometryType::Center:
		{
			ChangeCurrentPosition(m_ROIData.m_targetROIViewerAttribute, m_ROIData.m_BoxROIDrawer->GetBoxCenter());
			break;
		}
		case BoxROIGeometryType::Vertex:
		{
			ChangeCurrentPosition(m_ROIData.m_targetROIViewerAttribute, m_ROIData.m_BoxROIDrawer->GetCoordinateOfPoint(i_index));
			break;
		}
	}
}

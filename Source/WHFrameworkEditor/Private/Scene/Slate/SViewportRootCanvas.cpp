// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Slate/SViewportRootCanvas.h"

#include "IAssetViewport.h"
#include "Scene/SceneEditor.h"
#include "Slate/SObjectWidget.h"
#include "Widget/World/WorldWidgetBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SViewportRootCanvas::SViewportRootCanvas()
{
	Viewport = nullptr;
}

void SViewportRootCanvas::Construct(const FArguments& InArgs)
{
	Viewport = InArgs._Viewport;

	SetVisibility(EVisibility::SelfHitTestInvisible);

	SConstraintCanvas::Construct(SConstraintCanvas::FArguments());
}

int32 SViewportRootCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayerId = SConstraintCanvas::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FChildren* ChildWidgets = const_cast<SViewportRootCanvas*>(this)->GetChildren();
	for (int32 i = 0; i < ChildWidgets->Num(); i++)
	{
		FSlot& ChildSlot = static_cast<FSlot&>(const_cast<FSlotBase&>(ChildWidgets->GetSlotAt(i)));

		if(TSharedPtr<SObjectWidget> ChildWidget = StaticCastSharedPtr<SObjectWidget>(ChildWidgets->GetChildAt(i).ToSharedPtr()))
		{
			if(UWorldWidgetBase* WorldWidget = Cast<UWorldWidgetBase>(ChildWidget->GetWidgetObject()))
			{
				FSceneViewProjectionData ProjectionData;
				if (FSceneEditorManager::Get().GetSceneViewProjectionData(&Viewport->GetAssetViewportClient(), ProjectionData))
				{
					FWorldWidgetMapping Mapping;
					if(WorldWidget->GetWidgetMapping(WorldWidget, Mapping))
					{
						FVector2D ScreenPosition;
						if (FSceneView::ProjectWorldToScreen(Mapping.GetLocation(), ProjectionData.GetConstrainedViewRect(), ProjectionData.ComputeViewProjectionMatrix(), ScreenPosition))
						{
							ChildSlot.SetOffset(ScreenPosition);
						}
						ChildWidget->SetVisibility(GIsPlaying ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible);
					}
				}
			}
		}
	}

	return MaxLayerId;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

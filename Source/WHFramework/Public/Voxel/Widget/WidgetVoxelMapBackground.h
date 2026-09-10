#pragma once

#include "Scene/Widget/WidgetSceneMapBase.h"
#include "WidgetVoxelMapBackground.generated.h"

UCLASS()
class WHFRAMEWORK_API UWidgetVoxelMapBackground : public UWidgetSceneMapBase
{
	GENERATED_BODY()

public:
	static int32 GetLODStride(float InCellPixelSize);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel Map|Style")
	FLinearColor BackgroundColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel Map|Style")
	FLinearColor MapTintColor = FLinearColor::White;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
};

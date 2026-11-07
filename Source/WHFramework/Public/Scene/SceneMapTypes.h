#pragma once

#include "CoreMinimal.h"

#include "SceneMapTypes.generated.h"

UENUM(BlueprintType)
enum class EWorldMiniMapMode : uint8
{
	// 无
	None,
	// 固定位置
	FixedPoint,
	// 视角位置
	ViewPoint,
	// 相机位置
	CameraPoint
};

UENUM(BlueprintType)
enum class EWorldMiniMapSource : uint8
{
	None,
	SceneCapture,
	Widget
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneMapView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Range = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Yaw = 0.f;

	FVector2D WorldToPanel(const FVector2D& InWorld, const FVector2D& InSize) const
	{
		const FVector2D Delta = (InWorld - Center).GetRotated(-Yaw);
		return InSize * 0.5 + FVector2D(Delta.X, -Delta.Y) * (Range > UE_SMALL_NUMBER ? InSize.X / Range : 0.0);
	}

	FVector2D PanelToWorld(const FVector2D& InPanel, const FVector2D& InSize) const
	{
		const FVector2D Delta = InPanel - InSize * 0.5;
		return Center + (FVector2D(Delta.X, -Delta.Y) * (InSize.X > UE_SMALL_NUMBER ? Range / InSize.X : 0.0)).GetRotated(Yaw);
	}
};

/**
* 世界文本风格
*/

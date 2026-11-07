#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "SceneAreaTypes.generated.h"

UENUM(BlueprintType)
enum class ESceneAreaShape : uint8
{
	// 方形
	Box,
	// 椭圆
	Ellipse,
	// 多边形
	Polygon
};

/**
* 世界大地图区域类型
*/
UENUM(BlueprintType)
enum class ESceneAreaType : uint8
{
	Default,
	Chunk
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneArea
{
	GENERATED_BODY()

public:
	FSceneArea()
	{
		AreaName = NAME_None;
		AreaDisplayName = FText::GetEmpty();
		AreaType = ESceneAreaType::Default;
		AreaShape = ESceneAreaShape::Box;
		AreaCenter = FVector2D::ZeroVector;
		AreaRadius = FVector2D::ZeroVector;
		AreaPoints = TArray<FVector2D>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AreaName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText AreaDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESceneAreaType AreaType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESceneAreaShape AreaShape;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == ESceneAreaShape::Box || AreaShape == ESceneAreaShape::Ellipse"))
	FVector2D AreaCenter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == ESceneAreaShape::Box || AreaShape == ESceneAreaShape::Ellipse"))
	FVector2D AreaRadius;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == ESceneAreaShape::Polygon"))
	TArray<FVector2D> AreaPoints;

	/** 生成结构的首选世界空间导航点 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector EntranceLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer FeatureTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId SourceAssetID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGuid EncounterID;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSceneAreaAdded, const FSceneArea&);
DECLARE_MULTICAST_DELEGATE(FOnSceneAreasChanged);
DECLARE_DELEGATE_RetVal_TwoParams(FSceneArea, FSceneAreaResolver, const FSceneArea&, const FVector2D&);

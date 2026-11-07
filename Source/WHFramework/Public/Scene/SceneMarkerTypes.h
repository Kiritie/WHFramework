#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "SceneMarkerTypes.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSceneMarkersChanged);


UENUM(BlueprintType, meta = (Bitflags))
enum class ESceneMarkerChannel : uint8
{
	None = 0,
	Map = 1 << 0,
	MiniMap = 1 << 1,
	Compass = 1 << 2,
	World = 1 << 3
};
ENUM_CLASS_FLAGS(ESceneMarkerChannel);

/**
 *
 */

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid MarkerID;

	/** Invalid means the marker is visible to all players. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid OwnerPlayerId;

	/** Keep the map entry, but show navigation channels only while this player tracks it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bNavigationOnlyWhenTracked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag MarkerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Symbol;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Badge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Color = FLinearColor::White;

	/** 世界空间备用位置，绑定角色卸载时更新 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid ActorID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName AreaName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = "/Script/WHFramework.ESceneMarkerChannel"))
	int32 Channels = static_cast<int32>(ESceneMarkerChannel::Map);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MinDistance = 0.f;

	/** 零表示不限制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MaxDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPersistent = false;

	bool HasChannel(ESceneMarkerChannel InChannel) const
	{
		return EnumHasAnyFlags(static_cast<ESceneMarkerChannel>(Channels), InChannel);
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneMarkerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FSceneMarker Marker;

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float Distance = 0.f;

	/** 相对给定视角偏航的有符号角度 */
	UPROPERTY(BlueprintReadOnly)
	float Bearing = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bTargetLoaded = false;

	UPROPERTY(BlueprintReadOnly)
	bool bTracked = false;

	UPROPERTY(BlueprintReadOnly)
	bool bPlayer = false;
};

/**
 *
 */

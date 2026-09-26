#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VoxelWorldObjectDefinition.generated.h"

class AVoxelWorldObjectPresenter;
class UStaticMesh;

UENUM(BlueprintType)
enum class EVoxelWorldObjectPresentation : uint8
{
	None,
	SectionShape,
	StaticMesh,
	PresenterActor
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelWorldObjectPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FIntVector Offset = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere)
	FName BlockName;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "65535"))
	int32 State = 0;

	UPROPERTY(EditAnywhere)
	bool bNeedsFloor = false;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelWorldObjectDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UVoxelWorldObjectDefinition();

	UPROPERTY(EditAnywhere)
	FName DefinitionId;

	UPROPERTY(EditAnywhere)
	TArray<FVoxelWorldObjectPart> Footprint;

	UPROPERTY(EditAnywhere)
	FName UseAction;

	UPROPERTY(EditAnywhere)
	TArray<FName> Capabilities;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "65535"))
	int32 ToggleMask = 0;

	UPROPERTY(EditAnywhere)
	EVoxelWorldObjectPresentation Presentation = EVoxelWorldObjectPresentation::SectionShape;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere)
	TSubclassOf<AVoxelWorldObjectPresenter> PresenterClass;

	UPROPERTY(EditAnywhere)
	FTransform MeshTransform;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float MaterializationRadius = 4800.f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float LightRadius = 1600.f;

	UPROPERTY(EditAnywhere)
	FLinearColor LightColor = FLinearColor(1.f, .65f, .25f);

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float LightIntensity = 0.f;
};

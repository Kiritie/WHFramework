// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/AbilityModuleTypes.h"
#include "Asset/AssetModuleTypes.h"
#include "Math/MathTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"

#include "VoxelModuleTypes.generated.h"

class IVoxelAgentInterface;
class UVoxelData;
class AVoxelChunk;
class AVoxelAuxiliary;
class UVoxel;

UENUM(BlueprintType)
enum class EVoxelActionType : uint8
{
	Action1,
	Action2,
	Action3
};

UENUM(BlueprintType)
enum class EVoxelWorldMode : uint8
{
	None,
	Normal,
	Preview,
};

UENUM(BlueprintType)
enum class EVoxelWorldState : uint8
{
	None,
	Spawning,
	Destroying,
	Generating,
	LoadingMap,
	BuildingMap,
	BuildingMesh,
	SavingData
};

/**
 * ????????
 */
UENUM(BlueprintType)
enum class EVoxelType : uint8
{
	Empty, //??
	Unknown,
	Bedrock, //????
	Dirt, //????
	Stone, //??
	Sand, //???
	Grass, //???
	Snow, //???
	Oak, //???
	Birch, //?????
	Oak_Plank, //?????
	Birch_Plank, //???????
	Cobble_Stone, //??
	Stone_Brick, //??
	Red_Brick, //???
	Sand_Stone, //??
	Oak_Leaves, //?????
	Birch_Leaves, //???????
	Ice, //????
	Glass, //????
	Oak_Door, //?????
	Oak_Door_Upper, //?????
	Birch_Door, //???????
	Birch_Door_Upper, //???????
	Torch, //???
	Water, // ?
	Tall_Grass, //?????
	Flower_Allium, 
	Flower_Blue_Orchid,
	Flower_Dandelion,
	Flower_Houstonia,
	Flower_Oxeye_Daisy,
	Flower_Paeonia,
	Flower_Rose,
	Flower_Tulip_Orange,
	Flower_Tulip_Pink,
	Flower_Tulip_Red,
	Flower_Tulip_White,
	Custom1,
	Custom2,
	Custom3,
	Custom4,
	Custom5,
	Custom6,
	Custom7,
	Custom8,
	Custom9,
	Custom10
};

/**
 * ????
 */
UENUM(BlueprintType)
enum class EVoxelPartType : uint8
{
	Main,
	Part
};

/**
 * ????
 */
UENUM(BlueprintType)
enum class EVoxelMeshType : uint8
{
	Cube,
	Custom
};

/**
 * ?????
 */
UENUM(BlueprintType)
enum class EVoxelTransparency : uint8
{
	// ???
	Solid,
	// ?????
	SemiTransparent,
	// ???
	Transparent
};

/**
 * ????????????
 */
UENUM(BlueprintType)
enum class EVoxelMeshNature : uint8
{
	// ?????
	Chunk,
	// ??????
	PickUp,
	// ?????
	Entity,
	// ?????
	Preview,
	// ????????
	Vitality
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelMeshUVData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D UVCorner;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D UVSpan;

	FORCEINLINE FVoxelMeshUVData()
	{
		UVCorner = FVector2D::ZeroVector;
		UVSpan = FVector2D::UnitVector;
	}

	FORCEINLINE FVoxelMeshUVData(FVector2D InUVCorner, FVector2D InUVSpan)
	{
		UVCorner = InUVCorner;
		UVSpan = InUVSpan;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	FString VoxelData;

public:
	FVoxelSaveData()
	{
		VoxelData = TEXT("");
	}
		
	FVoxelSaveData(const FSaveData& InSaveData) : FSaveData(InSaveData)
	{
		VoxelData = TEXT("");
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelItem : public FAbilityItem
{
	GENERATED_BODY()

public:
	static FVoxelItem Empty;
	static FVoxelItem Unknown;

public:
	UPROPERTY(BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(BlueprintReadWrite)
	ERightAngle Angle;

	UPROPERTY(BlueprintReadOnly)
	AVoxelChunk* Owner;

	UPROPERTY(BlueprintReadOnly)
	AVoxelAuxiliary* Auxiliary;

	UPROPERTY(BlueprintReadOnly)
	bool bGenerated;

public:
	FVoxelItem()
	{
		Index = FIndex::ZeroIndex;
		Angle = ERightAngle::RA_0;
		Owner = nullptr;
		Auxiliary = nullptr;
		bGenerated = false;
	}
		
	FVoxelItem(const FAbilityItem& InAbilityItem) : FAbilityItem(InAbilityItem)
	{
		Index = FIndex::ZeroIndex;
		Angle = ERightAngle::RA_0;
		Owner = nullptr;
		Auxiliary = nullptr;
		bGenerated = false;
	}
	
	FVoxelItem(const FVoxelItem& InVoxelItem, int InCount) : FAbilityItem(InVoxelItem, InCount)
	{
		Index = FIndex::ZeroIndex;
		Angle = ERightAngle::RA_0;
		Owner = nullptr;
		Auxiliary = nullptr;
		bGenerated = false;
	}

	FVoxelItem(EVoxelType InVoxelType, bool bRefreshData = false);

	FVoxelItem(const FPrimaryAssetId& InID, bool bRefreshData = false);

	FVoxelItem(const FVoxelSaveData& InSaveData);

public:
	void OnGenerate(IVoxelAgentInterface* InAgent = nullptr);

	void OnDestroy(IVoxelAgentInterface* InAgent = nullptr);

	void RefreshData(UVoxel* InVoxel = nullptr);

	bool IsValid(bool bNeedNotNull = false) const override;

	bool IsUnknown() const;

	bool IsReplaceable(const FVoxelItem& InVoxelItem = FVoxelItem::Empty) const;

	FVoxelSaveData& ToSaveData(bool bRefresh = false) const;

	FVoxelItem& GetMain() const;

	FVoxelItem& GetPart(FIndex InIndex) const;

	TArray<FVoxelItem> GetParts() const;

	EVoxelType GetVoxelType() const;

	FVector GetRange() const;

	FVector GetLocation(bool bWorldSpace = true) const;

	template<class T>
	T& GetVoxel() const
	{
		return static_cast<T&>(GetVoxel());
	}

	UVoxel& GetVoxel() const;

	AVoxelChunk* GetOwner() const;

	template<class T>
	T& GetVoxelData(bool bLogWarning = true) const
	{
		return static_cast<T&>(GetVoxelData(bLogWarning));
	}

	UVoxelData& GetVoxelData(bool bLogWarning = true) const;

	FORCEINLINE friend bool operator==(const FVoxelItem& A, const FVoxelItem& B)
	{
		return (A.ID == B.ID) && (A.Count == B.Count) && (A.Level == B.Level) && (A.Index == B.Index);
	}

	FORCEINLINE friend bool operator!=(const FVoxelItem& A, const FVoxelItem& B)
	{
		return (A.ID != B.ID) || (A.Count != B.Count) || (A.Level != B.Level) && (A.Index != B.Index);
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelHitResult
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector Point;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector Normal;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVoxelItem VoxelItem;

public:
	FVoxelHitResult()
	{
		VoxelItem = FVoxelItem();
		Point = FVector();
		Normal = FVector();
	}
	
	FVoxelHitResult(const FHitResult& InHitResult);
	
	FVoxelHitResult(const FVoxelItem& InVoxelItem, FVector InPoint, FVector InNormal);

public:
	bool IsValid() const;

	UVoxel& GetVoxel() const;

	AVoxelChunk* GetChunk() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelChunkMaterial
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* UnlitMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D BlockUVSize;

	FORCEINLINE FVoxelChunkMaterial()
	{
		Material = nullptr;
		UnlitMaterial = nullptr;
		BlockUVSize = FVector2D(0.0625f, 0.5f);
	}

	FORCEINLINE FVoxelChunkMaterial(UMaterialInterface* InMaterial, UMaterialInterface* InUnlitMaterial, FVector2D InBlockUVSize)
	{
		Material = InMaterial;
		UnlitMaterial = InUnlitMaterial;
		BlockUVSize = InBlockUVSize;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelChunkSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FVoxelSaveData> VoxelDatas;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FPickUpSaveData> PickUpDatas;

	FORCEINLINE FVoxelChunkSaveData()
	{
		Index = FIndex::ZeroIndex;
		VoxelDatas = TArray<FVoxelSaveData>();
		PickUpDatas = TArray<FPickUpSaveData>();
	}

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
		for(auto& Iter : VoxelDatas)
		{
			Iter.MakeSaved();
		}
		for(auto& Iter : PickUpDatas)
		{
			Iter.MakeSaved();
		}
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelWorldBasicSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelWorldBasicSaveData()
	{
		BlockSize = 80;
		ChunkSize = 16;
		
		ChunkHeightRange = 3;

		TerrainBaseHeight = 0.1f;
		TerrainPlainScale = FVector(0.005f, 0.005f, 0.2f);
		TerrainMountainScale = FVector(0.03f, 0.03f, 0.25f);
		TerrainStoneVoxelScale = FVector(0.05f, 0.05f, 0.18f);
		TerrainSandVoxelScale = FVector(0.04f, 0.04f, 0.21f);
		TerrainPlantVoxelScale = FVector(0.5f, 0.5f, 0.3f);
		TerrainTreeVoxelScale = FVector(0.8f, 0.8f, 0.23f);
		TerrainBedrockVoxelHeight = 0.02f;
		TerrainWaterVoxelHeight = 0.25f;

		ChunkMaterials = TArray<FVoxelChunkMaterial>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BlockSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ChunkSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ChunkHeightRange;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TerrainBaseHeight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TerrainBedrockVoxelHeight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TerrainWaterVoxelHeight;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector TerrainPlainScale;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector TerrainMountainScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector TerrainStoneVoxelScale;
				
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector TerrainSandVoxelScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector TerrainPlantVoxelScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector TerrainTreeVoxelScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FVoxelChunkMaterial> ChunkMaterials;

public:
	FORCEINLINE float GetChunkLength() const
	{
		return ChunkSize * BlockSize;
	}

	FORCEINLINE int32 GetWorldHeight(bool bLength = false) const
	{
		return ChunkSize * ChunkHeightRange * (bLength ? BlockSize : 1.f);
	}

	FORCEINLINE FVoxelChunkMaterial GetChunkMaterial(EVoxelTransparency InTransparency) const
	{
		const int32 Index = FMath::Clamp((int32)InTransparency, 0, ChunkMaterials.Num());
		if(ChunkMaterials.IsValidIndex(Index))
		{
			return ChunkMaterials[Index];
		}
		return FVoxelChunkMaterial();
	}

	FORCEINLINE FVector GetBlockSizedNormal(FVector InNormal, float InLength = 0.25f) const
	{
		return BlockSize * InNormal * InLength;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelWorldSaveData : public FVoxelWorldBasicSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelWorldSaveData()
	{
		WorldSeed = 0;
		TimeSeconds = -1.f;
		SecondsOfDay = 600.f;
		RandomStream = FRandomStream();
	}
	
	FORCEINLINE FVoxelWorldSaveData(const FVoxelWorldBasicSaveData& InBasicSaveData) : FVoxelWorldBasicSaveData(InBasicSaveData)
	{
		WorldSeed = 0;
		TimeSeconds = -1.f;
		SecondsOfDay = 600.f;
		RandomStream = FRandomStream();
	}

public:
	static FVoxelWorldSaveData Empty;
	
	UPROPERTY(BlueprintReadOnly)
	int32 WorldSeed;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float TimeSeconds;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float SecondsOfDay;

	UPROPERTY(Transient)
	FRandomStream RandomStream;
	
public:
	virtual bool IsExistChunkData(FIndex InChunkIndex) const { return false; }

	template<class T>
	T* GetChunkData(FIndex InChunkIndex)
	{
		return static_cast<T*>(GetChunkData(InChunkIndex));
	}

	virtual FVoxelChunkSaveData* GetChunkData(FIndex InChunkIndex) { return nullptr; }

	virtual void SetChunkData(FIndex InChunkIndex, FVoxelChunkSaveData* InChunkData) { }
};

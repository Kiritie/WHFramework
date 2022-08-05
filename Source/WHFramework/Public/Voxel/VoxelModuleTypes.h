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
	Generating,
	BasicGenerated,
	FullGenerated
};

/**
 * ????????
 */
UENUM(BlueprintType)
enum class EVoxelType : uint8
{
	Empty, //??
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
	Unknown //δ?
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
enum class EVoxelFacing : uint8
{
	Up,
	Down,
	Forward,
	Back,
	Left,
	Right
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
enum class EVoxelMeshType : uint8
{
	// ?????
	Chunk,
	// ??????
	PickUp,
	// ?????
	Entity,
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
	EVoxelType VoxelType;

public:
	FVoxelSaveData()
	{
		VoxelType = EVoxelType::Empty;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelItem : public FAbilityItem, public FVoxelSaveData
{
	GENERATED_BODY()

public:
	static FVoxelItem Empty;
	static FVoxelItem Unknown;

public:
	UPROPERTY(BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(BlueprintReadWrite)
	FRotator Rotation;
	
	UPROPERTY(BlueprintReadWrite)
	FVector Scale;

	UPROPERTY(BlueprintReadOnly)
	AVoxelChunk* Owner;

	UPROPERTY(BlueprintReadOnly)
	AVoxelAuxiliary* Auxiliary;


public:
	FVoxelItem()
	{
		Index = FIndex::ZeroIndex;
		Rotation = FRotator::ZeroRotator;
		Scale = FVector::OneVector;
		Owner = nullptr;
		Auxiliary = nullptr;
	}
		
	FVoxelItem(const FAbilityItem& InAbilityItem) : FAbilityItem(InAbilityItem)
	{
		Index = FIndex::ZeroIndex;
		Rotation = FRotator::ZeroRotator;
		Scale = FVector::OneVector;
		Owner = nullptr;
		Auxiliary = nullptr;
	}
		
	FVoxelItem(EVoxelType InVoxelType, bool bRefreshData = false);

	FVoxelItem(const FPrimaryAssetId& InID, bool bRefreshData = false);

	FVoxelItem(const FVoxelSaveData& InSaveData);

public:
	virtual bool IsValid(bool bNeedNotNull = false) const override;

	bool IsUnknown() const;

	bool IsReplaceable(const FVoxelItem& InVoxelItem = FVoxelItem::Empty) const;

	void RefreshData();

	FVoxelItem& GetMainItem() const;

	FVoxelItem& GetPartItem(FIndex InIndex) const;

	FVector GetRange() const;

	template<class T>
	T& GetVoxel() const
	{
		return static_cast<T&>(GetVoxel());
	}

	UVoxel& GetVoxel() const;

	AVoxelChunk* GetOwner() const;
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
	FVoxelHitResult();
	
	FVoxelHitResult(const FVoxelItem& InVoxelItem, FVector InPoint, FVector InNormal);

public:
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
	FVector2D BlockUVSize;

	FORCEINLINE FVoxelChunkMaterial()
	{
		Material = nullptr;
		BlockUVSize = FVector2D(0.0625f, 0.5f);
	}

	FORCEINLINE FVoxelChunkMaterial(UMaterialInterface* InMaterial, FVector2D InBlockUVSize)
	{
		Material = InMaterial;
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
	virtual void Reset() override
	{
		FSaveData::Reset();
		Index = FIndex::ZeroIndex;
		VoxelDatas.Empty();
		PickUpDatas.Empty();
	}

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
		TerrainWaterVoxelHeight = 0.3f;
		TerrainBedrockVoxelHeight = 0.02f;

		ChunkMaterials = TArray<FVoxelChunkMaterial>();

		VitalityRaceDensity = 50.f;
		CharacterRaceDensity = 50.f;
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
	TArray<FVoxelChunkMaterial> ChunkMaterials;
							
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float VitalityRaceDensity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CharacterRaceDensity;

public:
	FORCEINLINE float GetChunkLength() const
	{
		return ChunkSize * BlockSize;
	}

	FORCEINLINE int32 GetWorldHeight() const
	{
		return ChunkSize * ChunkHeightRange;
	}

	FORCEINLINE FVoxelChunkMaterial GetChunkMaterial(EVoxelTransparency Transparency) const
	{
		const int32 Index = FMath::Clamp((int32)Transparency, 0, ChunkMaterials.Num());
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
		TimeSeconds = 0.f;
		RandomStream = FRandomStream();
		LastCharacterRaceIndex = FIndex::ZeroIndex;
		LastVitalityRaceIndex = FIndex::ZeroIndex;
	}
	
	FORCEINLINE FVoxelWorldSaveData(const FVoxelWorldBasicSaveData& InBasicSaveData) : FVoxelWorldBasicSaveData(InBasicSaveData)
	{
		WorldSeed = 0;
		TimeSeconds = 0.f;
		RandomStream = FRandomStream();
		LastCharacterRaceIndex = FIndex::ZeroIndex;
		LastVitalityRaceIndex = FIndex::ZeroIndex;
	}

public:
	static FVoxelWorldSaveData Empty;
	
	UPROPERTY(BlueprintReadOnly)
	int32 WorldSeed;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 TimeSeconds;
	
	UPROPERTY(Transient)
	FRandomStream RandomStream;

	UPROPERTY(Transient)
	FIndex LastVitalityRaceIndex;

	UPROPERTY(Transient)
	FIndex LastCharacterRaceIndex;
	
public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
	}

	virtual bool IsExistChunkData(FIndex InChunkIndex) const
	{
		return false;
	}

	template<class T>
	T* GetChunkData(FIndex InChunkIndex)
	{
		return static_cast<T*>(GetChunkData(InChunkIndex));
	}

	virtual FVoxelChunkSaveData* GetChunkData(FIndex InChunkIndex)
	{
		return nullptr;
	}

	virtual void SetChunkData(FIndex InChunkIndex, FVoxelChunkSaveData* InChunkData)
	{
		
	}
};
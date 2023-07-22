// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
enum class EVoxelRaycastType : uint8
{
	FromAimPoint,
	FromMousePosition
};

UENUM(BlueprintType)
enum class EVoxelWorldMode : uint8
{
	None,
	InGame,
	Preview
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

UENUM(BlueprintType)
enum class EVoxelChunkState : uint8
{
	None,
	Spawned,
	Builded,
	Generated,
	Finally
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
		BlockSize = 80.f;
		
		ChunkSize = FVector(16.f);
		
		WorldSize = FVector(-1.f, -1.f, 3.f);
		WorldRange = FVector2D(7.f, 7.f);

		BaseHeight = 0.1f;
		PlainScale = FVector(0.005f, 0.005f, 0.2f);
		MountainScale = FVector(0.03f, 0.03f, 0.25f);
		StoneScale = FVector(0.05f, 0.05f, 0.18f);
		SandScale = FVector(0.04f, 0.04f, 0.21f);
		PlantScale = FVector(0.5f, 0.5f, 0.3f);
		TreeScale = FVector(0.8f, 0.8f, 0.23f);
		BedrockHeight = 0.02f;
		WaterHeight = 0.25f;

		ChunkMaterials = TArray<FVoxelChunkMaterial>();

		TimeSeconds = -1.f;
		SecondsOfDay = 600.f;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BlockSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector ChunkSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector WorldSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D WorldRange;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseHeight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BedrockHeight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float WaterHeight;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector PlainScale;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector MountainScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector StoneScale;
				
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector SandScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector PlantScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector TreeScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FVoxelChunkMaterial> ChunkMaterials;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float TimeSeconds;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SecondsOfDay;

public:
	FORCEINLINE FVector GetChunkRealSize() const
	{
		return ChunkSize * BlockSize;
	}

	FORCEINLINE FVector GetWorldSize() const
	{
		return FVector(WorldSize.X != -1.f ? WorldSize.X : WorldRange.X * 2.f, WorldSize.Y != -1.f ? WorldSize.Y : WorldRange.Y * 2.f, WorldSize.Z);
	}

	FORCEINLINE FVector GetWorldRealSize() const
	{
		return GetWorldSize() * ChunkSize * BlockSize;
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
		RandomStream = FRandomStream();
	}
	
	FORCEINLINE FVoxelWorldSaveData(const FVoxelWorldBasicSaveData& InBasicSaveData) : FVoxelWorldBasicSaveData(InBasicSaveData)
	{
		WorldSeed = 0;
		RandomStream = FRandomStream();
	}

public:
	static FVoxelWorldSaveData Empty;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 WorldSeed;

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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FDefaultVoxelWorldSaveData : public FVoxelWorldSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FDefaultVoxelWorldSaveData()
	{
		ChunkDatas = TMap<FVector, FVoxelChunkSaveData>();
	}
	
	FORCEINLINE FDefaultVoxelWorldSaveData(const FVoxelWorldBasicSaveData& InBasicSaveData) : FVoxelWorldSaveData(InBasicSaveData)
	{
		ChunkDatas = TMap<FVector, FVoxelChunkSaveData>();
	}

public:
	UPROPERTY(BlueprintReadOnly)
	TMap<FVector, FVoxelChunkSaveData> ChunkDatas;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
		for(auto Iter : ChunkDatas)
		{
			Iter.Value.MakeSaved();
		}
	}
	
	virtual bool IsExistChunkData(FIndex InChunkIndex) const override
	{
		return ChunkDatas.Contains(InChunkIndex.ToVector());
	}

	virtual FVoxelChunkSaveData* GetChunkData(FIndex InChunkIndex) override
	{
		if (ChunkDatas.Contains(InChunkIndex.ToVector()))
		{
			return &ChunkDatas[InChunkIndex.ToVector()];
		}
		return nullptr;
	}

	virtual void SetChunkData(FIndex InChunkIndex, FVoxelChunkSaveData* InChunkData) override
	{
		ChunkDatas.Emplace(InChunkIndex.ToVector(), InChunkData->CastRef<FVoxelChunkSaveData>());
	}
};

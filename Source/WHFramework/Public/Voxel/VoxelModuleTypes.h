// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/AbilityModuleTypes.h"
#include "Asset/AssetModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "VoxelModuleTypes.generated.h"

class UVoxelData;
class AVoxelChunk;
class AVoxelAuxiliary;
class UVoxel;
/**
 * ????
 */
UENUM(BlueprintType)
enum class EMouseButton : uint8
{
	Left,
	Middle,
	Right
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
	Birch_Door, //???????
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
enum class EFacing : uint8
{
	Up,
	Down,
	Forward,
	Back,
	Left,
	Right
};

/**
 * ????
 */
UENUM(BlueprintType)
enum class EDirection : uint8
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
enum class ETransparency : uint8
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
	Preview,
	// ????????
	Vitality
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FIndex
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 X;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Z;

	static const FIndex ZeroIndex;

	static const FIndex OneIndex;

	FORCEINLINE FIndex()
	{
		X = 0;
		Y = 0;
		Z = 0;
	}

	FORCEINLINE FIndex(int32 InX, int32 InY, int32 InZ)
	{
		X = InX;
		Y = InY;
		Z = InZ;
	}

	FORCEINLINE FIndex(FVector InVector)
	{
		X = FMath::CeilToInt(InVector.X);
		Y = FMath::CeilToInt(InVector.Y);
		Z = FMath::CeilToInt(InVector.Z);
	}

	FORCEINLINE FIndex(const FString& InValue)
	{
		TArray<FString> tmpArr;
		InValue.ParseIntoArray(tmpArr, TEXT(","));
		X = FCString::Atoi(*tmpArr[0]);
		Y = FCString::Atoi(*tmpArr[1]);
		Z = FCString::Atoi(*tmpArr[2]);
	}

	FORCEINLINE FVector ToVector() const
	{
		return FVector(X, Y, Z);
	}

	FORCEINLINE FString ToString() const
	{
		return FString::Printf(TEXT("%d,%d,%d"), X, Y, Z);
	}

	FORCEINLINE static float Distance(FIndex A, FIndex B, bool bIgnoreZ = false)
	{
		if (bIgnoreZ) A.Z = B.Z = 0;
		return FVector::Distance(A.ToVector(), B.ToVector());
	}

	FORCEINLINE friend bool operator==(const FIndex& A, const FIndex& B)
	{
		return (A.X == B.X) && (A.Y == B.Y) && (A.Z == B.Z);
	}

	FORCEINLINE friend bool operator!=(const FIndex& A, const FIndex& B)
	{
		return (A.X != B.X) || (A.Y != B.Y) || (A.Z != B.Z);
	}

	FORCEINLINE FIndex operator+(const FIndex& InIndex) const
	{
		return FIndex(X + InIndex.X, Y + InIndex.Y, Z + InIndex.Z);
	}

	FORCEINLINE FIndex operator-(const FIndex& InIndex) const
	{
		return FIndex(X - InIndex.X, Y - InIndex.Y, Z - InIndex.Z);
	}

	FORCEINLINE FIndex operator*(const FIndex& InIndex) const
	{
		return FIndex(X * InIndex.X, Y * InIndex.Y, Z * InIndex.Z);
	}

	FORCEINLINE FIndex operator/(const FIndex& InIndex) const
	{
		return FIndex(X / InIndex.X, Y / InIndex.Y, Z / InIndex.Z);
	}

	FORCEINLINE FIndex operator*(const int& InValue) const
	{
		return FIndex(X * InValue, Y * InValue, Z * InValue);
	}

	FORCEINLINE FIndex operator/(const int& InValue) const
	{
		return FIndex(X / InValue, Y / InValue, Z / InValue);
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, FIndex& InValue)
	{
		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		Record << SA_VALUE(TEXT("X"), InValue.X);
		Record << SA_VALUE(TEXT("Y"), InValue.Y);
		Record << SA_VALUE(TEXT("Z"), InValue.Z);
	}
};

FORCEINLINE uint32 GetTypeHash(const FIndex& InIndex)
{
	return FCrc::MemCrc_DEPRECATED(&InIndex, sizeof(InIndex));
}

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FMeshUVData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D UVCorner;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D UVSpan;


	FORCEINLINE FMeshUVData()
	{
		UVCorner = FVector2D::ZeroVector;
		UVSpan = FVector2D::UnitVector;
	}

	FORCEINLINE FMeshUVData(FVector2D InUVCorner, FVector2D InUVSpan)
	{
		UVCorner = InUVCorner;
		UVSpan = InUVSpan;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelItem : public FItem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(BlueprintReadWrite)
	FRotator Rotation;
	
	UPROPERTY(BlueprintReadWrite)
	FVector Scale;
	
	UPROPERTY(BlueprintReadWrite)
	TMap<FName, FParameter> Params;

	UPROPERTY(BlueprintReadWrite)
	AVoxelChunk* Owner;

	UPROPERTY(BlueprintReadWrite)
	AVoxelAuxiliary* Auxiliary;

	static FVoxelItem EmptyVoxel;

	static FVoxelItem UnknownVoxel;

public:
	FVoxelItem()
	{
		Index = FIndex::ZeroIndex;
		Rotation = FRotator::ZeroRotator;
		Scale = FVector::OneVector;
		Params = TMap<FName, FParameter>();
		Owner = nullptr;
		Auxiliary = nullptr;
	}
		
	FVoxelItem(EVoxelType InVoxelType)
	{
		// ID = *FString::Printf(TEXT("Voxel_%d"), (int32)InVoxelType);
		Index = FIndex::ZeroIndex;
		Rotation = FRotator::ZeroRotator;
		Scale = FVector::OneVector;
		Params = TMap<FName, FParameter>();
		Owner = nullptr;
		Auxiliary = nullptr;
	}

	FVoxelItem(const FPrimaryAssetId& InID)
	{
		ID = InID;
		Index = FIndex::ZeroIndex;
		Rotation = FRotator::ZeroRotator;
		Scale = FVector::OneVector;
		Params = TMap<FName, FParameter>();
		Owner = nullptr;
		Auxiliary = nullptr;
	}

public:
	virtual bool IsValid() const override;
	
	bool IsEmpty() const;

	bool IsUnknown() const;

	UVoxel* GetVoxel() const;

	FString GetStringData() const;

	bool HasParam(FName InName) const;

	FParameter GetParam(FName InName);

	void SetParam(FName InName, FParameter InParam);
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
	UVoxel* GetVoxel() const;

	AVoxelChunk* GetOwner() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FChunkMaterial
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D BlockUVSize;

	FORCEINLINE FChunkMaterial()
	{
		Material = nullptr;
		BlockUVSize = FVector2D(0.0625f, 0.5f);
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPickUpSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FItem Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector Location;
	
	FORCEINLINE FPickUpSaveData()
	{
		Item = FItem::Empty;
		Location = FVector::ZeroVector;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FChunkSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FVoxelItem> VoxelItems;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FPickUpSaveData> PickUpDatas;

	FORCEINLINE FChunkSaveData()
	{
		Index = FIndex::ZeroIndex;
		VoxelItems = TArray<FVoxelItem>();
		PickUpDatas = TArray<FPickUpSaveData>();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldBasicSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FWorldBasicSaveData()
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

		ChunkMaterials = TArray<FChunkMaterial>();

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
	TArray<FChunkMaterial> ChunkMaterials;
							
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float VitalityRaceDensity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CharacterRaceDensity;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldSaveData : public FWorldBasicSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FWorldSaveData()
	{
		WorldSeed = 0;
		TimeSeconds = 0.f;
	}
	
	FORCEINLINE FWorldSaveData(FWorldBasicSaveData InBasicSaveData)
	{
		WorldSeed = 0;
		TimeSeconds = 0;

		BlockSize = InBasicSaveData.BlockSize;
		ChunkSize = InBasicSaveData.ChunkSize;
		
		ChunkHeightRange = InBasicSaveData.ChunkHeightRange;

		TerrainBaseHeight = InBasicSaveData.TerrainBaseHeight;
		TerrainPlainScale = InBasicSaveData.TerrainPlainScale;
		TerrainMountainScale = InBasicSaveData.TerrainMountainScale;
		TerrainStoneVoxelScale = InBasicSaveData.TerrainStoneVoxelScale;
		TerrainSandVoxelScale = InBasicSaveData.TerrainSandVoxelScale;
		TerrainWaterVoxelHeight = InBasicSaveData.TerrainWaterVoxelHeight;
		TerrainBedrockVoxelHeight = InBasicSaveData.TerrainBedrockVoxelHeight;

		ChunkMaterials = InBasicSaveData.ChunkMaterials;

		VitalityRaceDensity = InBasicSaveData.VitalityRaceDensity;
		CharacterRaceDensity = InBasicSaveData.CharacterRaceDensity;
	}

public:
	static FWorldSaveData Empty;
	
	UPROPERTY(BlueprintReadOnly)
	int32 WorldSeed;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 TimeSeconds;

public:
	FORCEINLINE float GetChunkLength() const
	{
		return ChunkSize * BlockSize;
	}

	FORCEINLINE int32 GetWorldHeight() const
	{
		return ChunkSize * ChunkHeightRange;
	}

	FORCEINLINE FChunkMaterial GetChunkMaterial(ETransparency Transparency) const
	{
		const int32 Index = FMath::Clamp((int32)Transparency, 0, ChunkMaterials.Num());
		if(ChunkMaterials.IsValidIndex(Index))
		{
			return ChunkMaterials[Index];
		}
		return FChunkMaterial();
	}

	FORCEINLINE FVector GetBlockSizedNormal(FVector InNormal, float InLength = 0.25f) const
	{
		return BlockSize * InNormal * InLength;
	}
};

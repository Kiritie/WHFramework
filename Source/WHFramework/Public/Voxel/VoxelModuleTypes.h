// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Common/CommonTypes.h"
#include "Math/MathTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"
#include "Threads/VoxelChunkQueueThread.h"

#include "VoxelModuleTypes.generated.h"

class UVoxelGenerator;
class IVoxelAgentInterface;
class UVoxelData;
class AVoxelChunk;
class AVoxelAuxiliary;
class UVoxel;

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
	Default,
	Preview,
	Prefab
};

UENUM(BlueprintType)
enum class EVoxelWorldState : uint8
{
	None,
	Spawning,
	Destroying,
	Generating,
	MapLoading,
	MapBuilding,
	MeshSpawning,
	MeshBuilding
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
	Red_Brick, //???
	Stone, //??
	Cobble_Stone, //??
	Stone_Brick, //??
	Stone_Stair, //?????
	Sand, //???
	Sand_Stone, //??
	Glass, //????
	Grass, //???
	Snow, //???
	Ice, //????
	Coal_Ore, //??
	Iron_Ore, //??
	Gold_Ore, //??
	Emerald_Ore, //??
	Diamond_Ore, //??
	Oak, //???
	Oak_Leaves, //?????
	Oak_Plank, //?????
	Oak_Stair, //?????
	Oak_Door, //?????
	Oak_Door_Upper, //?????
	Birch, //?????
	Birch_Leaves, //???????
	Birch_Plank, //???????
	Birch_Stair, //???????
	Birch_Door, //???????
	Birch_Door_Upper, //???????
	Bed, //?????
	Bed_Upper, //?????
	Furnace,
	Crafting_Table, // ?
	Chest, // ?
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
enum class EVoxelElement : uint8
{
	None,
	Stone,
	Wood,
	Silt
};

/**
 * ?????
 */
UENUM(BlueprintType)
enum class EVoxelTransparency : uint8
{
	None,
	// ???
	Solid,
	// ?????
	Semi,
	// ???
	Trans
};

/**
 * ?????
 */
UENUM(BlueprintType)
enum class EVoxelNature : uint8
{
	None,
	// ???
	Solid,
	// ???
	SemiSolid,
	// ???
	SmallSemiSolid,
	// ???
	TransSolid,
	// ???
	Liquid,
	// ???
	SemiLiquid,
	// ?????
	Foliage,
	// ?????
	SemiFoliage
};

/**
 * ????????????
 */
UENUM(BlueprintType)
enum class EVoxelScope : uint8
{
	None,
	// ?????
	Chunk,
	// ?????
	Prefab,
	// ?????
	Entity,
	// ?????
	Capture,
	// ?????
	Preview,
	// ??????
	PickUp,
	// ????????
	Vitality
};

/**
 * 像素声音类型
 */
UENUM(BlueprintType)
enum class EVoxelSoundType : uint8
{
	None,
	// 生成
	Generate,
	// 销毁
	Destroy,
	// 脚步
	Footstep,
	// 打开
	Open,
	// 关闭
	Close
};

/**
 * ????
 */
UENUM(BlueprintType)
enum class EVoxelGenerateToolType : uint8
{
	None,
	Pickaxe,
	Axe,
	Shovel
};

/**
 * 体术交互选项
 */
UENUM(BlueprintType)
enum class EVoxelInteractAction : uint8
{
	// 无
	None = EInteractAction::None,
	// 交互
	Interact = EInteractAction::Interact,
	// 退出
	UnInteract = EInteractAction::UnInteract,
	// 打开
	Open = EInteractAction::Custom1,
	// 关闭
	Close = EInteractAction::Custom2,
};

/**
* 体素世界大地图区域类型
*/
UENUM(BlueprintType)
enum class EVoxelWorldMaxMapAreaType : uint8
{
	// 湖泊
	Lake = EWorldMaxMapAreaType::Area1,
	// 村庄
	Village = EWorldMaxMapAreaType::Area2
};

UENUM(BlueprintType)
enum class EVoxelBiomeType: uint8
{
	None,
	Snow,
	Green,
	Dry,
	Stone,
	Desert
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelMeshUVData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D UVCorner;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D UVSpan;

	UPROPERTY(Transient)
	FVector2D UVOffset;

	FORCEINLINE FVoxelMeshUVData()
	{
		Texture = nullptr;
		UVCorner = FVector2D::ZeroVector;
		UVSpan = FVector2D::UnitVector;
		UVOffset = FVector2D::ZeroVector;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelMeshData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCustomMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MeshScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MeshOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bCustomMesh == true"))
	TArray<FVector> MeshVertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bCustomMesh == true"))
	TArray<FVector> MeshNormals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelMeshUVData> MeshUVDatas;

	FORCEINLINE FVoxelMeshData()
	{
		bCustomMesh = false;
		MeshScale = FVector::OneVector;
		MeshOffset = FVector::ZeroVector;
		MeshVertices = TArray<FVector>();
		MeshNormals = TArray<FVector>();
		MeshUVDatas.SetNum(6);
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelRenderData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelRenderData()
	{
		Material = nullptr;
		UnlitMaterial = nullptr;
		TransMaterial = nullptr;
		PixelSize = 16;
		TextureSize = FVector2D::ZeroVector;
		CombineTexture = nullptr;
		Textures = TArray<UTexture2D*>();
		MaterialInst = nullptr;
		UnlitMaterialInst = nullptr;
		TransMaterialInst = nullptr;
	}

	FORCEINLINE FVoxelRenderData(UMaterialInterface* InMaterial, UMaterialInterface* InUnlitMaterial, UMaterialInterface* InTransMaterialInst, int32 InBlockPixelSize = 16) : FVoxelRenderData()
	{
		Material = InMaterial;
		UnlitMaterial = InUnlitMaterial;
		TransMaterial = InTransMaterialInst;
		PixelSize = InBlockPixelSize;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* UnlitMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* TransMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PixelSize;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector2D TextureSize;

	UPROPERTY(Transient)
	UTexture2D* CombineTexture;

	UPROPERTY(Transient)
	TArray<UTexture2D*> Textures;

	UPROPERTY(Transient)
	UMaterialInstance* MaterialInst;

	UPROPERTY(Transient)
	UMaterialInstance* UnlitMaterialInst;

	UPROPERTY(Transient)
	UMaterialInstance* TransMaterialInst;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelItem : public FAbilityItem
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(BlueprintReadWrite)
	ERightAngle Angle;

	UPROPERTY(BlueprintReadWrite)
	float Durability;

	UPROPERTY(BlueprintReadWrite)
	FString Data;

	UPROPERTY(BlueprintReadOnly)
	AVoxelChunk* Owner;

	UPROPERTY(BlueprintReadOnly)
	AVoxelAuxiliary* Auxiliary;

	FSaveData* AuxiliaryData;

	UPROPERTY(BlueprintReadOnly)
	bool bGenerated;

public:
	static FVoxelItem Empty;
	static FVoxelItem Unknown;

public:
	FVoxelItem()
	{
		Index = FIndex::ZeroIndex;
		Angle = ERightAngle::RA_0;
		Durability = 1.f;
		Data = TEXT("");
		Owner = nullptr;
		Auxiliary = nullptr;
		AuxiliaryData = nullptr;
		bGenerated = false;
	}
		
	FVoxelItem(const FAbilityItem& InAbilityItem) : FAbilityItem(InAbilityItem)
	{
		Index = FIndex::ZeroIndex;
		Angle = ERightAngle::RA_0;
		Durability = 1.f;
		Data = TEXT("");
		Owner = nullptr;
		Auxiliary = nullptr;
		AuxiliaryData = nullptr;
		bGenerated = false;
	}
	
	FVoxelItem(const FVoxelItem& InVoxelItem, int32 InCount) : FAbilityItem(InVoxelItem, InCount)
	{
		Index = FIndex::ZeroIndex;
		Angle = ERightAngle::RA_0;
		Durability = 1.f;
		Data = TEXT("");
		Owner = nullptr;
		Auxiliary = nullptr;
		AuxiliaryData = nullptr;
		bGenerated = false;
	}

	FVoxelItem(const FPrimaryAssetId& InID, FIndex InIndex = FIndex::ZeroIndex, AVoxelChunk* InOwner = nullptr, const FString& InData = TEXT(""));

	FVoxelItem(EVoxelType InVoxelType, FIndex InIndex = FIndex::ZeroIndex, AVoxelChunk* InOwner = nullptr, const FString& InData = TEXT(""));

	FVoxelItem(const FString& InSaveData, bool bWorldSpace = false);

public:
	void OnGenerate(IVoxelAgentInterface* InAgent = nullptr);

	void OnDestroy(IVoxelAgentInterface* InAgent = nullptr);

	void RefreshData(bool bOrigin = false);

	void RefreshData(UVoxel& InVoxel, bool bOrigin = false);

	FString ToSaveData(bool bWorldSpace = false, bool bRefresh = false) const;

public:
	virtual bool IsValid() const override;

	virtual bool IsEmpty() const override;

	virtual bool IsUnknown() const;

	virtual bool EqualIndex(const FVoxelItem& InItem) const;

	bool IsReplaceable(const FVoxelItem& InVoxelItem = FVoxelItem::Empty) const;

	FVoxelItem ReplaceID(const FPrimaryAssetId& InID) const;

	bool IsMain() const;

	FVoxelItem& GetMain() const;

	FVoxelItem& GetPart(FIndex InIndex) const;

	TArray<FVoxelItem> GetParts() const;

	EVoxelType GetVoxelType() const;

	FVector GetRange(bool bIncludeAngle = true, bool bIncludeDirection = false) const;

	FIndex GetIndex(bool bWorldSpace = true) const;

	FVector GetLocation(bool bWorldSpace = true) const;

	template<class T>
	T& GetVoxel() const
	{
		return static_cast<T&>(GetVoxel());
	}

	UVoxel& GetVoxel() const;

	AVoxelChunk* GetOwner() const;

	template<class T>
	T& GetVoxelData(bool bEnsured = true) const
	{
		return static_cast<T&>(GetVoxelData(bEnsured));
	}

	UVoxelData& GetVoxelData(bool bEnsured = true) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelHitResult
{
	GENERATED_BODY()

public:
	FVoxelHitResult()
	{
		VoxelItem = FVoxelItem();
		Point = FVector();
		Normal = FVector();
	}
	
	FVoxelHitResult(const FHitResult& InHitResult);
	
	FVoxelHitResult(const FVoxelItem& InVoxelItem, FVector InPoint = FVector::ZeroVector, FVector InNormal = FVector::ZeroVector);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector Point;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector Normal;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVoxelItem VoxelItem;

public:
	bool IsValid() const;

	template<class T>
	T& GetVoxel() const
	{
		return static_cast<T&>(GetVoxel());
	}

	UVoxel& GetVoxel() const;

	AVoxelChunk* GetChunk() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelAuxiliarySaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelAuxiliarySaveData()
	{
		VoxelItem = FVoxelItem();
		VoxelScope = EVoxelScope::None;
		InventoryData = FInventorySaveData();
	}

	FORCEINLINE FVoxelAuxiliarySaveData(const FVoxelItem& InVoxelItem, EVoxelScope InVoxelScope = EVoxelScope::None) : FVoxelAuxiliarySaveData()
	{
		VoxelItem = InVoxelItem;
		VoxelScope = InVoxelScope;
	}

public:
	UPROPERTY()
	FVoxelItem VoxelItem;

	UPROPERTY()
	EVoxelScope VoxelScope;

	UPROPERTY(BlueprintReadWrite)
	FInventorySaveData InventoryData;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		VoxelItem.MakeSaved();
		InventoryData.MakeSaved();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelPrefabSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelPrefabSaveData()
	{
		VoxelDatas = TEXT("");
	}

public:
	UPROPERTY(BlueprintReadWrite)
	FString VoxelDatas;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelTopography
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelTopography()
	{
		Index = FIndex::ZeroIndex;
		Height = 0;
		Temperature = 0.f;
		Humidity = 0.f;
		BiomeType = EVoxelBiomeType::None;
	}

	FORCEINLINE FVoxelTopography(const FString& InSaveData);

public:
	FString ToSaveData() const;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIndex Index;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Height;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Temperature;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Humidity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EVoxelBiomeType BiomeType;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelChunkSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelChunkSaveData()
	{
		Index = FIndex::ZeroIndex;
		VoxelDatas = TEXT("");
		TopographyDatas = TEXT("");
		PickUpDatas = TArray<FPickUpSaveData>();
		AuxiliaryDatas = TArray<FVoxelAuxiliarySaveData>();
		bGenerated = false;
	}

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FString VoxelDatas;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FString TopographyDatas;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FPickUpSaveData> PickUpDatas;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FVoxelAuxiliarySaveData> AuxiliaryDatas;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	bool bGenerated;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
		for(auto& Iter : PickUpDatas)
		{
			Iter.MakeSaved();
		}
		for(auto& Iter : AuxiliaryDatas)
		{
			Iter.MakeSaved();
		}
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelRandomData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelRandomData()
	{
		RandomRate = 0.f;
		VoxelTypes = TArray<EVoxelType>();
	}

	FORCEINLINE FVoxelRandomData(float InRandomRate, const TArray<EVoxelType>& InVoxelTypes)
	{
		RandomRate = InRandomRate;
		VoxelTypes = InVoxelTypes;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RandomRate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<EVoxelType> VoxelTypes;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelWorldBasicSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelWorldBasicSaveData()
	{
		BlockSize = 80.f;
		
		ChunkSize = FVector2D(16.f);
		
		WorldSize = FVector2D(-1.f);
		WorldRange = FVector2D(15.f);

		SeaLevel = 32;
		SkyHeight = 100;

		RenderDatas = TMap<EVoxelNature, FVoxelRenderData>();

		IconMat = nullptr;

		SceneData = FSceneModuleSaveData();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BlockSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D ChunkSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D WorldSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D WorldRange;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SeaLevel;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SkyHeight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<EVoxelNature, FVoxelRenderData> RenderDatas;
				
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UMaterialInterface* IconMat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSceneModuleSaveData SceneData;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		SceneData.MakeSaved();
	}
	
	FORCEINLINE FVector2D GetChunkRealSize() const
	{
		return ChunkSize * BlockSize;
	}

	FORCEINLINE FVector2D GetWorldSize() const
	{
		return FVector2D(WorldSize.X != -1.f ? WorldSize.X : WorldRange.X * 2.f, WorldSize.Y != -1.f ? WorldSize.Y : WorldRange.Y * 2.f);
	}

	FORCEINLINE FVector2D GetWorldRealSize() const
	{
		return GetWorldSize() * ChunkSize * BlockSize;
	}

	FORCEINLINE float GetWorldRealHeight() const
	{
		return SkyHeight * BlockSize;
	}

	FORCEINLINE FVector GetBlockSizedNormal(FVector InNormal, float InLength = 0.25f) const
	{
		return BlockSize * InNormal * InLength;
	}

	FORCEINLINE FVoxelRenderData& GetRenderData(EVoxelNature InVoxelNature)
	{
		return RenderDatas[InVoxelNature];
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
	
	virtual bool IsChunkGenerated(FIndex InChunkIndex)
	{
		if(const auto ChunkData = GetChunkData(InChunkIndex))
		{
			return ChunkData->bGenerated;
		}
		return false;
	}
	
	virtual bool IsChunkHasVoxelData(FIndex InChunkIndex)
	{
		if(const auto ChunkData = GetChunkData(InChunkIndex))
		{
			return !ChunkData->VoxelDatas.IsEmpty();
		}
		return false;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelModuleSaveData : public FVoxelWorldSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelModuleSaveData()
	{
		ChunkDatas = TMap<FVector, FVoxelChunkSaveData>();
	}
	
	FORCEINLINE FVoxelModuleSaveData(const FVoxelWorldBasicSaveData& InBasicSaveData) : FVoxelWorldSaveData(InBasicSaveData)
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
		for(auto& Iter : ChunkDatas)
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
		return ChunkDatas.Find(InChunkIndex.ToVector());
	}

	virtual void SetChunkData(FIndex InChunkIndex, FVoxelChunkSaveData* InChunkData) override
	{
		ChunkDatas.Emplace(InChunkIndex.ToVector(), InChunkData->CastRef<FVoxelChunkSaveData>());
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelChunkQueue
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	bool bAsync;

	UPROPERTY(EditAnywhere)
	int32 Speed;

	UPROPERTY(VisibleAnywhere)
	TArray<FIndex> Queue;
		
	TArray<FVoxelChunkQueueThread*> Threads;

	FORCEINLINE FVoxelChunkQueue()
	{
		bAsync = false;
		Speed = 100;
		Queue = TArray<FIndex>();
		Threads = TArray<FVoxelChunkQueueThread*>();
	}

	FORCEINLINE FVoxelChunkQueue(bool bInAsync, int32 InSpeed)
	{
		bAsync = bInAsync;
		Speed = InSpeed;
		Queue = TArray<FIndex>();
		Threads = TArray<FVoxelChunkQueueThread*>();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelChunkQueues
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FVoxelChunkQueue> Queues;

	UPROPERTY(VisibleAnywhere)
	int32 Stage;

	FORCEINLINE FVoxelChunkQueues()
	{
		Queues = TArray<FVoxelChunkQueue>();
		Stage = 0;
	}

	FORCEINLINE FVoxelChunkQueues(const TArray<FVoxelChunkQueue>& InQueues)
	{
		Queues = InQueues;
		Stage = 0;
	}
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "AsyncTasks/AsyncTask_ChunkQueue.h"
#include "Common/CommonTypes.h"
#include "Math/MathTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"

#include "VoxelModuleTypes.generated.h"

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
	Preview
};

UENUM(BlueprintType)
enum class EVoxelWorldState : uint8
{
	None,
	Spawn,
	Destroy,
	Generate,
	MapLoad,
	MapBuild,
	MeshBuild
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
	Chest, // ?
	Furnace,
	Crafting_Table, // ?
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
enum class EVoxelTreePart : uint8
{
	None,
	Root,
	Bole,
	Leaves
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
 * 像素声音类型
 */
UENUM(BlueprintType)
enum class EVoxelSoundType : uint8
{
	// 生成
	Generate,
	// 销毁
	Destroy,
	// 脚步
	Footstep,
	// 交互1
	Interact1,
	// 交互1
	Interact2,
	// 交互3
	Interact3,
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

/**
 * 体术交互选项
 */
UENUM(BlueprintType)
enum class EVoxelInteractAction : uint8
{
	// 无
	None = EInteractAction::None UMETA(DisplayName="无"),
	// 打开
	Open = EInteractAction::Custom1 UMETA(DisplayName="打开"),
	// 关闭
	Close = EInteractAction::Custom2 UMETA(DisplayName="关闭")
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
	FVoxelItem()
	{
		Index = FIndex::ZeroIndex;
		Angle = ERightAngle::RA_0;
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
		Data = TEXT("");
		Owner = nullptr;
		Auxiliary = nullptr;
		AuxiliaryData = nullptr;
		bGenerated = false;
	}

	FVoxelItem(const FPrimaryAssetId& InID, FIndex InIndex = FIndex::ZeroIndex, AVoxelChunk* InOwner = nullptr, const FString& InData = TEXT(""));

	FVoxelItem(EVoxelType InVoxelType, FIndex InIndex = FIndex::ZeroIndex, AVoxelChunk* InOwner = nullptr, const FString& InData = TEXT(""));

	FVoxelItem(const FString& InSaveData);

public:
	void OnGenerate(IVoxelAgentInterface* InAgent = nullptr);

	void OnDestroy(IVoxelAgentInterface* InAgent = nullptr);

	void RefreshData(bool bOrigin = false);

	void RefreshData(UVoxel& InVoxel, bool bOrigin = false);

	FString ToSaveData(bool bRefresh = false) const;

public:
	virtual bool IsValid() const override;

	bool IsUnknown() const;

	bool IsReplaceable(const FVoxelItem& InVoxelItem = FVoxelItem::Empty) const;

	FVoxelItem ReplaceID(const FPrimaryAssetId& InID) const;

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
	T& GetVoxelData(bool bEnsured = true) const
	{
		return static_cast<T&>(GetVoxelData(bEnsured));
	}

	UVoxelData& GetVoxelData(bool bEnsured = true) const;

public:
	FORCEINLINE friend bool operator==(const FVoxelItem& A, const FVoxelItem& B)
	{
		return (A.ID == B.ID) && (A.Count == B.Count) && (A.Level == B.Level) && (A.Index == B.Index);
	}

	FORCEINLINE friend bool operator!=(const FVoxelItem& A, const FVoxelItem& B)
	{
		return (A.ID != B.ID) || (A.Count != B.Count) || (A.Level != B.Level) || (A.Index != B.Index);
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelAuxiliarySaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVoxelAuxiliarySaveData()
	{
		VoxelItem = FVoxelItem();
		MeshNature = EVoxelMeshNature::Chunk;
		InventoryData = FInventorySaveData();
	}

	FORCEINLINE FVoxelAuxiliarySaveData(const FVoxelItem& InVoxelItem, EVoxelMeshNature InMeshNature = EVoxelMeshNature::Chunk) : FVoxelAuxiliarySaveData()
	{
		VoxelItem = InVoxelItem;
		MeshNature = InMeshNature;
	}

public:
	UPROPERTY()
	FVoxelItem VoxelItem;

	UPROPERTY()
	EVoxelMeshNature MeshNature;

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
	
	FVoxelHitResult(const FVoxelItem& InVoxelItem, FVector InPoint = FVector::ZeroVector, FVector InNormal = FVector::ZeroVector);

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
struct WHFRAMEWORK_API FVoxelRenderData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* UnlitMaterial;

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

	FORCEINLINE FVoxelRenderData()
	{
		Material = nullptr;
		UnlitMaterial = nullptr;
		PixelSize = 16;
		TextureSize = FVector2D::ZeroVector;
		CombineTexture = nullptr;
		Textures = TArray<UTexture2D*>();
		MaterialInst = nullptr;
		UnlitMaterialInst = nullptr;
	}

	FORCEINLINE FVoxelRenderData(UMaterialInterface* InMaterial, UMaterialInterface* InUnlitMaterial, int32 InBlockPixelSize = 16) : FVoxelRenderData()
	{
		Material = InMaterial;
		UnlitMaterial = InUnlitMaterial;
		PixelSize = InBlockPixelSize;
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
	FString VoxelDatas;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FPickUpSaveData> PickUpDatas;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FVoxelAuxiliarySaveData> AuxiliaryDatas;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	bool bGenerated;

	FORCEINLINE FVoxelChunkSaveData()
	{
		Index = FIndex::ZeroIndex;
		VoxelDatas = TEXT("");
		PickUpDatas = TArray<FPickUpSaveData>();
		AuxiliaryDatas = TArray<FVoxelAuxiliarySaveData>();
		bGenerated = false;
	}

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
		BedrockHeight = 0.02f;
		WaterHeight = 0.35f;
		
		PlainScale = FVector(0.001f, 0.001f, 0.25f);
		MountainScale = FVector(0.01f, 0.01f, 0.3f);
		BlobScale = FVector(0.05f, 0.05f, 0.2f);
		StoneScale = FVector(0.05f, 0.05f, 0.15f);
		SandScale = FVector(0.03f, 0.03f, 0.25f);
		PlantScale = FVector4(0.8f, 0.8f, 0.5f, 0.08f);
		TreeScale = FVector4(0.01f, 0.01f, 0.5f, 0.005f);

		RenderDatas = TMap<EVoxelTransparency, FVoxelRenderData>();

		SceneData = FSceneModuleSaveData();
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
	FVector BlobScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector StoneScale;
				
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector SandScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector4 PlantScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector4 TreeScale;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<EVoxelTransparency, FVoxelRenderData> RenderDatas;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSceneModuleSaveData SceneData;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		SceneData.MakeSaved();
	}
	
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
		
	TArray<FAsyncTask<AsyncTask_ChunkQueue>*> Tasks;

	FORCEINLINE FVoxelChunkQueue()
	{
		bAsync = false;
		Speed = 100;
		Queue = TArray<FIndex>();
		Tasks = TArray<FAsyncTask<AsyncTask_ChunkQueue>*>();
	}

	FORCEINLINE FVoxelChunkQueue(bool bInAsync, int32 InSpeed)
	{
		bAsync = bInAsync;
		Speed = InSpeed;
		Queue = TArray<FIndex>();
		Tasks = TArray<FAsyncTask<AsyncTask_ChunkQueue>*>();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelChunkQueues
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FVoxelChunkQueue> Queues;

	FORCEINLINE FVoxelChunkQueues()
	{
		Queues = TArray<FVoxelChunkQueue>();
	}

	FORCEINLINE FVoxelChunkQueues(const TArray<FVoxelChunkQueue>& InQueues)
	{
		Queues = InQueues;
	}
};

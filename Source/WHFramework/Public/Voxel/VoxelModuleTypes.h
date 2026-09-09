#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Asset/AssetModuleTypes.h"
#include "Common/CommonModuleTypes.h"
#include "Math/MathTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"
#include "Threads/VoxelChunkQueueThread.h"

#include "VoxelModuleTypes.generated.h"

class UVoxelGenerator;
class IVoxelAgentInterface;
class UVoxelData;
class UVoxelChunk;
class AVoxelAuxiliary;
class UVoxel;
class AActor;
class UTexture2D;

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
	MapLoading,
	MapBuilding,
	MeshSpawning,
	MeshBuilding,
	Generating,
	Unloading
};

/**
 * ????????
 */
UENUM(BlueprintType)
enum class EVoxelType : uint8
{
	Empty = 0, //??
	Unknown = 1,
	Bedrock = 2, //????
	Dirt = 3, //????
	Red_Brick = 4, //???
	Stone = 5, //??
	Cobble_Stone = 6, //??
	Stone_Brick = 7, //??
	Stone_Stair = 8, //?????
	Sand = 9, //???
	Sand_Stone = 10, //??
	Glass = 11, //????
	Grass = 12, //???
	Snow = 13, //???
	Ice = 14, //????
	Coal_Ore = 15, //??
	Iron_Ore = 16, //??
	Gold_Ore = 17, //??
	Emerald_Ore = 18, //??
	Diamond_Ore = 19, //??
	Oak = 20, //???
	Oak_Leaves = 21, //?????
	Oak_Sapling = 51, // 橡树苗
	Oak_Plank = 22, //?????
	Oak_Stair = 23, //?????
	Oak_Door = 24, //?????
	Oak_Door_Upper = 25, //?????
	Birch = 26, //?????
	Birch_Leaves = 27, //???????
	Birch_Sapling = 52, // 白桦树苗
	Birch_Plank = 28, //???????
	Birch_Stair = 29, //???????
	Birch_Door = 30, //???????
	Birch_Door_Upper = 31, //???????
	Bed = 32, //?????
	Bed_Upper = 33, //?????
	Furnace = 34,
	Crafting_Table = 35, // ?
	Chest = 36, // ?
	Torch = 37, //???
	Water = 38, // ?
	Tall_Grass = 39, //?????
	Flower_Allium = 40,
	Flower_Blue_Orchid = 41,
	Flower_Dandelion = 42,
	Flower_Houstonia = 43,
	Flower_Oxeye_Daisy = 44,
	Flower_Paeonia = 45,
	Flower_Rose = 46,
	Flower_Tulip_Orange = 47,
	Flower_Tulip_Pink = 48,
	Flower_Tulip_Red = 49,
	Flower_Tulip_White = 50,
	Custom1 = 100,
	Custom2 = 101,
	Custom3 = 102,
	Custom4 = 103,
	Custom5 = 104,
	Custom6 = 105,
	Custom7 = 106,
	Custom8 = 107,
	Custom9 = 108,
	Custom10 = 109
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

namespace GameplayTags
{
	////////////////////////////////////////////////////
	// Voxel_Interaction_Option
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Voxel_Interaction_Option_Interact);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Voxel_Interaction_Option_UnInteract);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Voxel_Interaction_Option_Open);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Voxel_Interaction_Option_Close);
}

UENUM(BlueprintType)
enum class EVoxelBiomeType: uint8
{
	None, // 无
	Snow, // 冰原
	Green, // 绿地
	Dry, // 荒地
	Stone, // 石地
	Desert, // 沙漠
	Ocean, // 海洋
	River, // 河流
	Plains, // 平原
	Forest, // 森林
	Taiga, // 针叶林
	Savanna, // 热带草原
	Swamp, // 沼泽
	Mountains, // 山地
	Badlands, // 恶地
	Hills // 丘陵
};

UENUM(BlueprintType)
enum class EVoxelRegionType : uint8
{
	None, // 区域
	Wilderness, // 荒野
	Plain, // 平原
	Mountain, // 山脉
	Hills, // 丘陵
	River, // 河流
	Lake, // 湖泊
	Ocean, // 海域
	Town, // 小镇
	Building, // 建筑
	Desert, // 沙漠
	Icefield // 冰原
};

UENUM(BlueprintType)
enum class EVoxelAreaType : uint8
{
	Continent, // 大陆
	Town, // 小镇
	Building // 建筑
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

USTRUCT()
struct WHFRAMEWORK_API FVoxelMapCell
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTexture2D> Texture = nullptr;

	UPROPERTY()
	FVector2D UVCorner = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D UVSpan = FVector2D::UnitVector;

	UPROPERTY()
	ERightAngle Angle = ERightAngle::RA_0;

	UPROPERTY()
	int32 Height = INDEX_NONE;
};

USTRUCT()
struct WHFRAMEWORK_API FVoxelMapChunk
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D Origin = FVector2D::ZeroVector;

	UPROPERTY()
	FIntPoint Size = FIntPoint::ZeroValue;

	UPROPERTY()
	float CellSize = 0.f;

	UPROPERTY()
	TArray<FVoxelMapCell> Cells;
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

/** 体素液体状态 */
struct WHFRAMEWORK_API FVoxelLiquidState
{
	static constexpr uint8 MaxLevel = 7;

	uint8 Level;
	bool bFalling;

	FVoxelLiquidState(const FString& InData = TEXT(""));
	FVoxelLiquidState(uint8 InLevel, bool bInFalling = false);

	bool IsSource() const { return Level == 0 && !bFalling; }
	bool IsFalling() const { return bFalling; }
	uint8 GetLevel() const { return Level; }
	float GetHeight() const;
	FString ToData() const;
};

/** 体素液体快照 */
struct WHFRAMEWORK_API FVoxelLiquidSnapshot
{
	EVoxelType VoxelType = EVoxelType::Unknown;
	FString Data;
	bool bGenerated = false;
	bool bCanFlowThrough = false;
};

/** 连续存储的体素液体快照 */
struct WHFRAMEWORK_API FVoxelLiquidSnapshotGrid
{
	FIndex MinIndex;
	FIndex Size;
	TArray<FVoxelLiquidSnapshot> Snapshots;

	FVoxelLiquidSnapshotGrid(FIndex InMinIndex, FIndex InSize)
		: MinIndex(InMinIndex), Size(InSize)
	{
		Snapshots.SetNum(Size.X * Size.Y * Size.Z);
	}

	bool IsValidIndex(FIndex InIndex) const
	{
		return InIndex.X >= MinIndex.X && InIndex.X < MinIndex.X + Size.X &&
			InIndex.Y >= MinIndex.Y && InIndex.Y < MinIndex.Y + Size.Y &&
			InIndex.Z >= MinIndex.Z && InIndex.Z < MinIndex.Z + Size.Z;
	}

	int32 GetLinearIndex(FIndex InIndex) const
	{
		const FIndex LocalIndex = InIndex - MinIndex;
		return (LocalIndex.Z * Size.Y + LocalIndex.Y) * Size.X + LocalIndex.X;
	}

	FVoxelLiquidSnapshot* Find(FIndex InIndex)
	{
		return IsValidIndex(InIndex) ? &Snapshots[GetLinearIndex(InIndex)] : nullptr;
	}

	const FVoxelLiquidSnapshot* Find(FIndex InIndex) const
	{
		return IsValidIndex(InIndex) ? &Snapshots[GetLinearIndex(InIndex)] : nullptr;
	}

	FIndex GetIndex(int32 InLinearIndex) const
	{
		const int32 PlaneSize = Size.X * Size.Y;
		const int32 Z = InLinearIndex / PlaneSize;
		const int32 PlaneIndex = InLinearIndex - Z * PlaneSize;
		return MinIndex + FIndex(PlaneIndex % Size.X, PlaneIndex / Size.X, Z);
	}

};

/** 体素液体更新 */
struct WHFRAMEWORK_API FVoxelLiquidUpdate
{
	FString Data;
	bool bRemove = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelItem : public FSaveData
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(BlueprintReadWrite)
	FPrimaryAssetId ID;

	UPROPERTY(BlueprintReadWrite)
	ERightAngle Angle;

	UPROPERTY(BlueprintReadWrite)
	float Durability;

	UPROPERTY(BlueprintReadWrite)
	FString Data;

	UPROPERTY(BlueprintReadOnly)
	UVoxelChunk* Chunk;

	UPROPERTY(BlueprintReadOnly)
	AVoxelAuxiliary* Auxiliary;

	FSaveData* AuxiliaryData;

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
		Chunk = nullptr;
		Auxiliary = nullptr;
		AuxiliaryData = nullptr;
	}
		
	FVoxelItem(const FAbilityItem& InAbilityItem)
	{
		Index = FIndex::ZeroIndex;
		ID = InAbilityItem.ID;
		Angle = ERightAngle::RA_0;
		Durability = 1.f;
		Data = TEXT("");
		Chunk = nullptr;
		Auxiliary = nullptr;
		AuxiliaryData = nullptr;
	}

	FVoxelItem(const FPrimaryAssetId& InID, FIndex InIndex = FIndex::ZeroIndex, UVoxelChunk* InOwner = nullptr, const FString& InData = TEXT(""));

	FVoxelItem(EVoxelType InVoxelType, FIndex InIndex = FIndex::ZeroIndex, UVoxelChunk* InOwner = nullptr, const FString& InData = TEXT(""));

	FVoxelItem(const FString& InSaveData, bool bWorldSpace = false);

public:
	void OnGenerate(IVoxelAgentInterface* InAgent = nullptr);

	void OnDestroy(IVoxelAgentInterface* InAgent = nullptr);

	void RefreshData(bool bOrigin = false);

	void RefreshData(UVoxel& InVoxel, bool bOrigin = false);

	FString ToSaveData(bool bWorldSpace = false, bool bRefresh = false) const;

public:
	virtual bool IsValid() const override;

	virtual bool IsEmpty() const;

	virtual bool IsUnknown() const;

	FORCEINLINE bool Equal(const FVoxelItem& InItem) const
	{
		return InItem.ID == ID;
	}

	FORCEINLINE friend bool operator==(const FVoxelItem& A, const FVoxelItem& B)
	{
		return A.Equal(B);
	}

	FORCEINLINE friend bool operator!=(const FVoxelItem& A, const FVoxelItem& B)
	{
		return !A.Equal(B);
	}

	FORCEINLINE bool EqualIndex(const FVoxelItem& InItem) const
	{
		return InItem.GetIndex() == GetIndex();
	}

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
	T& GetData(bool bEnsured = true) const
	{
		return static_cast<T&>(GetData(bEnsured));
	}

	UVoxelData& GetData(bool bEnsured = true) const;

	template<class T>
	T& GetVoxel() const
	{
		return static_cast<T&>(GetVoxel());
	}

	UVoxel& GetVoxel() const;
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
		WaterHeight = INDEX_NONE;
		Temperature = 0.f;
		Humidity = 0.f;
		BiomeType = EVoxelBiomeType::None;
		RegionType = EVoxelRegionType::None;
		Continentalness = 0.f;
		Erosion = 0.f;
		PeaksAndValleys = 0.f;
		Fertility = 0.f;
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
	int32 WaterHeight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Temperature;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Humidity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EVoxelBiomeType BiomeType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EVoxelRegionType RegionType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Continentalness;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Erosion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float PeaksAndValleys;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Fertility;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelHitResult
{
	GENERATED_BODY()

public:
	FVoxelHitResult()
	{
		Actor = nullptr;
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
	AActor* Actor;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVoxelItem VoxelItem;

public:
	bool IsValid() const;

	bool IsGround() const;

	template<class T>
	T& GetVoxel() const
	{
		return static_cast<T&>(GetVoxel());
	}

	UVoxel& GetVoxel() const;

	UVoxelChunk* GetChunk() const;
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
		bChanged = false;
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
	bool bChanged;

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
		WorldRange = FVector2D(30.f);

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
	
	virtual bool IsChunkDataChanged(FIndex InChunkIndex)
	{
		if(const auto ChunkData = GetChunkData(InChunkIndex))
		{
			return ChunkData->bChanged;
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
	UPROPERTY(VisibleAnywhere)
	bool bAsync;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 Speed;

	UPROPERTY(VisibleAnywhere)
	TArray<FIndex> Queue;

	UPROPERTY(EditAnywhere, Instanced)
	TArray<UVoxelGenerator*> Generators;

	TSet<FIndex> Indices;
	bool bSortRequired;
		
	FORCEINLINE FVoxelChunkQueue()
	{
		bAsync = false;
		Speed = 100;
		Queue = TArray<FIndex>();
		Generators = TArray<UVoxelGenerator*>();
		Indices = TSet<FIndex>();
		bSortRequired = false;
	}

	FORCEINLINE FVoxelChunkQueue(bool bInAsync, int32 InSpeed, const TArray<UVoxelGenerator*>& InGenerators = { })
	{
		bAsync = bInAsync;
		Speed = InSpeed;
		Queue = TArray<FIndex>();
		Generators = InGenerators;
		Indices = TSet<FIndex>();
		bSortRequired = false;
	}

	bool Add(FIndex InIndex)
	{
		if(Indices.Contains(InIndex)) return false;
		Indices.Add(InIndex);
		Queue.Add(InIndex);
		bSortRequired = true;
		return true;
	}

	bool Remove(FIndex InIndex)
	{
		if(Indices.Remove(InIndex) == 0) return false;
		Queue.Remove(InIndex);
		return true;
	}

	void RemoveBatch(const TSet<FIndex>& InIndices)
	{
		Queue.RemoveAll([&InIndices](const FIndex& Index) { return InIndices.Contains(Index); });
		for(const FIndex& Index : InIndices) Indices.Remove(Index);
	}

	void RemoveFront(int32 InCount)
	{
		const int32 Count = FMath::Min(InCount, Queue.Num());
		for(int32 Index = 0; Index < Count; ++Index) Indices.Remove(Queue[Index]);
		Queue.RemoveAt(0, Count, EAllowShrinking::No);
	}

	void Reset()
	{
		Queue.Reset();
		Indices.Reset();
		bSortRequired = false;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelChunkQueues
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (EditFixedSize))
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

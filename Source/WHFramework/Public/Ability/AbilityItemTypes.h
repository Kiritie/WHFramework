#pragma once

#include "CoreMinimal.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Ability/Inventory/InventoryItemTypes.h"

#include "AbilityItemTypes.generated.h"

UENUM(BlueprintType)
enum class EAbilityItemType : uint8
{
	// 无
	None,
	// 货币
	Coin,
	// 体素
	Voxel,
	// 材料
	Raw,
	// 道具
	Prop,
	// 装备
	Equip,
	// 技能
	Skill,
	// 物体
	Actor,
	// 生命体
	Vitality,
	// 对象
	Pawn,
	// 角色
	Character,
	// 混杂
	Misc,
	// 物件
	Object
};

/**
* 装备模式
*/
UENUM(BlueprintType)
enum class EAbilityEquipMode : uint8
{
	// 无
	None,
	// 被动
	Passive,
	// 主动
	Initiative
};

/**
* 技能模式
*/
UENUM(BlueprintType)
enum class EAbilitySkillMode : uint8
{
	// 无
	None,
	// 被动
	Passive,
	// 主动
	Initiative
};

/**
* 物品稀有度
*/
UENUM(BlueprintType)
enum class EAbilityItemRarity : uint8
{
	// 无
	None,
	// 普通
	Common,
	// 非凡
	Uncommon,
	// 稀有
	Rare,
	// 史诗
	Epic,
	// 传奇
	Legendary,
	// 神话
	Mythical
};


USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItemSpawnParameter : public FWHActorSpawnParameter
{
	GENERATED_BODY()

public:
	FAbilityItemSpawnParameter() = default;

	FAbilityItemSpawnParameter(AActor* InOwner, const FPrimaryAssetId& InItemId, int32 InLevel)
		: ItemId(InItemId), Level(InLevel)
	{
		WorldContext = InOwner;
		Owner = InOwner;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 0;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItems
{
	GENERATED_BODY()

public:
	FAbilityItems()
	{
		Items = TArray<FAbilityItem>();
	}

	FAbilityItems(int32 InNum)
	{
		Items = TArray<FAbilityItem>();
		Items.SetNum(InNum);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "ItemId"))
	TArray<FAbilityItem> Items;
};

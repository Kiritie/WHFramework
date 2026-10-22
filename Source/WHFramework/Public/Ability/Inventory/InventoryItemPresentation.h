#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "InventoryItemPresentation.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EInventoryPickupVisual : uint8
{
    None,
    Icon,
    StaticMesh,
    SkeletalMesh
};

/** 只描述世界表现；物品数量、属性和所有权仍来自库存快照。 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryPickupPresentation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryPickupVisual Visual = EInventoryPickupVisual::Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> StaticMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UMaterialInterface> MaterialOverride;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform LocalTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="8", ClampMax="512"))
    float IconSize = 48.f;
};

UENUM(BlueprintType)
enum class EInventoryPersistentBuffMode : uint8
{
    None,
    IndependentInstances,
    SharedDuration,
    IndependentLayers
};

/** 药剂事务领域的叠层规则。None 使用效果的缺省叠加方式；战斗周期效果不经过此投影。 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryPersistentBuffPolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> EffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryPersistentBuffMode Mode = EInventoryPersistentBuffMode::None;

    // 0 表示使用 GE 的上限；无限上限在持久药剂领域约束为 64。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="64"))
    int32 MaximumStacks = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bRefreshSharedDuration = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bReplaceOldestAtLimit = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bRemoveOnDeath = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bDispellable = true;
};

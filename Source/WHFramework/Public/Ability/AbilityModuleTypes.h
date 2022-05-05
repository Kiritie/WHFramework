#pragma once

#include "Abilities/GameplayAbilityTypes.h"
#include "Asset/Primary/PrimaryAssetManager.h"

#include "AbilityModuleTypes.generated.h"

class UItemDataBase;
class AAbilitySkillBase;
class AAbilityEquipBase;

/**
 * ����Ŀ�����
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin))
class WHFRAMEWORK_API UTargetType : public UObject
{
	GENERATED_BODY()

public:
	UTargetType() {}

	/** ��ȡĿ�� */
	UFUNCTION(BlueprintNativeEvent)
	void GetTargets(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const;
};

/**
 * ����Ŀ��_ʹ��ӵ����
 */
UCLASS(NotBlueprintable)
class WHFRAMEWORK_API UTargetType_UseOwner : public UTargetType
{
	GENERATED_BODY()

public:
	UTargetType_UseOwner() {}
	
	virtual void GetTargets_Implementation(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const override;
};

/**
 * ����Ŀ��_ʹ���¼�����
 */
UCLASS(NotBlueprintable)
class WHFRAMEWORK_API UTargetType_UseEventData : public UTargetType
{
	GENERATED_BODY()

public:
	UTargetType_UseEventData() {}

	virtual void GetTargets_Implementation(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const override;
};

/**
 * GameplayEffect????
 */
USTRUCT(BlueprintType)
struct FGameplayEffectContainer
{
	GENERATED_BODY()

public:
    FGameplayEffectContainer() {}

public:
    /** ??????? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TSubclassOf<UTargetType> TargetType;

    /** ??????????GameplayEffect?б? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TArray<TSubclassOf<UGameplayEffect>> TargetGameplayEffectClasses;
};

/**
 * GameplayEffect?????淶
 */
USTRUCT(BlueprintType)
struct FGameplayEffectContainerSpec
{
    GENERATED_BODY()

public:
    FGameplayEffectContainerSpec() {}

public:
    /** ??????? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    FGameplayAbilityTargetDataHandle TargetData;

    /** ?????????GameplayEffect?б? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TArray<FGameplayEffectSpecHandle> TargetGameplayEffectSpecs;
    
    /** ???????Ч?????*/
    bool HasValidTargets() const;

    /** ???????Ч??Effect*/
    bool HasValidEffects() const;

    /** ???????????????? */
    void AddTargets(const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors);
};

/**
 * ???????????
 */
UENUM(BlueprintType)
enum class EAbilityCostType : uint8
{
	// ??
	None,
	// ?????
	Health,
	// ????
	Mana,
	// ?????
	Stamina
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	EAbilityCostType CostType;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float Cost;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float Cooldown;

	FORCEINLINE FAbilityInfo()
	{
		CostType = EAbilityCostType::None;
		Cost = 0.f;
		Cooldown = -1.f;
	}

	FORCEINLINE FAbilityInfo(EAbilityCostType InCostType, float InCost, float InCooldown)
	{
		CostType = InCostType;
		Cost = InCost;
		Cooldown = InCooldown;
	}
};

USTRUCT(BlueprintType)
struct FCooldownInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	float TotalTime;

	UPROPERTY(BlueprintReadOnly)
	float RemainTime;

	UPROPERTY(BlueprintReadOnly)
	bool bCooldowning;

	FCooldownInfo()
	{
		TotalTime = 0.f;
		RemainTime = 0.f;
		bCooldowning = false;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityData : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId AbilityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AbilityLevel;

	FGameplayAbilitySpecHandle AbilityHandle;

	FORCEINLINE FAbilityData()
	{
		AbilityLevel = 1;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}

public:
	template<class T>
	T& GetItemData() const
	{
		return static_cast<T&>(GetItemData());
	}

	UItemDataBase& GetItemData() const;
};

/**
 * ???????
 */
UENUM(BlueprintType)
enum class EDamageType : uint8
{
	// ????
	Any,
	// ????
	Physics,
	// ???
	Magic
};

/**
* 技能类型
*/
UENUM(BlueprintType)
enum class ESkillType : uint8
{
	// 无
	None,
	// 近战
	Melee,
	// ???????
	Remote
};

/**
* 技能模式
*/
UENUM(BlueprintType)
enum class ESkillMode : uint8
{
	// 无
	None,
	// 被动
	Passive,
	// 主动
	Initiative
};

/**
 * 交互选项
 */
UENUM(BlueprintType)
enum class EInteractAction : uint8
{
	// 无
	None UMETA(DisplayName="无"),
	// 复活
	Revive UMETA(DisplayName="复活"),
	// 喂食
	Feed UMETA(DisplayName="喂食"),
	// 骑乘
	Ride UMETA(DisplayName="骑乘"),
	// 取消骑乘
	UnRide UMETA(DisplayName="取消骑乘"),
	// 战斗
	Fight UMETA(DisplayName="战斗"),
	// 对话
	Dialogue UMETA(DisplayName="对话"),
	// 交易
	Transaction UMETA(DisplayName="交易"),
	// 打开
	Open UMETA(DisplayName="打开"),
	// 关闭
	Close UMETA(DisplayName="关闭")
};

class UItemDataBase;
/**
 * ???????
 */
UENUM(BlueprintType)
enum class EAbilityItemType : uint8
{
	// ??
	None,
	// ????
	Voxel,
	// ????
	Prop,
	// ???
	Equip,
	// ????
	Skill,
	// ????
	Vitality,
	// ????
	Character
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;

	static FAbilityItem Empty;

public:
	FORCEINLINE FAbilityItem()
	{
		Count = 0;
		Level = 0;
	}
		
	FORCEINLINE FAbilityItem(const FPrimaryAssetId& InID, int32 InCount, int32 InLevel = 0)
	{
		ID = InID;
		Count = InCount;
		Level = InLevel;
	}
	
	FORCEINLINE FAbilityItem(const FAbilityItem& InItem, int InCount = -1)
	{
		ID = InItem.ID;
		Count = InCount == -1 ? InItem.Count : InCount;
		Level = InItem.Level;
	}

	virtual ~FAbilityItem() = default;

	template<class T>
	T& GetData() const
	{
		return static_cast<T&>(GetData());
	}

	UItemDataBase& GetData() const;

	FORCEINLINE virtual bool IsValid() const
	{
		return ID.IsValid();
	}

	FORCEINLINE bool EqualType(FAbilityItem InItem) const
	{
		return InItem.IsValid() && InItem.ID == ID && InItem.Level == Level;
	}

	FORCEINLINE friend bool operator==(const FAbilityItem& A, const FAbilityItem& B)
	{
		return (A.ID == B.ID) && (A.Count == B.Count) && (A.Level == B.Level);
	}

	FORCEINLINE friend bool operator!=(const FAbilityItem& A, const FAbilityItem& B)
	{
		return (A.ID != B.ID) || (A.Count != B.Count) || (A.Level != B.Level);
	}

	FORCEINLINE friend FAbilityItem operator+(FAbilityItem& A, FAbilityItem& B)
	{
		if(A.EqualType(B))
		{
			A.Count += B.Count;
		}
		return A;
	}

	FORCEINLINE friend FAbilityItem operator-(FAbilityItem& A, FAbilityItem& B)
	{
		if(A.EqualType(B))
		{
			A.Count -= B.Count;
		}
		return A;
	}

	FORCEINLINE friend FAbilityItem& operator+=(FAbilityItem& A, FAbilityItem& B)
	{
		if(A.EqualType(B))
		{
			A.Count += B.Count;
		}
		return A;
	}

	FORCEINLINE friend FAbilityItem& operator-=(FAbilityItem& A, FAbilityItem& B)
	{
		if(A.EqualType(B))
		{
			A.Count -= B.Count;
		}
		return A;
	}
};

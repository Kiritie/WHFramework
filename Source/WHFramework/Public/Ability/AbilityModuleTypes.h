#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "Abilities/GameplayAbility.h"
#include "Common/Base/WHObject.h"
#include "GameplayTagContainer.h"
#include "Asset/AssetModuleTypes.h"
#include "Common/CommonModuleTypes.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Scene/SceneModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"

#include "AbilityModuleTypes.generated.h"

class UEffectBase;
class UAbilityActorDataBase;
class UVitalityActionAbilityBase;
class UAbilityInventoryBase;
class UAbilityCharacterDataBase;
class UAbilityVitalityDataBase;
class UAbilityItemDataBase;
class AAbilityProjectileBase;
class AAbilityEquipBase;

extern WHFRAMEWORK_API FPrimaryAssetId PAID_EXP;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityActorSpawnParameter : public FWHActorSpawnParameter
{
	GENERATED_BODY()

public:
	FAbilityActorSpawnParameter() = default;

	FAbilityActorSpawnParameter(
		UObject* InWorldContext,
		const FTransform& InTransform,
		const FGuid& InActorID,
		const FPrimaryAssetId& InAssetID)
	{
		WorldContext = InWorldContext;
		Transform = InTransform;
		bOverrideActorID = true;
		ActorID = InActorID;
		AssetID = InAssetID;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId AssetID;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityProjectileSpawnParameter : public FWHActorSpawnParameter
{
	GENERATED_BODY()

public:
	FAbilityProjectileSpawnParameter() = default;

	FAbilityProjectileSpawnParameter(
		AActor* InOwner,
		const FGameplayAbilitySpecHandle& InAbilityHandle)
		: AbilityHandle(InAbilityHandle)
	{
		WorldContext = InOwner;
		Owner = InOwner;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayAbilitySpecHandle AbilityHandle;
};

DECLARE_PROPERTY_ROB_GETTER(FGameplayEffectModifierMagnitude, AttributeBasedMagnitude, FAttributeBasedFloat)

DECLARE_PROPERTY_ROB_GETTER(FGameplayEffectModifierMagnitude, CustomMagnitude, FCustomCalculationBasedFloat)

#define GET_GAMEPLAYATTRIBUTE(ClassName, PropertyName) \
	FGameplayAttribute(GET_MEMBER_PROPERTY(ClassName, PropertyName))

#define GAMEPLAYATTRIBUTE_VALUE_BASE_GETTER(PropertyName) \
	FORCEINLINE float GetBase##PropertyName() const \
	{ \
		return PropertyName.GetBaseValue(); \
	}

#define GAMEPLAYATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_BASE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	ATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_GETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_SETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_MODIFY(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_MULTIPLE(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_INITTER(ClassName, PropertyName)

#define ATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	FORCEINLINE FGameplayAttribute Get##PropertyName##Attribute() const \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		return _AttributeSet->Get##PropertyName##Attribute(); \
	}

#define ATTRIBUTE_VALUE_GETTER(ClassName, PropertyName) \
	FORCEINLINE float Get##PropertyName() const \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		return _AttributeSet->Get##PropertyName(); \
	}\
	FORCEINLINE float GetBase##PropertyName() const \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		return _AttributeSet->GetBase##PropertyName(); \
	}

#define ATTRIBUTE_VALUE_SETTER(ClassName, PropertyName) \
	FORCEINLINE void Set##PropertyName(float InValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->SetAttributeValue(_AttributeSet->Get##PropertyName##Attribute(), InValue); \
	}

#define ATTRIBUTE_VALUE_MODIFY(ClassName, PropertyName) \
	FORCEINLINE void Modify##PropertyName(float InDeltaValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->ModifyAttributeValue(_AttributeSet->Get##PropertyName##Attribute(), InDeltaValue); \
	}

#define ATTRIBUTE_VALUE_MULTIPLE(ClassName, PropertyName) \
	FORCEINLINE void Multiple##PropertyName(float InMultipleValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->MultipleAttributeValue(_AttributeSet->Get##PropertyName##Attribute(), InMultipleValue); \
	}

#define ATTRIBUTE_VALUE_INITTER(ClassName, PropertyName) \
	FORCEINLINE void Init##PropertyName(float InValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->Init##PropertyName(InValue); \
	}

#define ATTRIBUTE_DELTAVALUE_CLAMP(UserObject, PropertyName, DeltaValue) \
	DeltaValue > 0.f ? FMath::Min(DeltaValue, UserObject->GetMax##PropertyName() - UserObject->Get##PropertyName()) : FMath::Max(DeltaValue, -UserObject->Get##PropertyName())

USTRUCT()
struct WHFRAMEWORK_API FGameplayEffectContextBase : public FGameplayEffectContext
{
	GENERATED_USTRUCT_BODY()

public:

	virtual FGameplayAbilityTargetDataHandle GetTargetData()
	{
		return TargetData;
	}

	virtual void AddTargetData(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
	{
		TargetData.Append(TargetDataHandle);
	}

	/**
	* Functions that subclasses of FGameplayEffectContext need to override
	*/

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGameplayEffectContextBase::StaticStruct();
	}

	virtual FGameplayEffectContextBase* Duplicate() const override
	{
		FGameplayEffectContextBase* NewContext = new FGameplayEffectContextBase();
		*NewContext = *this;
		NewContext->AddActors(Actors);
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		// Shallow copy of TargetData, is this okay?
		NewContext->TargetData.Append(TargetData);
		return NewContext;
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

protected:
	FGameplayAbilityTargetDataHandle TargetData;
};

template<>
struct TStructOpsTypeTraits<FGameplayEffectContextBase> : public TStructOpsTypeTraitsBase2<FGameplayEffectContextBase>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true		// Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};


USTRUCT()
struct WHFRAMEWORK_API FAbilityMeshMontage
{
	GENERATED_BODY()

public:
	UPROPERTY()
	class USkeletalMeshComponent* Mesh;

	UPROPERTY()
	class UAnimMontage* Montage;

	FAbilityMeshMontage() : Mesh(nullptr), Montage(nullptr)
	{
	}

	FAbilityMeshMontage(class USkeletalMeshComponent* InMesh, class UAnimMontage* InMontage) 
		: Mesh(InMesh), Montage(InMontage)
	{
	}
};

UENUM(BlueprintType)
enum class EAbilityInputID : uint8
{
	None,
	Confirm,
	Cancel
};

/**
 * 目标类型
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin))
class WHFRAMEWORK_API UTargetType : public UWHObject
{
	GENERATED_BODY()

public:
	UTargetType() {}

	/** 获取目标 */
	UFUNCTION(BlueprintNativeEvent)
	void GetTargets(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const;
};

/**
 * 目标类型_使用自身
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
 * 目标类型_使用事件数据
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
 * 伤害处理类
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API UDamageHandle : public UWHObject
{
	GENERATED_BODY()

public:
	UDamageHandle() {}

	virtual void HandleDamage(AActor* SourceActor, AActor* TargetActor, float DamageValue, const FGameplayAttribute& DamageAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
};

/**
 * 伤害处理类
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API URecoveryHandle : public UWHObject
{
	GENERATED_BODY()

public:
	URecoveryHandle() {}

	virtual void HandleRecovery(AActor* SourceActor, AActor* TargetActor, float RecoveryValue, const FGameplayAttribute& RecoveryAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
};

/**
 * 伤害处理类
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API UInterruptHandle : public UWHObject
{
	GENERATED_BODY()

public:
	UInterruptHandle() {}

	virtual void HandleInterrupt(AActor* SourceActor, AActor* TargetActor, float InterruptDuration, const FGameplayAttribute& InterruptAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
};

/**
 * GameplayEffect容器
 */
USTRUCT(BlueprintType)
struct FGameplayEffectContainer
{
	GENERATED_BODY()

public:
    FGameplayEffectContainer() {}

public:
    /** 目标类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TSubclassOf<UTargetType> TargetType;

    /** 目标GameplayEffect类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TArray<TSubclassOf<UEffectBase>> TargetGameplayEffectClasses;
};

/**
 * GameplayEffect容器规格
 */
USTRUCT(BlueprintType)
struct FGameplayEffectContainerSpec
{
    GENERATED_BODY()

public:
    FGameplayEffectContainerSpec() {}

public:
    /** 目标数据 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    FGameplayAbilityTargetDataHandle TargetData;

    /** 目标GameplayEffect规格 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TArray<FGameplayEffectSpecHandle> TargetGameplayEffectSpecs;
    
    /** 是否有有效目标 */
    bool HasValidTargets() const;

    /** 是否有有效Effect */
    bool HasValidEffects() const;

    /** 添加目标 */
    void AddTargets(const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors);
};

/**
 * 物品类型
 */
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
	Misc
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
struct WHFRAMEWORK_API FAbilityWidgetSpawnParameter : public FSubWidgetSpawnParameter
{
	GENERATED_BODY()

public:
	FAbilityWidgetSpawnParameter() = default;

	explicit FAbilityWidgetSpawnParameter(const FAbilityItem& InItem)
		: Item(InItem)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityItem Item;
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityData : public FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;
	
	UPROPERTY(Transient)
	FGameplayAbilitySpecHandle AbilityHandle;

public:
	FAbilityData()
	{
		ID = FPrimaryAssetId();
		Level = 1;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}

	virtual bool IsValid() const override
	{
		return AbilityHandle.IsValid();
	}

	template<class T>
	T& GetData() const
	{
		return static_cast<T&>(GetData());
	}

	UPrimaryAssetBase& GetData() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVitalityActionAbilityData : public FAbilityData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UVitalityActionAbilityBase> AbilityClass;

	FORCEINLINE FVitalityActionAbilityData()
	{
		AbilityClass = nullptr;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAttributeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FGameplayAttribute Attribute;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FGameplayAttribute BaseAttribute;

	UPROPERTY(EditDefaultsOnly, Category=Capture)
	EGameplayEffectAttributeCaptureSource AttributeSource;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TEnumAsByte<EGameplayModOp::Type> ModifierOp;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float Value;

public:
	FORCEINLINE FAttributeInfo()
	{
		Attribute = FGameplayAttribute();
		BaseAttribute = FGameplayAttribute();
		AttributeSource = EGameplayEffectAttributeCaptureSource();
		ModifierOp = EGameplayModOp::Additive;
		Value = 0.f;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEffectInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FAttributeInfo> Attributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float Duration;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float Period;

public:
	FORCEINLINE FEffectInfo()
	{
		Attributes = TArray<FAttributeInfo>();
		Duration = 0.f;
		Period = 0.f;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FEffectInfo> Effects;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FGameplayAttribute CostAttribute;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float CostValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float CooldownDuration;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float CooldownRemaining;

public:
	FORCEINLINE FAbilityInfo()
	{
		Effects = TArray<FEffectInfo>();
		CostAttribute = FGameplayAttribute();
		CostValue = 0.f;
		CooldownDuration = -1.f;
		CooldownRemaining = 0.f;
	}

	FORCEINLINE bool IsCooldownning() const
	{
		return CooldownRemaining > 0.f;
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


/**
 * 查询项类型
 */


USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FRaceItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount;

	FORCEINLINE FRaceItem()
	{
		ID = FPrimaryAssetId();
		MinCount = 1;
		MaxCount = 1;
	}

	template<class T>
	T& GetData() const
	{
		return static_cast<T&>(GetData());
	}

	UAbilityItemDataBase& GetData() const;
};

/**
 * 种族数据
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FRaceDataBase : public FDataTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Detail;

	FORCEINLINE FRaceDataBase()
	{
		Name = NAME_None;
		Detail = TEXT("");
	}
};

/**
 * 种族数据
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FRaceData : public FRaceDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector NoiseScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRaceItem> Items;

	FORCEINLINE FRaceData()
	{
		NoiseScale = FVector::OneVector;
		MinLevel = 1;
		MaxLevel = 1;
		Items = TArray<FRaceItem>();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVitalityRaceData : public FRaceData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVitalityRaceData()
	{
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterRaceData : public FRaceData
{
	GENERATED_BODY()

public:
	FORCEINLINE FCharacterRaceData()
	{
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPlayerRaceData : public FRaceDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityItem> Items;

	FORCEINLINE FPlayerRaceData()
	{
		Items = TArray<FAbilityItem>();
	}
};


USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FActorSaveData : public FSceneActorSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FActorSaveData()
	{
		AssetID = FPrimaryAssetId();
		Name = NAME_None;
		Level = 1;
		InventorySnapshot = FInventorySnapshot();
		BirthTransform = FTransform::Identity;
	}

	FORCEINLINE FActorSaveData(const FSceneActorSaveData& InSceneActorSaveData) : FSceneActorSaveData(InSceneActorSaveData)
	{
		AssetID = FPrimaryAssetId();
		Name = NAME_None;
		Level = 1;
		InventorySnapshot = FInventorySnapshot();
		BirthTransform = FTransform::Identity;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId AssetID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;
	
	UPROPERTY(BlueprintReadWrite)
	FInventorySnapshot InventorySnapshot;

	UPROPERTY()
	FTransform BirthTransform;

public:
	virtual bool IsValid() const override
	{
		return AssetID.IsValid();
	}
	


	virtual void InitData(FRandomStream InRandomStream = FRandomStream());

	template<class T>
	T& GetData() const
	{
		return static_cast<T&>(GetData());
	}

	UAbilityActorDataBase& GetData() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVitalitySaveData : public FActorSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVitalitySaveData()
	{
		RaceID = NAME_None;
		ActionAbilities = TMap<FGameplayTag, FVitalityActionAbilityData>();
	}

	FORCEINLINE FVitalitySaveData(const FActorSaveData& InActorSaveData) : FActorSaveData(InActorSaveData)
	{
		RaceID = NAME_None;
		ActionAbilities = TMap<FGameplayTag, FVitalityActionAbilityData>();
	}

public:
	UPROPERTY(BlueprintReadWrite)
	FName RaceID;

	UPROPERTY()
	TMap<FGameplayTag, FVitalityActionAbilityData> ActionAbilities;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPawnSaveData : public FVitalitySaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FPawnSaveData()
	{
		
	}

	FORCEINLINE FPawnSaveData(const FVitalitySaveData& InVitalitySaveData) : FVitalitySaveData(InVitalitySaveData)
	{
		
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterSaveData : public FPawnSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FCharacterSaveData()
	{
	}

	FORCEINLINE FCharacterSaveData(const FPawnSaveData& InPawnSaveData) : FPawnSaveData(InPawnSaveData)
	{
	}
};


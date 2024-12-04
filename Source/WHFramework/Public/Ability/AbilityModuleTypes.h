#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "Abilities/GameplayAbility.h"
#include "Common/Base/WHObject.h"
#include "GameplayTagContainer.h"
#include "Asset/AssetModuleTypes.h"
#include "Common/CommonTypes.h"
#include "Scene/SceneModuleTypes.h"

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
class UWidgetAbilityInventorySlotBase;
class UAbilityInventorySlotBase;

DECLARE_PROPERTY_ROB_GETTER(FGameplayEffectModifierMagnitude, AttributeBasedMagnitude, FAttributeBasedFloat)

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
	None UMETA(DisplayName="无"),
	// 货币
	Coin UMETA(DisplayName="货币"),
	// 体素
	Voxel UMETA(DisplayName="体素"),
	// 材料
	Raw UMETA(DisplayName="材料"),
	// 道具
	Prop UMETA(DisplayName="道具"),
	// 装备
	Equip UMETA(DisplayName="装备"),
	// 技能
	Skill UMETA(DisplayName="技能"),
	// 物体
	Actor UMETA(DisplayName="物体"),
	// 生命体
	Vitality UMETA(DisplayName="生命体"),
	// 对象
	Pawn UMETA(DisplayName="对象"),
	// 角色
	Character UMETA(DisplayName="角色"),
	// 混杂
	Misc UMETA(DisplayName="混杂")
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
struct WHFRAMEWORK_API FAbilityItem : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FAbilityItem()
	{
		ID = FPrimaryAssetId();
		Count = 0;
		Level = 0;
		InventorySlot = nullptr;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}
	
	FORCEINLINE FAbilityItem(const FPrimaryAssetId& InID, int32 InCount = 1, int32 InLevel = 0)
	{
		ID = InID;
		Count = InCount;
		Level = InLevel;
		InventorySlot = nullptr;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}
	
	FORCEINLINE FAbilityItem(const FAbilityItem& InItem, int32 InCount = -1, bool bClearCaches = false)
	{
		ID = InItem.ID;
		Count = InCount == -1 ? InItem.Count : InCount;
		Level = InItem.Level;
		if(!bClearCaches)
		{
			InventorySlot = InItem.InventorySlot;
			AbilityHandle = InItem.AbilityHandle;
		}
		else
		{
			InventorySlot = nullptr;
			AbilityHandle = FGameplayAbilitySpecHandle();
		}
	}

	virtual ~FAbilityItem() override = default;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;

	UPROPERTY(Transient)
	UAbilityInventorySlotBase* InventorySlot;
	
	UPROPERTY(Transient)
	FGameplayAbilitySpecHandle AbilityHandle;
	
	static FAbilityItem Empty;

public:
	template<class T>
	T& GetData(bool bEnsured = true) const
	{
		return static_cast<T&>(GetData(bEnsured));
	}

	UAbilityItemDataBase& GetData(bool bEnsured = true) const;
	
	template<class T>
	bool IsDataType() const
	{
		return IsDataType(T::StaticClass());
	}

	bool IsDataType(TSubclassOf<UAbilityItemDataBase> InType) const;

	EAbilityItemType GetType() const;

	FORCEINLINE virtual bool IsValid() const override
	{
		return ID.IsValid();
	}

	FORCEINLINE virtual bool IsNull() const
	{
		return Count <= 0;
	}

	FORCEINLINE virtual bool IsEmpty() const
	{
		return *this == Empty;
	}

	FORCEINLINE bool Match(const FAbilityItem& InItem) const
	{
		return InItem.IsValid() && InItem.ID == ID && InItem.Level == Level;
	}

	FORCEINLINE bool Equal(const FAbilityItem& InItem) const
	{
		return (InItem.ID == ID) && (InItem.Count == Count) && (InItem.Level == Level);
	}

	FORCEINLINE friend bool operator==(const FAbilityItem& A, const FAbilityItem& B)
	{
		return A.Equal(B);
	}

	FORCEINLINE friend bool operator!=(const FAbilityItem& A, const FAbilityItem& B)
	{
		return !A.Equal(B);
	}

	FORCEINLINE friend FAbilityItem operator+(FAbilityItem& A, FAbilityItem& B)
	{
		if(A.Match(B))
		{
			A.Count += B.Count;
		}
		return A;
	}

	FORCEINLINE friend FAbilityItem operator-(FAbilityItem& A, FAbilityItem& B)
	{
		if(A.Match(B))
		{
			A.Count -= B.Count;
		}
		return A;
	}

	FORCEINLINE friend FAbilityItem& operator+=(FAbilityItem& A, FAbilityItem& B)
	{
		if(A.Match(B))
		{
			A.Count += B.Count;
		}
		return A;
	}

	FORCEINLINE friend FAbilityItem& operator-=(FAbilityItem& A, FAbilityItem& B)
	{
		if(A.Match(B))
		{
			A.Count -= B.Count;
		}
		return A;
	}
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "ID"))
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryRefresh);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventorySlotSelected, UAbilityInventorySlotBase*, InInventorySlot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventorySlotRefresh);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventorySlotActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventorySlottDeactived);

UENUM(BlueprintType)
enum class ESlotSplitType : uint8
{
	None,
	Default,
	Shortcut,
	Auxiliary,
	Equip,
	Skill
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FItemQueryData
{
	GENERATED_BODY()

public:
	FORCEINLINE FItemQueryData()
	{
		Item = FAbilityItem();
		Slots = TArray<UAbilityInventorySlotBase*>();
	}

	FORCEINLINE FItemQueryData(FAbilityItem InItem, TArray<UAbilityInventorySlotBase*> InSlots)
	{
		Item = InItem;
		Slots = InSlots;
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UAbilityInventorySlotBase*> Slots;

public:
	FORCEINLINE bool IsValid() const
	{
		return Item.Count > 0;
	}

	FORCEINLINE friend FItemQueryData operator+(FItemQueryData& A, FItemQueryData& B)
	{
		A.Item += B.Item;
		for(auto Iter : B.Slots)
		{
			A.Slots.Add(Iter);
		}
		return A;
	}

	FORCEINLINE friend FItemQueryData operator+=(FItemQueryData& A, FItemQueryData B)
	{
		A.Item = B.Item;
		for(auto Iter : B.Slots)
		{
			A.Slots.Add(Iter);
		}
		return A;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventorySlots
{
	GENERATED_BODY()

public:
	FORCEINLINE FInventorySlots()
	{
		Slots = TArray<UAbilityInventorySlotBase*>();
	}
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<UAbilityInventorySlotBase*> Slots;

public:
	FAbilityItems GetItems();
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetInventorySlotData
{
	GENERATED_BODY()

public:
	FORCEINLINE FWidgetInventorySlotData()
	{
		Class = nullptr;
		Slots = TArray<UWidgetAbilityInventorySlotBase*>();
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UWidgetAbilityInventorySlotBase> Class;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UWidgetAbilityInventorySlotBase*> Slots;
};

UENUM(BlueprintType)
enum class EAbilityItemFillType : uint8
{
	Fixed,
	Random,
	Rate
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItemFillItem
{
	GENERATED_BODY()

public:
	FORCEINLINE FAbilityItemFillItem()
	{
		Rate = 0.f;
		Num = 0;
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float Rate;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Num;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItemFillRule
{
	GENERATED_BODY()

public:
	FORCEINLINE FAbilityItemFillRule()
	{
		Rate = 0.f;
		Type = EAbilityItemFillType::Fixed;
		Num = 0;
		MinNum = 0;
		MaxNum = 0;
		Items = TArray<FAbilityItemFillItem>();
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float Rate;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EAbilityItemFillType Type;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (EditConditionHides, EditCondition = "Type==EAbilityItemFillType::Fixed"))
	int32 Num;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (EditConditionHides, EditCondition = "Type==EAbilityItemFillType::Random"))
	int32 MinNum;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (EditConditionHides, EditCondition = "Type==EAbilityItemFillType::Random"))
	int32 MaxNum;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (EditConditionHides, EditCondition = "Type==EAbilityItemFillType::Rate"))
	TArray<FAbilityItemFillItem> Items;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItemFillRules
{
	GENERATED_BODY()

public:
	FORCEINLINE FAbilityItemFillRules()
	{
		Rules = TMap<EAbilityItemRarity, FAbilityItemFillRule>();
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TMap<EAbilityItemRarity, FAbilityItemFillRule> Rules;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventorySaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FInventorySaveData()
	{
		InventoryClass = nullptr;
		SplitItems = TMap<ESlotSplitType, FAbilityItems>();
		SelectedIndexs = TMap<ESlotSplitType, int32>();
		FillRules = TMap<EAbilityItemType, FAbilityItemFillRules>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAbilityInventoryBase> InventoryClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ESlotSplitType, FAbilityItems> SplitItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EAbilityItemType, FAbilityItemFillRules> FillRules;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TMap<ESlotSplitType, int32> SelectedIndexs;

public:
	virtual bool IsValid() const override
	{
		if(!Super::IsValid()) return false;

		return SplitItems.Num() > 0;
	}
	
	virtual void FillItems(int32 InLevel, FRandomStream InRandomStream = FRandomStream());

	virtual void CopyItems(const FInventorySaveData& InSaveData);

	virtual void AddItem(FAbilityItem InItem, bool bUnique = false);

	virtual void RemoveItem(FAbilityItem InItem);

	virtual void ClearItem(FAbilityItem InItem);

	virtual void ClearItems();
};

/**
 * 查询项类型
 */
UENUM(BlueprintType)
enum class EItemQueryType : uint8
{
	// 获取
	Get,
	// 添加
	Add,
	// 移除
	Remove,
	// 匹配
	Match
};

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
struct WHFRAMEWORK_API FPickUpSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FPickUpSaveData()
	{
		Item = FAbilityItem::Empty;
		Location = FVector::ZeroVector;
	}

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector Location;
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
		InventoryData = FInventorySaveData();
		BirthTransform = FTransform::Identity;
	}

	FORCEINLINE FActorSaveData(const FSceneActorSaveData& InSceneActorSaveData) : FSceneActorSaveData(InSceneActorSaveData)
	{
		AssetID = FPrimaryAssetId();
		Name = NAME_None;
		Level = 1;
		InventoryData = FInventorySaveData();
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
	FInventorySaveData InventoryData;

	UPROPERTY()
	FTransform BirthTransform;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		InventoryData.MakeSaved();
	}

	virtual void InitInventoryData(FRandomStream InRandomStream = FRandomStream());

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

#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "Abilities/GameplayAbility.h"
#include "Common/Base/WHObject.h"
#include "GameplayTagContainer.h"
#include "Asset/AssetModuleTypes.h"

#include "AbilityModuleTypes.generated.h"

class UAbilityInventoryBase;
class UAbilityCharacterDataBase;
class UAbilityVitalityDataBase;
class UAbilityItemDataBase;
class AAbilityProjectileBase;
class AAbilityEquipBase;
class UWidgetAbilityInventorySlotBase;
class UAbilityInventorySlotBase;

#define GET_GAMEPLAYATTRIBUTE_PROPERTY(ClassName, PropertyName) \
	FindFieldChecked<FProperty>(ClassName::StaticClass(), GET_MEMBER_NAME_CHECKED(ClassName, PropertyName)); \

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

	virtual void HandleDamage(AActor* SourceActor, AActor* TargetActor, float DamageValue, EDamageType DamageType, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
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

	virtual void HandleRecovery(AActor* SourceActor, AActor* TargetActor, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
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

	virtual void HandleInterrupt(AActor* SourceActor, AActor* TargetActor, float InterruptDuration, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
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
    TArray<TSubclassOf<UGameplayEffect>> TargetGameplayEffectClasses;
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityInfo
{
	GENERATED_BODY()

public:
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityData : public FDataTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AbilityLevel;

	FGameplayAbilitySpecHandle AbilityHandle;

public:
	FAbilityData()
	{
		AbilityLevel = 1;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}

	virtual bool IsValid() const override
	{
		return AbilityHandle.IsValid();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItemData : public FAbilityData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId AbilityID;

	FORCEINLINE FAbilityItemData()
	{
		AbilityID = FPrimaryAssetId();
	}

public:
	template<class T>
	T& GetItemData() const
	{
		return static_cast<T&>(GetItemData());
	}

	UAbilityItemDataBase& GetItemData() const;
};

/**
 * 伤害类型
 */
UENUM(BlueprintType)
enum class EDamageType : uint8
{
	// 物理伤害
	Physics,
	// 魔法伤害
	Magic,
	// 掉落伤害
	Fall
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
	// 远程
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
 * 能力项类型
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
	// 对象
	Actor UMETA(DisplayName="对象"),
	// 生命
	Vitality UMETA(DisplayName="生命"),
	// 角色
	Character UMETA(DisplayName="角色")
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItem : public FSaveData
{
	GENERATED_BODY()

public:
	static FAbilityItem Empty;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;

	FGameplayAbilitySpecHandle AbilityHandle;

public:
	FORCEINLINE FAbilityItem()
	{
		ID = FPrimaryAssetId();
		Count = 0;
		Level = 0;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}
				
	FORCEINLINE FAbilityItem(const FSaveData& InSaveData) : FSaveData(InSaveData)
	{
		ID = FPrimaryAssetId();
		Count = 0;
		Level = 0;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}
		
	FORCEINLINE FAbilityItem(const FPrimaryAssetId& InID, int32 InCount = 1, int32 InLevel = 0)
	{
		ID = InID;
		Count = InCount;
		Level = InLevel;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}
	
	FORCEINLINE FAbilityItem(const FAbilityItem& InVoxelItem, int32 InCount = -1)
	{
		ID = InVoxelItem.ID;
		Count = InCount == -1 ? InVoxelItem.Count : InCount;
		Level = InVoxelItem.Level;
		AbilityHandle = InVoxelItem.AbilityHandle;
	}

	virtual ~FAbilityItem() override = default;

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

	FORCEINLINE bool EqualType(const FAbilityItem& InItem) const
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItems
{
	GENERATED_BODY()

public:
	FAbilityItems()
	{
		Items = TArray<FAbilityItem>();
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "ID"))
	TArray<FAbilityItem> Items;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryRefresh);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventorySlotSelected, UAbilityInventorySlotBase*, InInventorySlot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventorySlotRefresh);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventorySlotActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventorySlotCanceled);

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
struct WHFRAMEWORK_API FItemQueryInfo
{
	GENERATED_BODY()

public:
	FORCEINLINE FItemQueryInfo()
	{
		Item = FAbilityItem();
		Slots = TArray<UAbilityInventorySlotBase*>();
	}

	FORCEINLINE FItemQueryInfo(FAbilityItem InItem, TArray<UAbilityInventorySlotBase*> InSlots)
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

	FORCEINLINE friend FItemQueryInfo operator+(FItemQueryInfo& A, FItemQueryInfo& B)
	{
		A.Item += B.Item;
		for(auto Iter : B.Slots)
		{
			A.Slots.Add(Iter);
		}
		return A;
	}

	FORCEINLINE friend FItemQueryInfo operator+=(FItemQueryInfo& A, FItemQueryInfo B)
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
struct WHFRAMEWORK_API FWidgetInventorySlots
{
	GENERATED_BODY()

public:
	FORCEINLINE FWidgetInventorySlots()
	{
		Slots = TArray<UWidgetAbilityInventorySlotBase*>();
	}

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<UWidgetAbilityInventorySlotBase*> Slots;
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
		SelectedIndex = -1;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAbilityInventoryBase> InventoryClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ESlotSplitType, FAbilityItems> SplitItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 SelectedIndex;

public:
	virtual bool IsValid() const override
	{
		if(!Super::IsValid()) return false;

		return SplitItems.Num() > 0;
	}
	
	virtual void CopyAllItem(FInventorySaveData SaveData);

	virtual void AddItem(FAbilityItem InItem);

	virtual void RemoveItem(FAbilityItem InItem);

	virtual void ClearItem(FAbilityItem InItem);

	virtual void ClearAllItem();
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
	Remove
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FRaceItem : public FAbilityItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "Count == 0"))
	int32 MinCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "Count == 0"))
	int32 MaxCount;

	FORCEINLINE FRaceItem()
	{
		MinCount = 0;
		MaxCount = 0;
		Level = 1;
	}
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRaceItem> Items;

	FORCEINLINE FRaceData()
	{
		NoiseScale = FVector::OneVector;
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
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector Location;
	
	FORCEINLINE FPickUpSaveData()
	{
		Item = FAbilityItem::Empty;
		Location = FVector::ZeroVector;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FActorSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FActorSaveData()
	{
		ActorID = FGuid();
		AssetID = FPrimaryAssetId();
		Name = NAME_None;
		Level = 1;
		InventoryData = FInventorySaveData();
		SpawnLocation = FVector::ZeroVector;
		SpawnRotation = FRotator::ZeroRotator;
	}

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FGuid ActorID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId AssetID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInventorySaveData InventoryData;

	UPROPERTY()
	FVector SpawnLocation;
	
	UPROPERTY()
	FRotator SpawnRotation;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		InventoryData.MakeSaved();
	}

	template<class T>
	T& GetItemData() const
	{
		return static_cast<T&>(GetItemData());
	}

	UAbilityItemDataBase& GetItemData() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVitalitySaveData : public FActorSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVitalitySaveData()
	{
		RaceID = NAME_None;
	}

	FORCEINLINE FVitalitySaveData(const FActorSaveData& InActorSaveData) : FActorSaveData(InActorSaveData)
	{
		RaceID = NAME_None;
	}

public:
	UPROPERTY(BlueprintReadWrite)
	FName RaceID;
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
		CameraRotation = FRotator::ZeroRotator;
		CameraDistance = -1.f;
	}

	FORCEINLINE FCharacterSaveData(const FPawnSaveData& InPawnSaveData) : FPawnSaveData(InPawnSaveData)
	{
		CameraRotation = FRotator(-1.f);
		CameraDistance = -1.f;
	}

public:
	UPROPERTY()
	FRotator CameraRotation;

	UPROPERTY()
	float CameraDistance;
};

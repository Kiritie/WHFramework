#pragma once

#include "Abilities/GameplayAbilityTypes.h"
#include "Asset/AssetManagerBase.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Abilities/GameplayAbility.h"

#include "AbilityModuleTypes.generated.h"

class UAbilityCharacterDataBase;
class UAbilityVitalityDataBase;
class UAbilityItemDataBase;
class AAbilitySkillBase;
class AAbilityEquipBase;

#define GAMEPLAYATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_ACCESSORS(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	ATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_GETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_SETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_MODIFY(ClassName, PropertyName) \
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

#define ATTRIBUTE_VALUE_INITTER(ClassName, PropertyName) \
	FORCEINLINE void Init##PropertyName(float InValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->Init##PropertyName(InValue); \
	}

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
class WHFRAMEWORK_API UTargetType : public UObject
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

	FORCEINLINE FAbilityInfo()
	{
		CostAttribute = FGameplayAttribute();
		CostValue = 0.f;
		CooldownDuration = -1.f;
		CooldownRemaining = 0.f;
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
	int32 AbilityLevel;

	FGameplayAbilitySpecHandle AbilityHandle;

public:
	FAbilityData()
	{
		AbilityLevel = 1;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}

	bool IsValid() const
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
 * 交互选项
 */
UENUM(BlueprintType)
enum class EInteractAction : uint8
{
	// 无
	None UMETA(DisplayName="无"),
	// 复活
	Revive UMETA(DisplayName="复活"),
	// 战斗
	Fight UMETA(DisplayName="战斗"),
	// 对话
	Dialogue UMETA(DisplayName="对话"),
	// 交易
	Transaction UMETA(DisplayName="交易"),
	
	Custom1 UMETA(Hidden),
	Custom2 UMETA(Hidden),
	Custom3 UMETA(Hidden),
	Custom4 UMETA(Hidden),
	Custom5 UMETA(Hidden),
	Custom6 UMETA(Hidden),
	Custom7 UMETA(Hidden),
	Custom8 UMETA(Hidden),
	Custom9 UMETA(Hidden),
	Custom10 UMETA(Hidden)
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

/**
 * 能力项类型
 */
UENUM(BlueprintType)
enum class EAbilityItemType : uint8
{
	// 无
	None UMETA(DisplayName="无"),
	// 体素
	Voxel UMETA(DisplayName="体素"),
	// 道具
	Prop UMETA(DisplayName="道具"),
	// 装备
	Equip UMETA(DisplayName="装备"),
	// 技能
	Skill UMETA(DisplayName="技能"),
	// 生命
	Vitality UMETA(DisplayName="生命"),
	// 角色
	Character UMETA(DisplayName="角色")
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItem
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
				
	FORCEINLINE FAbilityItem(const FPrimaryAssetId& InID, int32 InCount = 1, int32 InLevel = 0)
	{
		ID = InID;
		Count = InCount;
		Level = InLevel;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}
	
	FORCEINLINE FAbilityItem(const FAbilityItem& InItem, int InCount = -1)
	{
		ID = InItem.ID;
		Count = InCount == -1 ? InItem.Count : InCount;
		Level = InItem.Level;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}

	virtual ~FAbilityItem() = default;

	template<class T>
	T& GetData() const
	{
		return static_cast<T&>(GetData());
	}

	UAbilityItemDataBase& GetData() const;

	FORCEINLINE virtual bool IsValid(bool bNeedNotNull = false) const
	{
		return ID.IsValid() && (!bNeedNotNull || Count > 0);
	}

	FORCEINLINE virtual bool IsEmpty() const
	{
		return *this == Empty;
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItems
{
	GENERATED_BODY()

public:
	FAbilityItems()
	{
		Items = TArray<FAbilityItem>();
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityItem> Items;
};

/**
 * 种族数据
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FRaceData : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FName ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Detail;
				
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Proportion;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityItem> Items;

	FORCEINLINE FRaceData()
	{
		ID = NAME_None;
		Name = NAME_None;
		Detail = TEXT("");
		Proportion = 1;
		Items = TArray<FAbilityItem>();
	}

	FORCEINLINE bool IsValid() const
	{
		return !ID.IsNone();
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
struct WHFRAMEWORK_API FVitalitySaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVitalitySaveData()
	{
		Name = NAME_None;
		RaceID = NAME_None;
		Level = 0;
		SpawnLocation = FVector::ZeroVector;
		SpawnRotation = FRotator::ZeroRotator;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId ID;

	UPROPERTY(BlueprintReadWrite)
	FName Name;
		
	UPROPERTY(BlueprintReadOnly)
	FName RaceID;

	UPROPERTY(BlueprintReadWrite)
	int32 Level;
	
	UPROPERTY()
	FVector SpawnLocation;
	
	UPROPERTY()
	FRotator SpawnRotation;

	UPROPERTY()
	FAbilityData DefaultAbility;

public:
	template<class T>
	T& GetVitalityData() const
	{
		return static_cast<T&>(GetVitalityData());
	}

	UAbilityVitalityDataBase& GetVitalityData() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterSaveData : public FVitalitySaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FCharacterSaveData()
	{
	}

public:
	template<class T>
	T& GetCharacterData() const
	{
		return static_cast<T&>(GetCharacterData());
	}

	UAbilityCharacterDataBase& GetCharacterData() const;
};
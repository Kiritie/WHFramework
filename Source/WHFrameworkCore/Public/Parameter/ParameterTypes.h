#pragma once

#include "Json.h"
#include "Parameter/ParameterValueAdapter.h"
#include "WHFrameworkCoreTypes.h"
#include "ParameterTypes.generated.h"

/** Legacy Blueprint labels only. V2 runtime identity is the wrapper UScriptStruct. */
UENUM(BlueprintType)
enum class EParameterType : uint8
{
	None,
	Misc,
	Integer,
	Float,
	Byte,
	Enum,
	String,
	Name,
	Text,
	Boolean,
	Vector,
	Rotator,
	Transform,
	Color,
	LinearColor,
	Key,
	Tag,
	Tags,
	Brush,
	Guid,
	AssetID,
	Class,
	ClassPtr,
	Object,
	ObjectInst,
	ObjectPtr,
	Delegate,
	Pointer UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameter
{
	GENERATED_BODY()

	FParameter();
	FParameter(const FParameter&) = default;
	FParameter(FParameter&&) = default;
	FParameter& operator=(const FParameter&) = default;
	FParameter& operator=(FParameter&&) = default;

	template <typename T, typename DecayType = std::decay_t<T>,
			  std::enable_if_t<TParameterValueAdapter<DecayType>::bSupported && !std::is_same_v<DecayType, FParameter>, int> = 0>
	FParameter(T&& InValue)
	{
		Set(Forward<T>(InValue));
	}

	template <typename T, typename DecayType = std::decay_t<T>, std::enable_if_t<TParameterValueAdapter<DecayType>::bSupported, int> = 0>
	void Set(T&& InValue)
	{
		TParameterValueAdapter<DecayType>::Set(Value, Forward<T>(InValue));
	}

	template <typename T> bool Is() const
	{
		using Adapter = TParameterValueAdapter<std::decay_t<T>>;
		return Adapter::bSupported && Value.GetScriptStruct() == Adapter::WrapperType::StaticStruct();
	}
	template <typename T> auto GetPtr() const
	{
		return TParameterValueAdapter<std::decay_t<T>>::GetPtr(Value);
	}
	template <typename T> auto GetMutablePtr()
	{
		return TParameterValueAdapter<std::decay_t<T>>::GetMutablePtr(Value);
	}

	const UScriptStruct* GetValueType() const
	{
		return Value.GetScriptStruct();
	}
	const void* GetValueMemory() const
	{
		return Value.GetMemory();
	}
	void* GetMutableValueMemory()
	{
		return Value.GetMutableMemory();
	}
	bool HasValue() const
	{
		return Value.IsValid();
	}
	void ResetValue()
	{
		Value.Reset();
	}
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		return Value.NetSerialize(Ar, Map, bOutSuccess);
	}

	friend bool operator==(const FParameter& A, const FParameter& B)
	{
		return A.Value == B.Value;
	}
	friend bool operator!=(const FParameter& A, const FParameter& B)
	{
		return !(A == B);
	}
	FORCEINLINE operator const TArray<FParameter>() const
	{
		return {*this};
	}

	void SetParameterValue(const FParameter& InParameter)
	{
		const FText SavedDescription = Description;
		Value = InParameter.Value;
		Description = SavedDescription;
	}
	FText GetDescription() const
	{
		return Description;
	}
	void SetDescription(const FText& InDescription)
	{
		Description = InDescription;
	}
	EParameterType GetParameterType() const;
	void SetParameterType(EParameterType InParameterType);

#define WH_PARAMETER_ACCESSORS(Name, NativeType, DefaultValue)                                                                             \
	NativeType Get##Name##Value() const                                                                                                    \
	{                                                                                                                                      \
		const NativeType* Ptr = GetPtr<NativeType>();                                                                                      \
		return Ptr ? *Ptr : DefaultValue;                                                                                                  \
	}                                                                                                                                      \
	void Set##Name##Value(const NativeType& InValue)                                                                                       \
	{                                                                                                                                      \
		Set(InValue);                                                                                                                      \
	}
	WH_PARAMETER_ACCESSORS(Integer, int32, 0)
	WH_PARAMETER_ACCESSORS(Float, float, 0.f)
	WH_PARAMETER_ACCESSORS(Byte, uint8, 0)
	WH_PARAMETER_ACCESSORS(Enum, FEnumParameterValue, FEnumParameterValue())
	WH_PARAMETER_ACCESSORS(String, FString, FString())
	WH_PARAMETER_ACCESSORS(Name, FName, NAME_None)
	WH_PARAMETER_ACCESSORS(Text, FText, FText::GetEmpty())
	WH_PARAMETER_ACCESSORS(Boolean, bool, false)
	WH_PARAMETER_ACCESSORS(Vector, FVector, FVector::ZeroVector)
	WH_PARAMETER_ACCESSORS(Rotator, FRotator, FRotator::ZeroRotator)
	WH_PARAMETER_ACCESSORS(Transform, FTransform, FTransform::Identity)
	WH_PARAMETER_ACCESSORS(Color, FColor, FColor::Transparent)
	WH_PARAMETER_ACCESSORS(LinearColor, FLinearColor, FLinearColor::Transparent)
	WH_PARAMETER_ACCESSORS(Key, FKey, FKey())
	WH_PARAMETER_ACCESSORS(Tag, FGameplayTag, FGameplayTag())
	WH_PARAMETER_ACCESSORS(Tags, FGameplayTagContainer, FGameplayTagContainer())
	WH_PARAMETER_ACCESSORS(Brush, FSlateBrush, FSlateBrush())
	WH_PARAMETER_ACCESSORS(Guid, FGuid, FGuid())
	WH_PARAMETER_ACCESSORS(AssetID, FPrimaryAssetId, FPrimaryAssetId())
#undef WH_PARAMETER_ACCESSORS

	UClass* GetClassValue() const;
	template <class T> TSubclassOf<T> GetClassValue() const
	{
		return GetClassValue();
	}
	void SetClassValue(UClass* InValue);
	template <class T = UObject> TSoftClassPtr<T> GetClassPtrValue() const
	{
		const FParameterSoftClassValue* Wrapper = Value.GetPtr<FParameterSoftClassValue>();
		return Wrapper ? TSoftClassPtr<T>(Wrapper->Value.ToSoftObjectPath()) : nullptr;
	}
	template <class T = UObject> void SetClassPtrValue(const TSoftClassPtr<T>& InValue)
	{
		Set(InValue);
	}

	UObject* GetObjectValue() const;
	template <class T> T* GetObjectValue() const
	{
		return Cast<T>(GetObjectValue());
	}
	void SetObjectValue(UObject* InValue)
	{
		Set(InValue);
	}
	UObject* GetObjectInstValue() const;
	template <class T> T* GetObjectInstValue() const
	{
		return Cast<T>(GetObjectInstValue());
	}
	void SetObjectInstValue(UObject* InValue);
	template <class T = UObject> TSoftObjectPtr<T> GetObjectPtrValue() const
	{
		const FParameterSoftObjectValue* Wrapper = Value.GetPtr<FParameterSoftObjectValue>();
		return Wrapper ? TSoftObjectPtr<T>(Wrapper->Value.ToSoftObjectPath()) : nullptr;
	}
	template <class T = UObject> void SetObjectPtrValue(const TSoftObjectPtr<T>& InValue)
	{
		Set(InValue);
	}
	FSimpleDynamicDelegate GetDelegateValue() const;
	void SetDelegateValue(const FSimpleDynamicDelegate& InValue)
	{
		Set(InValue);
	}

	void* GetPointerValue() const;
	template <typename T> T* GetPointerValue() const
	{
		return static_cast<T*>(GetPointerValue());
	}
	template <typename T> T& GetPointerValueRef() const
	{
		return *GetPointerValue<T>();
	}
	void SetPointerValue(void* InValue)
	{
		Set(InValue);
	}
	void SetPointerValue(const void* InValue)
	{
		Set(InValue);
	}

#define WH_PARAMETER_MAKE(Name, NativeType)                                                                                                \
	static FParameter Make##Name(const NativeType& InValue, const FText& InDescription = FText::GetEmpty())                                \
	{                                                                                                                                      \
		FParameter Result(InValue);                                                                                                        \
		Result.Description = InDescription;                                                                                                \
		return Result;                                                                                                                     \
	}
	WH_PARAMETER_MAKE(Integer, int32)
	WH_PARAMETER_MAKE(Float, float)
	WH_PARAMETER_MAKE(Byte, uint8)
	WH_PARAMETER_MAKE(Enum, FEnumParameterValue)
	WH_PARAMETER_MAKE(String, FString)
	WH_PARAMETER_MAKE(Name, FName)
	WH_PARAMETER_MAKE(Text, FText)
	WH_PARAMETER_MAKE(Boolean, bool)
	WH_PARAMETER_MAKE(Vector, FVector)
	WH_PARAMETER_MAKE(Rotator, FRotator)
	WH_PARAMETER_MAKE(Transform, FTransform)
	WH_PARAMETER_MAKE(Color, FColor)
	WH_PARAMETER_MAKE(LinearColor, FLinearColor)
	WH_PARAMETER_MAKE(Key, FKey)
	WH_PARAMETER_MAKE(Tag, FGameplayTag)
	WH_PARAMETER_MAKE(Tags, FGameplayTagContainer)
	WH_PARAMETER_MAKE(Brush, FSlateBrush)
	WH_PARAMETER_MAKE(Guid, FGuid)
	WH_PARAMETER_MAKE(AssetID, FPrimaryAssetId)
	WH_PARAMETER_MAKE(Delegate, FSimpleDynamicDelegate)
#undef WH_PARAMETER_MAKE

	static FParameter MakeVector(const FVector2D& InValue, const FText& InDescription = FText::GetEmpty())
	{
		return MakeVector(FVector(InValue, 0.f), InDescription);
	}
	static FParameter MakeClass(UClass* InValue, const FText& InDescription = FText::GetEmpty());
	template <class T> static FParameter MakeClassPtr(const TSoftClassPtr<T>& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Result;
		Result.SetClassPtrValue(InValue);
		Result.Description = InDescription;
		return Result;
	}
	static FParameter MakeObject(UObject* InValue, const FText& InDescription = FText::GetEmpty());
	static FParameter MakeObjectInst(UObject* InValue, const FText& InDescription = FText::GetEmpty());
	static FParameter MakePointer(void* InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Result(InValue);
		Result.Description = InDescription;
		return Result;
	}
	static FParameter MakePointer(const void* InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Result(InValue);
		Result.Description = InDescription;
		return Result;
	}
	template <class T> static FParameter MakeObjectPtr(const TSoftObjectPtr<T>& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Result;
		Result.SetObjectPtrValue(InValue);
		Result.Description = InDescription;
		return Result;
	}

	FORCEINLINE operator int32() const
	{
		return GetIntegerValue();
	}
	FORCEINLINE operator float() const
	{
		return GetFloatValue();
	}
	FORCEINLINE operator uint8() const
	{
		return GetByteValue();
	}
	FORCEINLINE operator FEnumParameterValue() const
	{
		return GetEnumValue();
	}
	FORCEINLINE operator FString() const
	{
		return GetStringValue();
	}
	FORCEINLINE operator FName() const
	{
		return GetNameValue();
	}
	FORCEINLINE operator FText() const
	{
		return GetTextValue();
	}
	FORCEINLINE operator bool() const
	{
		return GetBooleanValue();
	}
	FORCEINLINE operator FVector() const
	{
		return GetVectorValue();
	}
	FORCEINLINE operator FRotator() const
	{
		return GetRotatorValue();
	}
	FORCEINLINE operator FTransform() const
	{
		return GetTransformValue();
	}
	FORCEINLINE operator FColor() const
	{
		return GetColorValue();
	}
	FORCEINLINE operator FLinearColor() const
	{
		return GetLinearColorValue();
	}
	FORCEINLINE operator FKey() const
	{
		return GetKeyValue();
	}
	FORCEINLINE operator FGameplayTag() const
	{
		return GetTagValue();
	}
	FORCEINLINE operator FGameplayTagContainer() const
	{
		return GetTagsValue();
	}
	FORCEINLINE operator FSlateBrush() const
	{
		return GetBrushValue();
	}
	FORCEINLINE operator FGuid() const
	{
		return GetGuidValue();
	}
	FORCEINLINE operator FPrimaryAssetId() const
	{
		return GetAssetIDValue();
	}
	FORCEINLINE operator UClass*() const
	{
		return GetClassValue();
	}
	template <class T = UObject> FORCEINLINE operator TSubclassOf<T>() const
	{
		return GetClassValue<T>();
	}
	template <class T = UObject> FORCEINLINE operator TSoftClassPtr<T>() const
	{
		return GetClassPtrValue<T>();
	}
	FORCEINLINE operator UObject*() const
	{
		return GetObjectValue();
	}
	template <class T> FORCEINLINE operator T*() const
	{
		return GetObjectValue<T>();
	}
	template <class T = UObject> FORCEINLINE operator TSoftObjectPtr<T>() const
	{
		return GetObjectPtrValue<T>();
	}
	FORCEINLINE operator FSimpleDynamicDelegate() const
	{
		return GetDelegateValue();
	}
	FORCEINLINE operator void*() const
	{
		return GetPointerValue();
	}

private:
	UPROPERTY(EditAnywhere, meta = (ExcludeBaseStruct))
	TInstancedStruct<FParameterValueBase> Value;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FText Description;
};

template <> struct TStructOpsTypeTraits<FParameter> : TStructOpsTypeTraitsBase2<FParameter>
{
	enum
	{
		WithNetSerializer = true
	};
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameterSet
{
	GENERATED_BODY()
	FParameterSet();
	FParameterSet(FName InName, const FParameter& InParameter, const FText& InCategory = FText::GetEmpty(), bool bInRegistered = false)
		: Name(InName), bRegistered(bInRegistered), Category(InCategory), Parameter(InParameter)
	{
	}
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRegistered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bRegistered"))
	FText Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FParameter Parameter;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "Name"))
	TArray<FParameterSet> Sets;
	bool HasParameter(FName InName, bool bEnsured = true) const;
	void SetParameter(FName InName, const FParameter& InParameter);
	FParameter GetParameter(FName InName, bool bEnsured = true) const;
	TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const;
	void RemoveParameter(FName InName);
	void RemoveParameters(FName InName);
	void ClearAllParameter();
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParamData
{
	GENERATED_BODY()
	FParamData();
	virtual ~FParamData() = default;
	virtual void FromParams(const TArray<FParameter>& InParams)
	{
	}
	virtual TArray<FParameter> ToParams() const
	{
		return {};
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameterMap
{
	GENERATED_BODY()
	FParameterMap();
	FParameterMap(const TMap<FString, FString>& InMap) : Map(InMap)
	{
	}
	void Add(const FString& Key, const FString& InValue)
	{
		if (!Map.Contains(Key))
			Map.Add(Key, InValue);
	}
	void Set(const FString& Key, const FString& InValue)
	{
		Map.Emplace(Key, InValue);
	}
	void Remove(const FString& Key)
	{
		Map.Remove(Key);
	}
	void Clear()
	{
		Map.Empty();
	}
	bool Contains(const FString& Key) const
	{
		return Map.Contains(Key);
	}
	FString Get(const FString& Key) const
	{
		return Map.FindRef(Key);
	}
	const TMap<FString, FString>& GetSource() const
	{
		return Map;
	}
	int32 GetNum() const
	{
		return Map.Num();
	}
	FString ToString() const;
	FString ToJsonString() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, FString> Map;
};

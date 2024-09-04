// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Json.h"
#include "GameplayTagContainer.h"
#include "WHFrameworkCoreTypes.h"

#include "ParameterTypes.generated.h"

UENUM(BlueprintType)
enum class EParameterType : uint8
{
	None,
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
	Color,
	Key,
	Tag,
	Tags,
	Class,
	ClassPtr,
	Object,
	ObjectPtr,
	Delegate,
	Pointer UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FEnumParameterValue
{
	GENERATED_USTRUCT_BODY()

public:
	FEnumParameterValue()
	{
		EnumType = nullptr;
		EnumName = TEXT("");
		EnumNames = TArray<FString>();
		EnumValue = 0;
	}

	FEnumParameterValue(UEnum* const InEnumType, const uint8 InEnumValue) : FEnumParameterValue()
	{
		EnumType = InEnumType;
		EnumValue = FMath::Clamp(InEnumValue, 0, FMath::Max(EnumType->NumEnums() - 1, 0));
	}

	FEnumParameterValue(const FString& InEnumName, const uint8 InEnumValue) : FEnumParameterValue()
	{
		EnumType = LoadObject<UEnum>(nullptr, *InEnumName);
		EnumName = InEnumName;
		EnumValue = FMath::Clamp(InEnumValue, 0, FMath::Max(EnumType->NumEnums() - 1, 0));
	}
	
	FEnumParameterValue(const TArray<FString>& InEnumNames, const uint8 InEnumValue) : FEnumParameterValue()
	{
		EnumNames = InEnumNames;
		EnumValue = FMath::Clamp(InEnumValue, 0, FMath::Max(EnumNames.Num() - 1, 0));
	}

	friend bool operator==(const FEnumParameterValue& A, const FEnumParameterValue& B)
	{
		return A.EnumType == B.EnumType && A.EnumName == B.EnumName && A.EnumNames == B.EnumNames && A.EnumValue == B.EnumValue;
	}

	friend bool operator!=(const FEnumParameterValue& A, const FEnumParameterValue& B)
	{
		return A.EnumType != B.EnumType || A.EnumName != B.EnumName && A.EnumNames == B.EnumNames || A.EnumValue != B.EnumValue;
	}

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UEnum* EnumType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString EnumName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FString> EnumNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 EnumValue;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameter
{
#if WITH_EDITOR
	friend class FParameterCustomization;
#endif
	
	GENERATED_USTRUCT_BODY()

public:
	FParameter()
	{
		ParameterType = EParameterType::None;
		Description = FText::GetEmpty();
		IntegerValue = 0;
		FloatValue = 0.f;
		ByteValue = 0;
		EnumValue = FEnumParameterValue();
		StringValue = TEXT("");
		NameValue = NAME_None;
		TextValue = FText::GetEmpty();
		BooleanValue = false;
		VectorValue = FVector::ZeroVector;
		RotatorValue = FRotator::ZeroRotator;
		ColorValue = FColor::Transparent;
		KeyValue = FKey();
		TagValue = FGameplayTag();
		TagsValue = FGameplayTagContainer();
		ClassValue = nullptr;
		ClassPtrValue = nullptr;
		ObjectValue = nullptr;
		ObjectPtrValue = nullptr;
		PointerValue = nullptr;
	}

	FParameter(int32 InValue)
	{
		*this = MakeInteger(InValue);
	}
	
	FParameter(float InValue)
	{
		*this = MakeFloat(InValue);
	}
	
	FParameter(double InValue)
	{
		*this = MakeFloat(InValue);
	}
		
	FParameter(uint8 InValue)
	{
		*this = MakeByte(InValue);
	}

	FParameter(const FEnumParameterValue& InValue)
	{
		*this = MakeEnum(InValue);
	}

	FParameter(const FString& InValue)
	{
		*this = MakeString(InValue);
	}
			
	FParameter(const FName& InValue)
	{
		*this = MakeName(InValue);
	}

	FParameter(const FText& InValue)
	{
		*this = MakeText(InValue);
	}

	FParameter(bool InValue)
	{
		*this = MakeBoolean(InValue);
	}
	
	FParameter(const FVector& InValue)
	{
		*this = MakeVector(InValue);
	}
		
	FParameter(FVector2D InValue)
	{
		*this = MakeVector(InValue);
	}

	FParameter(const FRotator& InValue)
	{
		*this = MakeRotator(InValue);
	}
	
	FParameter(FColor InValue)
	{
		*this = MakeColor(InValue);
	}
		
	FParameter(FLinearColor InValue)
	{
		*this = MakeColor(InValue);
	}
	
	FParameter(const FKey& InValue)
	{
		*this = MakeKey(InValue);
	}

	FParameter(const FGameplayTag& InValue)
	{
		*this = MakeTag(InValue);
	}
		
	FParameter(const FGameplayTagContainer& InValue)
	{
		*this = MakeTags(InValue);
	}

	FParameter(UClass* InValue)
	{
		*this = MakeClass(InValue);
	}
	
	FParameter(const TSoftClassPtr<UObject>& InValue)
	{
		*this = MakeClassPtr(InValue);
	}

	template<class T>
	FParameter(const TSoftClassPtr<T>& InValue)
	{
		*this = MakeClassPtr(InValue);
	}

	FParameter(UObject* InValue)
	{
		*this = MakeObject(InValue);
	}
	
	template<class T>
	FParameter(const TSoftObjectPtr<T>& InValue)
	{
		*this = MakeObjectPtr(InValue);
	}

	FParameter(void* InValue)
	{
		*this = MakePointer(InValue);
	}

	FParameter(const void* InValue)
	{
		*this = MakePointer(InValue);
	}

	FORCEINLINE operator int32() const { return IntegerValue; }
	
	FORCEINLINE operator float() const { return FloatValue; }
	
	FORCEINLINE operator uint8() const { return ByteValue; }
	
	FORCEINLINE operator FEnumParameterValue() const { return EnumValue; }
	
	FORCEINLINE operator FString() const { return StringValue; }
	
	FORCEINLINE operator FName() const { return NameValue; }
	
	FORCEINLINE operator FText() const { return TextValue; }
	
	FORCEINLINE operator bool() const { return BooleanValue; }
	
	FORCEINLINE operator FVector() const { return VectorValue; }
	
	FORCEINLINE operator FRotator() const { return RotatorValue; }
	
	FORCEINLINE operator FColor() const { return ColorValue; }
	
	FORCEINLINE operator FKey() const { return KeyValue; }
	
	FORCEINLINE operator FGameplayTag() const { return TagValue; }
	
	FORCEINLINE operator FGameplayTagContainer() const { return TagsValue; }
	
	FORCEINLINE operator UClass*() const { return ClassValue; }
	
	template<class T>
	FORCEINLINE operator TSubclassOf<T>() const { return GetClassValue<T>(); }

	template<class T>
	FORCEINLINE operator TSoftClassPtr<T>() const { return GetClassPtrValue<T>(); }
	
	FORCEINLINE operator UObject*() const { return ObjectValue; }
	
	template<class T>
	FORCEINLINE operator T*() const { return GetObjectValue<T>(); }

	template<class T>
	FORCEINLINE operator TSoftObjectPtr<T>() const { return GetObjectPtrValue<T>(); }

	FORCEINLINE operator FSimpleDynamicDelegate() const { return DelegateValue; }
	
	FORCEINLINE operator void*() const { return PointerValue; }

	friend bool operator==(const FParameter& A, const FParameter& B)
	{
		switch (A.ParameterType)
		{
			case EParameterType::Integer: return A.IntegerValue == B.IntegerValue;
			case EParameterType::Float: return A.FloatValue == B.FloatValue;
			case EParameterType::Byte: return A.ByteValue == B.ByteValue;
			case EParameterType::Enum: return A.EnumValue == B.EnumValue;
			case EParameterType::String: return A.StringValue == B.StringValue;
			case EParameterType::Name: return A.NameValue == B.NameValue;
			case EParameterType::Text: return A.TextValue.EqualTo(B.TextValue);
			case EParameterType::Boolean: return A.BooleanValue == B.BooleanValue;
			case EParameterType::Vector: return A.VectorValue == B.VectorValue;
			case EParameterType::Rotator: return A.RotatorValue == B.RotatorValue;
			case EParameterType::Color: return A.ColorValue == B.ColorValue;
			case EParameterType::Tag: return A.TagValue == B.TagValue;
			case EParameterType::Tags: return A.TagsValue == B.TagsValue;
			case EParameterType::Class: return A.ClassValue == B.ClassValue;
			case EParameterType::ClassPtr: return A.ClassPtrValue == B.ClassPtrValue;
			case EParameterType::Object: return A.ObjectValue == B.ObjectValue;
			case EParameterType::ObjectPtr: return A.ObjectPtrValue == B.ObjectPtrValue;
			case EParameterType::Delegate: return A.DelegateValue == B.DelegateValue;
			case EParameterType::Pointer: return A.PointerValue == B.PointerValue;
			default: ;
		}
		return false;
	}

	friend bool operator!=(const FParameter& A, const FParameter& B)
	{
		switch (A.ParameterType)
		{
			case EParameterType::Integer: return A.IntegerValue != B.IntegerValue;
			case EParameterType::Float: return A.FloatValue != B.FloatValue;
			case EParameterType::Byte: return A.ByteValue != B.ByteValue;
			case EParameterType::Enum: return A.EnumValue != B.EnumValue;
			case EParameterType::String: return A.StringValue != B.StringValue;
			case EParameterType::Name: return A.NameValue != B.NameValue;
			case EParameterType::Text: return !A.TextValue.EqualTo(B.TextValue);
			case EParameterType::Boolean: return A.BooleanValue != B.BooleanValue;
			case EParameterType::Vector: return A.VectorValue != B.VectorValue;
			case EParameterType::Rotator: return A.RotatorValue != B.RotatorValue;
			case EParameterType::Color: return A.ColorValue != B.ColorValue;
			case EParameterType::Tag: return A.TagValue != B.TagValue;
			case EParameterType::Tags: return A.TagsValue != B.TagsValue;
			case EParameterType::Class: return A.ClassValue != B.ClassValue;
			case EParameterType::ClassPtr: return A.ClassPtrValue != B.ClassPtrValue;
			case EParameterType::Object: return A.ObjectValue != B.ObjectValue;
			case EParameterType::ObjectPtr: return A.ObjectPtrValue != B.ObjectPtrValue;
			case EParameterType::Delegate: return A.DelegateValue != B.DelegateValue;
			case EParameterType::Pointer: return A.PointerValue != B.PointerValue;
			default: ;
		}
		return false;
	}

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EParameterType ParameterType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText Description;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 IntegerValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float FloatValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 ByteValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FEnumParameterValue EnumValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString StringValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName NameValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (MultiLine))
	FText TextValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool BooleanValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector VectorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FRotator RotatorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor ColorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FKey KeyValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTag TagValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTagContainer TagsValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UClass* ClassValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSoftClassPtr<UObject> ClassPtrValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UObject* ObjectValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSoftObjectPtr<UObject> ObjectPtrValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSimpleDynamicDelegate DelegateValue;

	void* PointerValue;

public:
	//////////////////////////////////////////////////////////////////////////
	void SetParameterValue(const FParameter& InParameter)
	{
		const FText _Description = Description;
		*this = InParameter;
		Description = _Description;
	}

	//////////////////////////////////////////////////////////////////////////
	EParameterType GetParameterType() const { return ParameterType; }

	void SetParameterType(EParameterType InParameterType) { ParameterType = InParameterType; }

	//////////////////////////////////////////////////////////////////////////
	FText GetDescription() const { return Description; }

	void SetDescription(const FText& InDescription) { Description = InDescription; }

	//////////////////////////////////////////////////////////////////////////
	int32 GetIntegerValue() const { return IntegerValue; }

	void SetIntegerValue(int32 InValue) { IntegerValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	float GetFloatValue() const { return FloatValue; }

	void SetFloatValue(float InValue) { FloatValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	uint8 GetByteValue() const { return ByteValue; }

	void SetByteValue(uint8 InValue) { ByteValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	FEnumParameterValue GetEnumValue() const { return EnumValue; }

	void SetEnumValue(const FEnumParameterValue& InValue) { EnumValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	FString GetStringValue() const { return StringValue; }

	void SetStringValue(const FString& InValue) { StringValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	FName GetNameValue() const { return NameValue; }

	void SetNameValue(const FName InValue) { NameValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	FText GetTextValue() const { return TextValue; }

	void SetTextValue(const FText InValue) { TextValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	bool GetBooleanValue() const { return BooleanValue; }

	void SetBooleanValue(bool InValue) { BooleanValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	FVector GetVectorValue() const { return VectorValue; }

	void SetVectorValue(const FVector2D& InValue) { VectorValue = FVector(InValue.X, InValue.Y, 0.f); }

	void SetVectorValue(const FVector& InValue) { VectorValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	FRotator GetRotatorValue() const { return RotatorValue; }

	void SetRotatorValue(const FRotator& InValue) { RotatorValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	FColor GetColorValue() const { return ColorValue; }

	void SetColorValue(const FColor& InValue) { ColorValue = InValue; }

	void SetColorValue(const FLinearColor& InValue) { ColorValue = InValue.ToFColorSRGB(); }

	//////////////////////////////////////////////////////////////////////////
	FKey GetKeyValue() const { return KeyValue; }

	void SetKeyValue(const FKey& InKeyValue) { KeyValue = InKeyValue; }

	//////////////////////////////////////////////////////////////////////////
	FGameplayTag GetTagValue() const { return TagValue; }

	void SetTagValue(const FGameplayTag& InTagValue) { TagValue = InTagValue; }

	//////////////////////////////////////////////////////////////////////////
	FGameplayTagContainer GetTagsValue() const { return TagsValue; }

	void SetTagsValue(const FGameplayTagContainer& InTagValue) { TagsValue = InTagValue; }

	//////////////////////////////////////////////////////////////////////////
	UClass* GetClassValue() const { return ClassValue; }
	
	template<class T>
	TSubclassOf<T> GetClassValue() const { return ClassValue; }

	void SetClassValue(UClass* InValue) { ClassValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	template<class T = UObject>
	TSoftClassPtr<T> GetClassPtrValue() const { return ClassPtrValue.LoadSynchronous(); }

	template<class T = UObject>
	void SetClassPtrValue(const TSoftClassPtr<T>& InValue) { ClassPtrValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	UObject* GetObjectValue() const { return ObjectValue; }

	template<class T>
	T* GetObjectValue() const { return Cast<T>(ObjectValue); }

	void SetObjectValue(UObject* InValue) { ObjectValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	template<class T = UObject>
	TSoftObjectPtr<T> GetObjectPtrValue() const { return TSoftObjectPtr<T>(ObjectPtrValue.ToSoftObjectPath()); }

	template<class T = UObject>
	void SetObjectPtrValue(const TSoftObjectPtr<T>& InValue) { ObjectPtrValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	FSimpleDynamicDelegate GetDelegateValue() const { return DelegateValue; }

	void SetDelegateValue(const FSimpleDynamicDelegate& InValue) { DelegateValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	void* GetPointerValue() const { return PointerValue; }

	template<class T>
	T* GetPointerValue() const { return static_cast<T*>(PointerValue); }

	template<class T>
	T& GetPointerValueRef() const { return *GetPointerValue<T>(); }

	void SetPointerValue(void* InValue) { PointerValue = InValue; }

	void SetPointerValue(const void* InValue) { PointerValue = const_cast<void*>(InValue); }

public:
	static FParameter MakeInteger(int32 InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Integer;
		Parameter.Description = InDescription;
		Parameter.SetIntegerValue(InValue);
		return Parameter;
	}

	static FParameter MakeFloat(float InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Float;
		Parameter.Description = InDescription;
		Parameter.SetFloatValue(InValue);
		return Parameter;
	}

	static FParameter MakeByte(uint8 InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Byte;
		Parameter.Description = InDescription;
		Parameter.SetByteValue(InValue);
		return Parameter;
	}

	static FParameter MakeEnum(const FEnumParameterValue& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Enum;
		Parameter.Description = InDescription;
		Parameter.SetEnumValue(InValue);
		return Parameter;
	}

	static FParameter MakeString(const FString& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::String;
		Parameter.Description = InDescription;
		Parameter.SetStringValue(InValue);
		return Parameter;
	}

	static FParameter MakeName(const FName InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Name;
		Parameter.Description = InDescription;
		Parameter.SetNameValue(InValue);
		return Parameter;
	}

	static FParameter MakeText(const FText& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Text;
		Parameter.Description = InDescription;
		Parameter.SetTextValue(InValue);
		return Parameter;
	}

	static FParameter MakeBoolean(bool InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Boolean;
		Parameter.Description = InDescription;
		Parameter.SetBooleanValue(InValue);
		return Parameter;
	}

	static FParameter MakeVector(const FVector& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Vector;
		Parameter.Description = InDescription;
		Parameter.SetVectorValue(InValue);
		return Parameter;
	}

	static FParameter MakeVector(const FVector2D& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Vector;
		Parameter.Description = InDescription;
		Parameter.SetVectorValue(InValue);
		return Parameter;
	}

	static FParameter MakeRotator(const FRotator& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Rotator;
		Parameter.Description = InDescription;
		Parameter.SetRotatorValue(InValue);
		return Parameter;
	}

	static FParameter MakeColor(const FColor& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Color;
		Parameter.Description = InDescription;
		Parameter.SetColorValue(InValue);
		return Parameter;
	}

	static FParameter MakeColor(const FLinearColor& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Color;
		Parameter.Description = InDescription;
		Parameter.SetColorValue(InValue);
		return Parameter;
	}

	static FParameter MakeKey(const FKey& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Key;
		Parameter.Description = InDescription;
		Parameter.SetKeyValue(InValue);
		return Parameter;
	}

	static FParameter MakeTag(const FGameplayTag& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Tag;
		Parameter.Description = InDescription;
		Parameter.SetTagValue(InValue);
		return Parameter;
	}

	static FParameter MakeTags(const FGameplayTagContainer& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Tags;
		Parameter.Description = InDescription;
		Parameter.SetTagsValue(InValue);
		return Parameter;
	}

	static FParameter MakeClass(UClass* InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Class;
		Parameter.Description = InDescription;
		Parameter.SetClassValue(InValue);
		return Parameter;
	}

	template<class T>
	static FParameter MakeClassPtr(const TSoftClassPtr<T>& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::ClassPtr;
		Parameter.Description = InDescription;
		Parameter.SetClassPtrValue<T>(InValue);
		return Parameter;
	}

	static FParameter MakeObject(UObject* InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Object;
		Parameter.Description = InDescription;
		Parameter.SetObjectValue(InValue);
		return Parameter;
	}

	template<class T>
	static FParameter MakeObjectPtr(const TSoftObjectPtr<T>& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::ObjectPtr;
		Parameter.Description = InDescription;
		Parameter.SetObjectPtrValue<T>(InValue);
		return Parameter;
	}

	static FParameter MakeDelegate(const FSimpleDynamicDelegate& InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Delegate;
		Parameter.Description = InDescription;
		Parameter.SetDelegateValue(InValue);
		return Parameter;
	}

	static FParameter MakePointer(const void* InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Pointer;
		Parameter.Description = InDescription;
		Parameter.SetPointerValue(InValue);
		return Parameter;
	}

	static FParameter MakePointer(void* InValue, const FText& InDescription = FText::GetEmpty())
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Pointer;
		Parameter.Description = InDescription;
		Parameter.SetPointerValue(InValue);
		return Parameter;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameterSet
{
	GENERATED_USTRUCT_BODY()

public:
	FParameterSet()
	{
		Name = NAME_None;
		bRegistered = false;
		Category = FText::GetEmpty();
		Parameter = FParameter();
	}

	FParameterSet(FName InName, const FParameter& InParameter, const FText& InCategory = FText::GetEmpty(), bool bInRegistered = false)
	{
		Name = InName;
		bRegistered = bInRegistered;
		Category = InCategory;
		Parameter = InParameter;
	}

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bRegistered;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "bRegistered"))
	FText Category;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FParameter Parameter;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameters
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (TitleProperty = "Name"))
	TArray<FParameterSet> Sets;

public:
	//////////////////////////////////////////////////////////////////////////
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

public:
	virtual ~FParamData() = default;

	FParamData()
	{
	}

public:
	virtual void FromParams(const TArray<FParameter>& InParams)
	{
		
	}
	
	virtual TArray<FParameter> ToParams() const
	{
		return TArray<FParameter>();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameterMap
{
	GENERATED_USTRUCT_BODY()

public:
	FParameterMap()
	{
		Map = TMap<FString, FString>(); 
	}

	FParameterMap(const TMap<FString, FString>& InMap)
	{
		Map = InMap;
	}

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FString, FString> Map;

public:
	void Add(const FString& Key, const FString& Value)
	{
		if(!Map.Contains(Key))
		{
			Map.Add(Key, Value);
		}
	}

	void Set(const FString& Key, const FString& Value)
	{
		if(Contains(Key))
		{
			Map[Key] = Value;
		}
	}

	void Remove(const FString& Key)
	{
		if(Map.Contains(Key))
		{
			Map.Remove(Key);
		}
	}

	void Clear()
	{
		Map.Empty();
	}

public:
	bool Contains(const FString& Key) const
	{
		return Map.Contains(Key);
	}

	FString Get(const FString& Key) const
	{
		if(Contains(Key))
		{
			return Map[Key];
		}
		return TEXT("");
	}

	const TMap<FString, FString>& GetMap() const
	{
		return Map;
	}

	int32 GetNum() const
	{
		return Map.Num();
	}

	FString ToString() const
	{
		FString String;
		for(auto& Iter : Map)
		{
			String.Append(FString::Printf(TEXT("%s=%s,"), *Iter.Key, *Iter.Value));
		}
		String.RemoveFromEnd(TEXT(","));
		return String;
	}

	FString ToJsonString() const
	{
		FString JsonString;
		const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
		JsonWriter->WriteObjectStart();
		for (auto& Iter : Map)
		{
			JsonWriter->WriteValue(Iter.Key, Iter.Value);
		}
		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();
		return JsonString;
	}
};

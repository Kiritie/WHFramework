// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Json.h"
#include "GameplayTagContainer.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "ParameterModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct FParameterMap
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

	TMap<FString, FString>& GetMap()
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
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonString);
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

UENUM(BlueprintType)
enum class EParameterType : uint8
{
	None,
	Integer,
	Float,
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
	Pointer
};

USTRUCT(BlueprintType)
struct FParameter
{
	GENERATED_USTRUCT_BODY()

public:
	FParameter()
	{
		ParameterType = EParameterType::None;
		IntegerValue = 0;
		FloatValue = 0.f;
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

	friend bool operator==(const FParameter& A, const FParameter& B)
	{
		switch (A.ParameterType)
		{
			case EParameterType::Integer: return A.IntegerValue == B.IntegerValue;
			case EParameterType::Float: return A.FloatValue == B.FloatValue;
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
			case EParameterType::Pointer: return A.PointerValue != B.PointerValue;
			default: ;
		}
		return false;
	}

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EParameterType ParameterType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Integer"))
	int32 IntegerValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Float"))
	float FloatValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::String"))
	FString StringValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Name"))
	FName NameValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Text", MultiLine = "true"))
	FText TextValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Boolean"))
	bool BooleanValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Vector"))
	FVector VectorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Rotator"))
	FRotator RotatorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Color"))
	FColor ColorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Key"))
	FKey KeyValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Tag"))
	FGameplayTag TagValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Tags"))
	FGameplayTagContainer TagsValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Class"))
	UClass* ClassValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::ClassPtr"))
	TSoftClassPtr<UObject> ClassPtrValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Object"))
	UObject* ObjectValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::ObjectPtr"))
	TSoftObjectPtr<UObject> ObjectPtrValue;

	void* PointerValue;

public:
	//////////////////////////////////////////////////////////////////////////
	EParameterType GetParameterType() const { return ParameterType; }

	void SetParameterType(EParameterType InParameterType) { ParameterType = InParameterType; }

	//////////////////////////////////////////////////////////////////////////
	int32 GetIntegerValue() const { return IntegerValue; }

	void SetIntegerValue(int32 InValue) { IntegerValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	float GetFloatValue() const { return FloatValue; }

	void SetFloatValue(float InValue) { FloatValue = InValue; }

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
	TSoftObjectPtr<T> GetObjectPtrValue() const { return ObjectPtrValue.LoadSynchronous(); }

	template<class T = UObject>
	void SetObjectPtrValue(const TSoftObjectPtr<T>& InValue) { ObjectPtrValue = InValue; }

	//////////////////////////////////////////////////////////////////////////
	void* GetPointerValue() const { return PointerValue; }

	template<class T>
	T* GetPointerValue() const { return static_cast<T*>(PointerValue); }

	template<class T>
	T& GetPointerValueRef() const { return *GetPointerValue<T>(); }

	void SetPointerValue(void* InValue) { PointerValue = InValue; }

public:
	static FParameter MakeInteger(int32 InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Integer;
		Parameter.SetIntegerValue(InValue);
		return Parameter;
	}

	static FParameter MakeFloat(float InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Float;
		Parameter.SetFloatValue(InValue);
		return Parameter;
	}

	static FParameter MakeString(const FString& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::String;
		Parameter.SetStringValue(InValue);
		return Parameter;
	}

	static FParameter MakeName(const FName InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Name;
		Parameter.SetNameValue(InValue);
		return Parameter;
	}

	static FParameter MakeText(const FText InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Text;
		Parameter.SetTextValue(InValue);
		return Parameter;
	}

	static FParameter MakeBoolean(bool InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Boolean;
		Parameter.SetBooleanValue(InValue);
		return Parameter;
	}

	static FParameter MakeVector(const FVector& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Vector;
		Parameter.SetVectorValue(InValue);
		return Parameter;
	}

	static FParameter MakeVector(const FVector2D& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Vector;
		Parameter.SetVectorValue(InValue);
		return Parameter;
	}

	static FParameter MakeRotator(const FRotator& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Rotator;
		Parameter.SetRotatorValue(InValue);
		return Parameter;
	}

	static FParameter MakeColor(const FColor& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Color;
		Parameter.SetColorValue(InValue);
		return Parameter;
	}

	static FParameter MakeColor(const FLinearColor& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Color;
		Parameter.SetColorValue(InValue);
		return Parameter;
	}

	static FParameter MakeKey(const FKey& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Key;
		Parameter.SetKeyValue(InValue);
		return Parameter;
	}

	static FParameter MakeTag(const FGameplayTag& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Tag;
		Parameter.SetTagValue(InValue);
		return Parameter;
	}

	static FParameter MakeTags(const FGameplayTagContainer& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Tags;
		Parameter.SetTagsValue(InValue);
		return Parameter;
	}

	static FParameter MakeClass(UClass* InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Class;
		Parameter.SetClassValue(InValue);
		return Parameter;
	}

	template<class T>
	static FParameter MakeClassPtr(const TSoftClassPtr<T>& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::ClassPtr;
		Parameter.SetClassPtrValue<T>(InValue);
		return Parameter;
	}

	static FParameter MakeObject(UObject* InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Object;
		Parameter.SetObjectValue(InValue);
		return Parameter;
	}

	template<class T>
	static FParameter MakeObjectPtr(const TSoftObjectPtr<T>& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::ObjectPtr;
		Parameter.SetObjectPtrValue<T>(InValue);
		return Parameter;
	}

	static FParameter MakePointer(void* InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Pointer;
		Parameter.SetPointerValue(InValue);
		return Parameter;
	}
};

USTRUCT(BlueprintType)
struct FParameterSet
{
	GENERATED_USTRUCT_BODY()

public:
	FParameterSet()
	{
		Name = NAME_None;
		Parameter = FParameter();
	}

	FParameterSet(FName InName, FParameter InParameter)
	{
		Name = InName;
		Parameter = InParameter;
	}

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FParameter Parameter;
};

USTRUCT(BlueprintType)
struct FParameters
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FParameterSet> ParamSets;

public:
	//////////////////////////////////////////////////////////////////////////
	bool HasParameter(FName InName, bool bEnsured = true) const;

	void SetParameter(FName InName, FParameter InParameter);

	FParameter GetParameter(FName InName, bool bEnsured = true) const;

	TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const;

	void RemoveParameter(FName InName);

	void RemoveParameters(FName InName);

	void ClearAllParameter();
};

USTRUCT(BlueprintType)
struct FParamData
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
struct WHFRAMEWORK_API FParameterModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FParameterModuleSaveData()
	{
		Parameters = FParameters();
	}

public:
	UPROPERTY(BlueprintReadOnly)
	FParameters Parameters;
};

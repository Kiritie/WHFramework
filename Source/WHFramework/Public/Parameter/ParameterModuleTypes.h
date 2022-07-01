// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "json.h"
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
		for (auto& It : Map)
		{
			JsonWriter->WriteValue(It.Key, It.Value);
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
	Text,
	Boolean,
	Vector,
	Rotator,
	Color,
	Class,
	Object,
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
		TextValue = FText::GetEmpty();
		BooleanValue = false;
		VectorValue = FVector::ZeroVector;
		RotatorValue = FRotator::ZeroRotator;
		ClassValue = nullptr;
		ObjectValue = nullptr;
		PointerValue = nullptr;
	}

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EParameterType ParameterType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::Integer"))
	int32 IntegerValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::Float"))
	float FloatValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::String"))
	FString StringValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (MultiLine = "true", EditCondition = "ParameterType == EParameterType::Text"))
	FText TextValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::Boolean"))
	bool BooleanValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::Vector"))
	FVector VectorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::Rotator"))
	FRotator RotatorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::Rotator"))
	FColor ColorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::Class"))
	UClass* ClassValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "ParameterType == EParameterType::Object"))
	UObject* ObjectValue;

	void* PointerValue;

public:
	EParameterType GetParameterType() const { return ParameterType; }

	void SetParameterType(EParameterType InParameterType) { this->ParameterType = InParameterType; }

	int32 GetIntegerValue() const { return IntegerValue; }

	void SetIntegerValue(int32 InIntegerValue) { this->IntegerValue = InIntegerValue; }

	float GetFloatValue() const { return FloatValue; }

	void SetFloatValue(float InFloatValue) { this->FloatValue = InFloatValue; }

	FString GetStringValue() const { return StringValue; }

	void SetStringValue(const FString& InStringValue) { this->StringValue = InStringValue; }

	FText GetTextValue() const { return TextValue; }

	void SetTextValue(const FText InTextValue) { this->TextValue = InTextValue; }

	bool GetBooleanValue() const { return BooleanValue; }

	void SetBooleanValue(bool InBooleanValue) { BooleanValue = InBooleanValue; }

	FVector GetVectorValue() const { return VectorValue; }

	void SetVectorValue(const FVector& InVectorValue) { this->VectorValue = InVectorValue; }

	FRotator GetRotatorValue() const { return RotatorValue; }

	void SetRotatorValue(const FRotator& InRotatorValue) { this->RotatorValue = InRotatorValue; }

	FColor GetColorValue() const { return ColorValue; }

	void SetColorValue(const FColor& InColorValue) { this->ColorValue = InColorValue; }

	UClass* GetClassValue() const { return ClassValue; }

	void SetClassValue(UClass* InClassValue) { this->ClassValue = InClassValue; }

	UObject* GetObjectValue() const { return ObjectValue; }

	template<class T>
	T* GetObjectValue() const { return Cast<T>(ObjectValue); }

	void SetObjectValue(UObject* InObjectValue) { this->ObjectValue = InObjectValue; }

	void* GetPointerValue() const { return PointerValue; }

	template<class T>
	T* GetPointerValue() const { return static_cast<T*>(PointerValue); }

	template<class T>
	T& GetPointerValueRef() const { return *GetPointerValue<T>(); }

	void SetPointerValue(void* InPointerValue) { this->PointerValue = InPointerValue; }

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

	static FParameter MakeClass(UClass* InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Class;
		Parameter.SetClassValue(InValue);
		return Parameter;
	}

	static FParameter MakeObject(UObject* InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.ParameterType = EParameterType::Object;
		Parameter.SetObjectValue(InValue);
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

	//////////////////////////////////////////////////////////////////////////
	void SetIntegerParameter(FName InName, int32 InValue);

	int32 GetIntegerParameter(FName InName, bool bEnsured = true) const;

	TArray<int32> GetIntegerParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void SetFloatParameter(FName InName, float InValue);

	float GetFloatParameter(FName InName, bool bEnsured = true) const;

	TArray<float> GetFloatParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void SetStringParameter(FName InName, FString InValue);

	FString GetStringParameter(FName InName, bool bEnsured = true) const;

	TArray<FString> GetStringParameters(FName InName, bool bEnsured = true) const;
		
	//////////////////////////////////////////////////////////////////////////
	void SetTextParameter(FName InName, FText InValue);

	FText GetTextParameter(FName InName, bool bEnsured = true) const;

	TArray<FText> GetTextParameters(FName InName, bool bEnsured = true) const;

	//////////////////////////////////////////////////////////////////////////
	void SetBooleanParameter(FName InName, bool InValue);

	bool GetBooleanParameter(FName InName, bool bEnsured = true) const;

	TArray<bool> GetBooleanParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void SetVectorParameter(FName InName, FVector InValue);

	FVector GetVectorParameter(FName InName, bool bEnsured = true) const;

	TArray<FVector> GetVectorParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void SetRotatorParameter(FName InName, FRotator InValue);

	FRotator GetRotatorParameter(FName InName, bool bEnsured = true) const;

	TArray<FRotator> GetRotatorParameters(FName InName, bool bEnsured = true) const;
		
	//////////////////////////////////////////////////////////////////////////
	void SetColorParameter(FName InName, const FColor& InValue);

	FColor GetColorParameter(FName InName, bool bEnsured = true) const;

	TArray<FColor> GetColorParameters(FName InName, bool bEnsured = true) const;

	//////////////////////////////////////////////////////////////////////////
	void SetClassParameter(FName InName, UClass* InValue);

	UClass* GetClassParameter(FName InName, bool bEnsured = true) const;

	TArray<UClass*> GetClassParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void SetObjectParameter(FName InName, UObject* InValue);

	UObject* GetObjectParameter(FName InName, bool bEnsured = true) const;

	TArray<UObject*> GetObjectParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void SetPointerParameter(FName InName, void* InValue);

	void* GetPointerParameter(FName InName, bool bEnsured = true) const;

	TArray<void*> GetPointerParameters(FName InName, bool bEnsured = true) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FParameterSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FParameterSaveData()
	{
		Parameters = FParameters();
	}

public:
	UPROPERTY(BlueprintReadOnly)
	FParameters Parameters;
};


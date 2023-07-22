// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "json.h"

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
	
	FParameter(int32 InIntegerValue)
	{
		*this = MakeInteger(InIntegerValue);
	}
	
	FParameter(float InFloatValue)
	{
		*this = MakeFloat(InFloatValue);
	}
	
	FParameter(double InFloatValue)
	{
		*this = MakeFloat(InFloatValue);
	}
	
	FParameter(FString InStringValue)
	{
		*this = MakeString(InStringValue);
	}
	
	FParameter(bool InBooleanValue)
	{
		*this = MakeBoolean(InBooleanValue);
	}
	
	FParameter(FVector InVectorValue)
	{
		*this = MakeVector(InVectorValue);
	}
		
	FParameter(FVector2D InVectorValue)
	{
		*this = MakeVector(InVectorValue);
	}

	FParameter(FRotator InRotatorValue)
	{
		*this = MakeRotator(InRotatorValue);
	}
	
	FParameter(FColor InColorValue)
	{
		*this = MakeColor(InColorValue);
	}
		
	FParameter(FLinearColor InColorValue)
	{
		*this = MakeColor(InColorValue);
	}

	FParameter(UClass* InClassValue)
	{
		*this = MakeClass(InClassValue);
	}
	
	FParameter(UObject* InObjectValue)
	{
		*this = MakeObject(InObjectValue);
	}
	
	FParameter(void* InPointerValue)
	{
		*this = MakePointer(InPointerValue);
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Class"))
	UClass* ClassValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EParameterType::Object"))
	UObject* ObjectValue;

	void* PointerValue;

public:
	EParameterType GetParameterType() const { return ParameterType; }

	void SetParameterType(EParameterType InParameterType) { ParameterType = InParameterType; }

	int32 GetIntegerValue() const { return IntegerValue; }

	void SetIntegerValue(int32 InIntegerValue) { IntegerValue = InIntegerValue; }

	float GetFloatValue() const { return FloatValue; }

	void SetFloatValue(float InFloatValue) { FloatValue = InFloatValue; }

	FString GetStringValue() const { return StringValue; }

	void SetStringValue(const FString& InStringValue) { StringValue = InStringValue; }

	FText GetTextValue() const { return TextValue; }

	void SetTextValue(const FText InTextValue) { TextValue = InTextValue; }

	bool GetBooleanValue() const { return BooleanValue; }

	void SetBooleanValue(bool InBooleanValue) { BooleanValue = InBooleanValue; }

	FVector GetVectorValue() const { return VectorValue; }

	void SetVectorValue(const FVector2D& InVectorValue) { VectorValue = FVector(InVectorValue.X, InVectorValue.Y, 0.f); }

	void SetVectorValue(const FVector& InVectorValue) { VectorValue = InVectorValue; }

	FRotator GetRotatorValue() const { return RotatorValue; }

	void SetRotatorValue(const FRotator& InRotatorValue) { RotatorValue = InRotatorValue; }

	FColor GetColorValue() const { return ColorValue; }

	void SetColorValue(const FColor& InColorValue) { ColorValue = InColorValue; }

	void SetColorValue(const FLinearColor& InColorValue) { ColorValue = InColorValue.ToFColorSRGB(); }

	UClass* GetClassValue() const { return ClassValue; }

	void SetClassValue(UClass* InClassValue) { ClassValue = InClassValue; }

	UObject* GetObjectValue() const { return ObjectValue; }

	template<class T>
	T* GetObjectValue() const { return Cast<T>(ObjectValue); }

	void SetObjectValue(UObject* InObjectValue) { ObjectValue = InObjectValue; }

	void* GetPointerValue() const { return PointerValue; }

	template<class T>
	T* GetPointerValue() const { return static_cast<T*>(PointerValue); }

	template<class T>
	T& GetPointerValueRef() const { return *GetPointerValue<T>(); }

	void SetPointerValue(void* InPointerValue) { PointerValue = InPointerValue; }

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

	void AddParameter(FName InName, FParameter InParameter);

	void SetParameter(FName InName, FParameter InParameter);

	FParameter GetParameter(FName InName, bool bEnsured = true) const;

	TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const;

	//////////////////////////////////////////////////////////////////////////
	void RemoveParameter(FName InName);

	void RemoveParameters(FName InName);

	void ClearAllParameter();

	//////////////////////////////////////////////////////////////////////////
	void AddIntegerParameter(FName InName, int32 InValue);

	void SetIntegerParameter(FName InName, int32 InValue);

	int32 GetIntegerParameter(FName InName, bool bEnsured = true) const;

	TArray<int32> GetIntegerParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void AddFloatParameter(FName InName, float InValue);

	void SetFloatParameter(FName InName, float InValue);

	float GetFloatParameter(FName InName, bool bEnsured = true) const;

	TArray<float> GetFloatParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void AddStringParameter(FName InName, FString InValue);

	void SetStringParameter(FName InName, FString InValue);

	FString GetStringParameter(FName InName, bool bEnsured = true) const;

	TArray<FString> GetStringParameters(FName InName, bool bEnsured = true) const;
		
	//////////////////////////////////////////////////////////////////////////
	void AddTextParameter(FName InName, FText InValue);

	void SetTextParameter(FName InName, FText InValue);

	FText GetTextParameter(FName InName, bool bEnsured = true) const;

	TArray<FText> GetTextParameters(FName InName, bool bEnsured = true) const;

	//////////////////////////////////////////////////////////////////////////
	void AddBooleanParameter(FName InName, bool InValue);

	void SetBooleanParameter(FName InName, bool InValue);

	bool GetBooleanParameter(FName InName, bool bEnsured = true) const;

	TArray<bool> GetBooleanParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void AddVectorParameter(FName InName, FVector InValue);

	void SetVectorParameter(FName InName, FVector InValue);

	FVector GetVectorParameter(FName InName, bool bEnsured = true) const;

	TArray<FVector> GetVectorParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void AddRotatorParameter(FName InName, FRotator InValue);

	void SetRotatorParameter(FName InName, FRotator InValue);

	FRotator GetRotatorParameter(FName InName, bool bEnsured = true) const;

	TArray<FRotator> GetRotatorParameters(FName InName, bool bEnsured = true) const;
		
	//////////////////////////////////////////////////////////////////////////
	void AddColorParameter(FName InName, const FColor& InValue);

	void SetColorParameter(FName InName, const FColor& InValue);

	FColor GetColorParameter(FName InName, bool bEnsured = true) const;

	TArray<FColor> GetColorParameters(FName InName, bool bEnsured = true) const;

	//////////////////////////////////////////////////////////////////////////
	void AddClassParameter(FName InName, UClass* InValue);

	void SetClassParameter(FName InName, UClass* InValue);

	UClass* GetClassParameter(FName InName, bool bEnsured = true) const;

	TArray<UClass*> GetClassParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void AddObjectParameter(FName InName, UObject* InValue);

	void SetObjectParameter(FName InName, UObject* InValue);

	UObject* GetObjectParameter(FName InName, bool bEnsured = true) const;

	TArray<UObject*> GetObjectParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void AddPointerParameter(FName InName, void* InValue);

	void SetPointerParameter(FName InName, void* InValue);

	void* GetPointerParameter(FName InName, bool bEnsured = true) const;

	TArray<void*> GetPointerParameters(FName InName, bool bEnsured = true) const;
};

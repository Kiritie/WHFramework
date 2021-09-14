// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ParameterModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct FParameter
{
	GENERATED_USTRUCT_BODY()

public:
	FParameter()
	{
		IntegerValue = 0;
		FloatValue = 0.f;
		StringValue = TEXT("");
		BooleanValue = false;
		VectorValue = FVector::ZeroVector;
		RotatorValue = FRotator::ZeroRotator;
		ClassValue = nullptr;
		ObjectValue = nullptr;
	}

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 IntegerValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float FloatValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString StringValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool BooleanValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector VectorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FRotator RotatorValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UClass* ClassValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UObject* ObjectValue;

public:
	int32 GetIntegerValue() const { return IntegerValue; }

	void SetIntegerValue(int32 InIntegerValue) { this->IntegerValue = InIntegerValue; }

	float GetFloatValue() const { return FloatValue; }

	void SetFloatValue(float InFloatValue) { this->FloatValue = InFloatValue; }

	FString GetStringValue() const { return StringValue; }

	void SetStringValue(const FString& InStringValue) { this->StringValue = InStringValue; }

	bool GetBooleanValue() const { return BooleanValue; }

	void SetBooleanValue(bool InBooleanValue) { BooleanValue = InBooleanValue; }

	FVector GetVectorValue() const { return VectorValue; }

	void SetVectorValue(const FVector& InVectorValue) { this->VectorValue = InVectorValue; }

	FRotator GetRotatorValue() const { return RotatorValue; }

	void SetRotatorValue(const FRotator& InRotatorValue) { this->RotatorValue = InRotatorValue; }

	UClass* GetClassValue() const { return ClassValue; }

	void SetClassValue(UClass* InClassValue) { this->ClassValue = InClassValue; }

	UObject* GetObjectValue() const { return ObjectValue; }

	template<class T>
	T* GetObjectValue() const { return Cast<T>(ObjectValue); }

	void SetObjectValue(UObject* InObjectValue) { this->ObjectValue = InObjectValue; }

public:
	static FParameter MakeInteger(int32 InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.SetIntegerValue(InValue);
		return Parameter;
	}

	static FParameter MakeFloat(float InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.SetFloatValue(InValue);
		return Parameter;
	}

	static FParameter MakeString(const FString& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.SetStringValue(InValue);
		return Parameter;
	}

	static FParameter MakeBoolean(bool InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.SetBooleanValue(InValue);
		return Parameter;
	}

	static FParameter MakeVector(const FVector& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.SetVectorValue(InValue);
		return Parameter;
	}

	static FParameter MakeRotator(const FRotator& InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.SetRotatorValue(InValue);
		return Parameter;
	}

	static FParameter MakeClass(UClass* InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.SetClassValue(InValue);
		return Parameter;
	}

	static FParameter MakeObject(UObject* InValue)
	{
		FParameter Parameter = FParameter();
		Parameter.SetObjectValue(InValue);
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
	void SetClassParameter(FName InName, UClass* InValue);

	UClass* GetClassParameter(FName InName, bool bEnsured = true) const;

	TArray<UClass*> GetClassParameters(FName InName, bool bEnsured = true) const;
	
	//////////////////////////////////////////////////////////////////////////
	void SetObjectParameter(FName InName, UObject* InValue);

	UObject* GetObjectParameter(FName InName, bool bEnsured = true) const;

	TArray<UObject*> GetObjectParameters(FName InName, bool bEnsured = true) const;
};

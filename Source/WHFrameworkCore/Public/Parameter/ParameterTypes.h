#pragma once

#include "Json.h"
#include "Parameter/ParameterValueAdapter.h"
#include "WHFrameworkCoreTypes.h"
#include "ParameterTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameter
{
	GENERATED_BODY()

	FParameter();
	FParameter(const FParameter&) = default;
	FParameter(FParameter&&) = default;
	FParameter& operator=(const FParameter&) = default;
	FParameter& operator=(FParameter&&) = default;

	template<typename T>
		requires CParameterCompatible<T> && (!std::is_same_v<std::decay_t<T>, FParameter>)
	FParameter(T&& InValue)
	{
		Set(Forward<T>(InValue));
	}

	template<typename T>
		requires CParameterCompatible<T> && (!std::is_same_v<std::decay_t<T>, FParameter>)
	FParameter& operator=(T&& InValue)
	{
		Set(Forward<T>(InValue));
		return *this;
	}

	template<typename T>
		requires CParameterCompatible<T>
	void Set(T&& InValue)
	{
		TParameterValueAdapter<std::decay_t<T>>::Set(Value, Forward<T>(InValue));
	}

	template<typename T>
	bool TryGet(T& OutValue) const
	{
		using Adapter = TParameterValueAdapter<std::decay_t<T>>;
		if constexpr(Adapter::bSupported)
		{
			return Adapter::Get(Value, OutValue);
		}
		else
		{
			return false;
		}
	}

	template<typename T>
	T Get(const T& DefaultValue = T{}) const
	{
		T Result;
		return TryGet(Result) ? Result : DefaultValue;
	}

	template<typename T>
	bool Is() const
	{
		using Adapter = TParameterValueAdapter<std::decay_t<T>>;
		if constexpr(!Adapter::bSupported)
		{
			return false;
		}
		else if constexpr(requires { Adapter::Is(Value); })
		{
			return Adapter::Is(Value);
		}
		else
		{
			T Result{};
			return Adapter::Get(Value, Result);
		}
	}

	template<typename T>
		requires CParameterReferenceable<T>
	const std::decay_t<T>* GetPtr() const
	{
		using ValueType = std::decay_t<T>;
		using Adapter = TParameterValueAdapter<ValueType>;
		return Adapter::GetPtr(Value);
	}

	template<typename T>
		requires CParameterReferenceable<T>
	const std::decay_t<T>& GetRef() const
	{
		const std::decay_t<T>* Result = GetPtr<T>();
		checkf(Result, TEXT("FParameter value type mismatch."));
		return *Result;
	}

	template<typename T>
		requires CParameterReferenceable<T>
	std::decay_t<T>* GetMutablePtr()
	{
		using ValueType = std::decay_t<T>;
		using Adapter = TParameterValueAdapter<ValueType>;
		if constexpr(requires { Adapter::GetMutablePtr(Value); })
		{
			return Adapter::GetMutablePtr(Value);
		}
		else
		{
			return nullptr;
		}
	}

	const UScriptStruct* GetValueStruct() const
	{
		return Value.GetScriptStruct();
	}

	bool HasValue() const
	{
		return Value.IsValid();
	}

	void Reset()
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
	FText GetDescription() const
	{
		return Description;
	}
	void SetDescription(const FText& InDescription)
	{
		Description = InDescription;
	}

private:
	friend class UParameterModuleStatics;

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
	FParameterSet(FName InName, const FParameter& InParam, const FText& InCategory = FText::GetEmpty(), bool bInRegistered = false)
		: Name(InName), bRegistered(bInRegistered), Category(InCategory), Parameter(InParam)
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
struct WHFRAMEWORKCORE_API FParameterSets
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "Name"))
	TArray<FParameterSet> Sets;
	bool HasParameter(FName InName, bool bEnsured = true) const;
	void SetParameter(FName InName, const FParameter& InParam);
	FParameter GetParameter(FName InName, bool bEnsured = true) const;
	TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const;
	void RemoveParameter(FName InName);
	void RemoveParameters(FName InName);
	void ClearAllParameter();

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

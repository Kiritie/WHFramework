#pragma once

#include "Parameter/ParameterValueTypes.h"
#include "StructUtils/InstancedStruct.h"

template<typename T, typename Enable = void>
struct TParameterValueAdapter
{
	static constexpr bool bSupported = false;
	static constexpr bool bReferenceable = false;
};

template<typename T, typename = void>
struct TIsParameterStructValue : std::false_type
{
};

template<typename T>
struct TIsParameterStructValue<T, std::void_t<decltype(T::StaticStruct())>>
	: std::bool_constant<!std::is_base_of_v<FParameterValueBase, T>>
{
};

template<typename T>
struct TParameterValueAdapter<T, std::enable_if_t<TIsParameterStructValue<T>::value>>
{
	using WrapperType = FParameterStructValue;
	static constexpr bool bSupported = true;
	static constexpr bool bReferenceable = true;

	template<typename U>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, U&& InValue)
	{
		Storage.template InitializeAs<WrapperType>().Value.template InitializeAs<T>(Forward<U>(InValue));
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, T& OutValue)
	{
		const T* Value = GetPtr(Storage);
		if(!Value)
		{
			return false;
		}

		OutValue = *Value;
		return true;
	}

	static const T* GetPtr(const TInstancedStruct<FParameterValueBase>& Storage)
	{
		const WrapperType* Wrapper = Storage.template GetPtr<WrapperType>();
		return Wrapper ? Wrapper->Value.template GetPtr<T>() : nullptr;
	}

	static bool Is(const TInstancedStruct<FParameterValueBase>& Storage)
	{
		return GetPtr(Storage) != nullptr;
	}
};

template<typename T>
struct TParameterValueAdapter<T, std::enable_if_t<std::is_base_of_v<FParameterValueBase, T>>>
{
	using WrapperType = T;

	static constexpr bool bSupported = true;
	static constexpr bool bReferenceable = true;

	template<typename U>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, U&& InValue)
	{
		Storage.template InitializeAs<T>(Forward<U>(InValue));
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, T& OutValue)
	{
		const T* Value = GetPtr(Storage);
		if(!Value)
		{
			return false;
		}

		OutValue = *Value;
		return true;
	}

	static const T* GetPtr(const TInstancedStruct<FParameterValueBase>& Storage)
	{
		return Storage.template GetPtr<T>();
	}

	static bool Is(const TInstancedStruct<FParameterValueBase>& Storage)
	{
		return GetPtr(Storage) != nullptr;
	}
};

#define WH_PARAMETER_VALUE_ADAPTER(NativeType, ParameterWrapper) \
	template<> \
	struct TParameterValueAdapter<NativeType> \
	{ \
		using WrapperType = ParameterWrapper; \
		static constexpr bool bSupported = true; \
		static constexpr bool bReferenceable = true; \
		template<typename T> \
		static void Set(TInstancedStruct<FParameterValueBase>& Storage, T&& InValue) \
		{ \
			Storage.template InitializeAs<WrapperType>().Value = Forward<T>(InValue); \
		} \
		static const NativeType* GetPtr(const TInstancedStruct<FParameterValueBase>& Storage) \
		{ \
			const WrapperType* Wrapper = Storage.template GetPtr<WrapperType>(); \
			return Wrapper ? &Wrapper->Value : nullptr; \
		} \
		static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, NativeType& OutValue) \
		{ \
			const NativeType* Value = GetPtr(Storage); \
			if(!Value) \
			{ \
				return false; \
			} \
			OutValue = *Value; \
			return true; \
		} \
		static bool Is(const TInstancedStruct<FParameterValueBase>& Storage) \
		{ \
			return GetPtr(Storage) != nullptr; \
		} \
	};

WH_PARAMETER_VALUE_ADAPTER(bool, FParameterBoolValue)
WH_PARAMETER_VALUE_ADAPTER(uint8, FParameterByteValue)
WH_PARAMETER_VALUE_ADAPTER(int32, FParameterIntValue)
WH_PARAMETER_VALUE_ADAPTER(int64, FParameterInt64Value)
WH_PARAMETER_VALUE_ADAPTER(float, FParameterFloatValue)
WH_PARAMETER_VALUE_ADAPTER(double, FParameterDoubleValue)
WH_PARAMETER_VALUE_ADAPTER(FEnumParameterValue, FParameterEnumValue)
WH_PARAMETER_VALUE_ADAPTER(FString, FParameterStringValue)
WH_PARAMETER_VALUE_ADAPTER(FName, FParameterNameValue)
WH_PARAMETER_VALUE_ADAPTER(FText, FParameterTextValue)
WH_PARAMETER_VALUE_ADAPTER(FVector, FParameterVectorValue)
WH_PARAMETER_VALUE_ADAPTER(FRotator, FParameterRotatorValue)
WH_PARAMETER_VALUE_ADAPTER(FTransform, FParameterTransformValue)
WH_PARAMETER_VALUE_ADAPTER(FColor, FParameterColorValue)
WH_PARAMETER_VALUE_ADAPTER(FLinearColor, FParameterLinearColorValue)
WH_PARAMETER_VALUE_ADAPTER(FKey, FParameterKeyValue)
WH_PARAMETER_VALUE_ADAPTER(FGameplayTag, FParameterTagValue)
WH_PARAMETER_VALUE_ADAPTER(FGameplayTagContainer, FParameterTagsValue)
WH_PARAMETER_VALUE_ADAPTER(FSlateBrush, FParameterBrushValue)
WH_PARAMETER_VALUE_ADAPTER(FGuid, FParameterGuidValue)
WH_PARAMETER_VALUE_ADAPTER(FPrimaryAssetId, FParameterAssetIdValue)
WH_PARAMETER_VALUE_ADAPTER(FSimpleDynamicDelegate, FParameterDelegateValue)

#undef WH_PARAMETER_VALUE_ADAPTER

template<>
struct TParameterValueAdapter<const TCHAR*> : TParameterValueAdapter<FString>
{
	static constexpr bool bReferenceable = false;

	template<typename T>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, T&& InValue)
	{
		TParameterValueAdapter<FString>::Set(Storage, FString(Forward<T>(InValue)));
	}
};

template<>
struct TParameterValueAdapter<UClass*> : TParameterValueAdapter<FParameterClassValue>
{
	static constexpr bool bReferenceable = false;

	template<typename T>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, T&& InValue)
	{
		Storage.InitializeAs<FParameterClassValue>().Value = Forward<T>(InValue);
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, UClass*& OutValue)
	{
		const FParameterClassValue* Value = Storage.GetPtr<FParameterClassValue>();
		if(!Value)
		{
			return false;
		}

		OutValue = Value->Value.Get();
		return true;
	}
};

template<typename T>
struct TParameterValueAdapter<TSubclassOf<T>> : TParameterValueAdapter<FParameterClassValue>
{
	static constexpr bool bReferenceable = false;

	template<typename U>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, U&& InValue)
	{
		Storage.InitializeAs<FParameterClassValue>().Value = Forward<U>(InValue).Get();
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, TSubclassOf<T>& OutValue)
	{
		const FParameterClassValue* Value = Storage.GetPtr<FParameterClassValue>();
		if(!Value)
		{
			return false;
		}

		OutValue = Value->Value.Get();
		return true;
	}
};

template<typename T>
struct TParameterValueAdapter<TSoftClassPtr<T>> : TParameterValueAdapter<FParameterSoftClassValue>
{
	static constexpr bool bReferenceable = false;

	template<typename U>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, U&& InValue)
	{
		Storage.InitializeAs<FParameterSoftClassValue>().Value = TSoftClassPtr<UObject>(Forward<U>(InValue));
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, TSoftClassPtr<T>& OutValue)
	{
		const FParameterSoftClassValue* Value = Storage.GetPtr<FParameterSoftClassValue>();
		if(!Value)
		{
			return false;
		}

		OutValue = TSoftClassPtr<T>(Value->Value.ToSoftObjectPath());
		return true;
	}
};

template<typename T>
struct TParameterValueAdapter<T*, std::enable_if_t<std::is_base_of_v<UObject, T>>> : TParameterValueAdapter<FParameterObjectValue>
{
	static constexpr bool bReferenceable = false;

	template<typename U>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, U&& InValue)
	{
		Storage.InitializeAs<FParameterObjectValue>().Value = Forward<U>(InValue);
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, T*& OutValue)
	{
		const FParameterObjectValue* Value = Storage.GetPtr<FParameterObjectValue>();
		if(!Value)
		{
			return false;
		}

		OutValue = Cast<T>(Value->Value.Get());
		return OutValue != nullptr || Value->Value == nullptr;
	}
};

template<typename T>
struct TParameterValueAdapter<TObjectPtr<T>> : TParameterValueAdapter<FParameterObjectValue>
{
	static constexpr bool bReferenceable = false;

	template<typename U>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, U&& InValue)
	{
		Storage.InitializeAs<FParameterObjectValue>().Value = Forward<U>(InValue).Get();
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, TObjectPtr<T>& OutValue)
	{
		T* Object = nullptr;
		if(!TParameterValueAdapter<T*>::Get(Storage, Object))
		{
			return false;
		}

		OutValue = Object;
		return true;
	}
};

template<typename T>
struct TParameterValueAdapter<TScriptInterface<T>> : TParameterValueAdapter<FParameterObjectValue>
{
	static constexpr bool bReferenceable = false;

	template<typename U>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, U&& InValue)
	{
		Storage.InitializeAs<FParameterObjectValue>().Value = Forward<U>(InValue).GetObject();
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, TScriptInterface<T>& OutValue)
	{
		const FParameterObjectValue* Value = Storage.GetPtr<FParameterObjectValue>();
		if(!Value)
		{
			return false;
		}

		UObject* Object = Value->Value.Get();
		T* Interface = Object ? Cast<T>(Object) : nullptr;
		if(Object && !Interface)
		{
			return false;
		}

		OutValue.SetObject(Object);
		OutValue.SetInterface(Interface);
		return true;
	}
};

template<typename T>
struct TParameterValueAdapter<TSoftObjectPtr<T>> : TParameterValueAdapter<FParameterSoftObjectValue>
{
	static constexpr bool bReferenceable = false;

	template<typename U>
	static void Set(TInstancedStruct<FParameterValueBase>& Storage, U&& InValue)
	{
		Storage.InitializeAs<FParameterSoftObjectValue>().Value = TSoftObjectPtr<UObject>(Forward<U>(InValue));
	}

	static bool Get(const TInstancedStruct<FParameterValueBase>& Storage, TSoftObjectPtr<T>& OutValue)
	{
		const FParameterSoftObjectValue* Value = Storage.GetPtr<FParameterSoftObjectValue>();
		if(!Value)
		{
			return false;
		}

		OutValue = TSoftObjectPtr<T>(Value->Value.ToSoftObjectPath());
		return true;
	}
};

template<typename T>
concept CParameterCompatible = TParameterValueAdapter<std::decay_t<T>>::bSupported;

template<typename T>
concept CParameterReferenceable =
	TParameterValueAdapter<std::decay_t<T>>::bSupported &&
	TParameterValueAdapter<std::decay_t<T>>::bReferenceable;

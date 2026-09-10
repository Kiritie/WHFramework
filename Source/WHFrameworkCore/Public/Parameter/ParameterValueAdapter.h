#pragma once

#include "Parameter/ParameterValueTypes.h"
#include "StructUtils/InstancedStruct.h"

template <typename T, typename Enable = void> struct TParameterValueAdapter
{
	static constexpr bool bSupported = false;
};

template <typename T> struct TParameterValueAdapter<T, std::enable_if_t<std::is_base_of_v<FParameterValueBase, T>>>
{
	using WrapperType = T;
	static constexpr bool bSupported = true;

	template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)
	{
		OutValue.template InitializeAs<T>(Forward<U>(InValue));
	}

	static const T* GetPtr(const TInstancedStruct<FParameterValueBase>& InValue)
	{
		return InValue.template GetPtr<T>();
	}
	static T* GetMutablePtr(TInstancedStruct<FParameterValueBase>& InValue)
	{
		return InValue.template GetMutablePtr<T>();
	}
};

#define WH_PARAMETER_ADAPTER(ValueType, ParameterWrapper)                                                                                  \
	template <> struct TParameterValueAdapter<ValueType>                                                                                   \
	{                                                                                                                                      \
		using WrapperType = ParameterWrapper;                                                                                              \
		static constexpr bool bSupported = true;                                                                                           \
		template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)                                \
		{                                                                                                                                  \
			OutValue.template InitializeAs<WrapperType>().Value = Forward<U>(InValue);                                                     \
		}                                                                                                                                  \
		static const ValueType* GetPtr(const TInstancedStruct<FParameterValueBase>& InValue)                                               \
		{                                                                                                                                  \
			const WrapperType* Wrapper = InValue.template GetPtr<WrapperType>();                                                           \
			return Wrapper ? &Wrapper->Value : nullptr;                                                                                    \
		}                                                                                                                                  \
		static ValueType* GetMutablePtr(TInstancedStruct<FParameterValueBase>& InValue)                                                    \
		{                                                                                                                                  \
			WrapperType* Wrapper = InValue.template GetMutablePtr<WrapperType>();                                                          \
			return Wrapper ? &Wrapper->Value : nullptr;                                                                                    \
		}                                                                                                                                  \
	};

WH_PARAMETER_ADAPTER(int32, FParameterIntValue)
WH_PARAMETER_ADAPTER(float, FParameterFloatValue)
WH_PARAMETER_ADAPTER(uint8, FParameterByteValue)
WH_PARAMETER_ADAPTER(FEnumParameterValue, FParameterEnumValue)
WH_PARAMETER_ADAPTER(FString, FParameterStringValue)
WH_PARAMETER_ADAPTER(FName, FParameterNameValue)
WH_PARAMETER_ADAPTER(FText, FParameterTextValue)
WH_PARAMETER_ADAPTER(bool, FParameterBoolValue)
WH_PARAMETER_ADAPTER(FVector, FParameterVectorValue)
WH_PARAMETER_ADAPTER(FRotator, FParameterRotatorValue)
WH_PARAMETER_ADAPTER(FTransform, FParameterTransformValue)
WH_PARAMETER_ADAPTER(FColor, FParameterColorValue)
WH_PARAMETER_ADAPTER(FLinearColor, FParameterLinearColorValue)
WH_PARAMETER_ADAPTER(FKey, FParameterKeyValue)
WH_PARAMETER_ADAPTER(FGameplayTag, FParameterTagValue)
WH_PARAMETER_ADAPTER(FGameplayTagContainer, FParameterTagsValue)
WH_PARAMETER_ADAPTER(FSlateBrush, FParameterBrushValue)
WH_PARAMETER_ADAPTER(FGuid, FParameterGuidValue)
WH_PARAMETER_ADAPTER(FPrimaryAssetId, FParameterAssetIdValue)
WH_PARAMETER_ADAPTER(FSimpleDynamicDelegate, FParameterDelegateValue)

#undef WH_PARAMETER_ADAPTER

template <> struct TParameterValueAdapter<double> : TParameterValueAdapter<float>
{
	template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)
	{
		TParameterValueAdapter<float>::Set(OutValue, static_cast<float>(InValue));
	}
};

template <> struct TParameterValueAdapter<const TCHAR*> : TParameterValueAdapter<FString>
{
	template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)
	{
		TParameterValueAdapter<FString>::Set(OutValue, FString(Forward<U>(InValue)));
	}
};

template <int32 N> struct TParameterValueAdapter<const TCHAR[N]> : TParameterValueAdapter<const TCHAR*>
{
};

template <> struct TParameterValueAdapter<void*>
{
	using WrapperType = FParameterPointerValue;
	static constexpr bool bSupported = true;
	static void Set(TInstancedStruct<FParameterValueBase>& OutValue, void* InValue)
	{
		OutValue.InitializeAs<FParameterPointerValue>().Value = InValue;
	}
	static void* const* GetPtr(const TInstancedStruct<FParameterValueBase>& InValue)
	{
		const FParameterPointerValue* Wrapper = InValue.GetPtr<FParameterPointerValue>();
		return Wrapper ? &Wrapper->Value : nullptr;
	}
	static void** GetMutablePtr(TInstancedStruct<FParameterValueBase>& InValue)
	{
		FParameterPointerValue* Wrapper = InValue.GetMutablePtr<FParameterPointerValue>();
		return Wrapper ? &Wrapper->Value : nullptr;
	}
};

template <> struct TParameterValueAdapter<const void*> : TParameterValueAdapter<void*>
{
	static void Set(TInstancedStruct<FParameterValueBase>& OutValue, const void* InValue)
	{
		TParameterValueAdapter<void*>::Set(OutValue, const_cast<void*>(InValue));
	}
};

template <typename T> struct TParameterValueAdapter<T*, std::enable_if_t<!std::is_base_of_v<UObject, std::remove_cv_t<T>>>>
{
	using WrapperType = FParameterPointerValue;
	static constexpr bool bSupported = true;
	static void Set(TInstancedStruct<FParameterValueBase>& OutValue, T* InValue)
	{
		OutValue.InitializeAs<FParameterPointerValue>().Value = const_cast<std::remove_cv_t<T>*>(InValue);
	}
};

template <> struct TParameterValueAdapter<UClass*> : TParameterValueAdapter<FParameterClassValue>
{
	template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)
	{
		OutValue.template InitializeAs<FParameterClassValue>().Value = Forward<U>(InValue);
	}
};

template <typename T> struct TParameterValueAdapter<TSubclassOf<T>> : TParameterValueAdapter<FParameterClassValue>
{
	template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)
	{
		OutValue.template InitializeAs<FParameterClassValue>().Value = Forward<U>(InValue).Get();
	}
};

template <typename T> struct TParameterValueAdapter<TSoftClassPtr<T>> : TParameterValueAdapter<FParameterSoftClassValue>
{
	template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)
	{
		OutValue.template InitializeAs<FParameterSoftClassValue>().Value = TSoftClassPtr<UObject>(Forward<U>(InValue));
	}
};

template <typename T>
struct TParameterValueAdapter<T*, std::enable_if_t<std::is_base_of_v<UObject, T>>> : TParameterValueAdapter<FParameterObjectValue>
{
	template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)
	{
		OutValue.template InitializeAs<FParameterObjectValue>().Value = Forward<U>(InValue);
	}
};

template <typename T> struct TParameterValueAdapter<TSoftObjectPtr<T>> : TParameterValueAdapter<FParameterSoftObjectValue>
{
	template <typename U> static void Set(TInstancedStruct<FParameterValueBase>& OutValue, U&& InValue)
	{
		OutValue.template InitializeAs<FParameterSoftObjectValue>().Value = TSoftObjectPtr<UObject>(Forward<U>(InValue));
	}
};

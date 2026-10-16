#pragma once

#include "CoreMinimal.h"
#include <charconv>
#include <type_traits>

namespace CommonNumericText
{
	template<typename NumberType>
	bool Parse(const FString& Text, NumberType& OutValue)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		const FTCHARToUTF8 Utf8(*Trimmed);
		const char* First = Utf8.Get();
		const char* Last = First + Utf8.Length();
		if (First != Last && *First == '+')
		{
			++First;
			if (First != Last && (*First == '-' || *First == '+'))
			{
				return false;
			}
		}
		if (First == Last)
		{
			return false;
		}
		NumberType Parsed{};
		std::from_chars_result Result;
		if constexpr (std::is_floating_point_v<NumberType>)
		{
			Result = std::from_chars(First, Last, Parsed, std::chars_format::general);
		}
		else
		{
			Result = std::from_chars(First, Last, Parsed, 10);
		}
		if (Result.ec != std::errc() || Result.ptr != Last)
		{
			return false;
		}
		if constexpr (std::is_floating_point_v<NumberType>)
		{
			if (!FMath::IsFinite(Parsed))
			{
				return false;
			}
		}
		OutValue = Parsed;
		return true;
	}
}

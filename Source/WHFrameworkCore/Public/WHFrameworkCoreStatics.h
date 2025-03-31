#pragma once
#include "Dom/JsonObject.h"

class WHFRAMEWORKCORE_API FCoreStatics
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Serialize
	/*
	* 将目标对象数据保存到内存
	* @param InObject 目标对象
	* @param OutObjectData 保存到的字节数组
	*/
	static void SaveObjectDataToMemory(UObject* InObject, TArray<uint8>& OutObjectData);

	/*
	* 从内存加载目标对象数据
	* @param InObject 目标对象
	* @param InObjectData 目标对象字节数组
	*/
	static void LoadObjectDataFromMemory(UObject* InObject, const TArray<uint8>& InObjectData);

	/*
	* 导入目标对象暴露的参数
	* @param InObject 目标对象
	* @param InJsonObject 导入的Json对象
	*/
	static void ImportExposedProperties(UObject* InObject, TSharedPtr<FJsonObject> InJsonObject);
	
	/*
	* 导出目标对象暴露的参数
	* @param InObject = 目标对象
	* @param InJsonObject 导出的Json对象
	* @param bExportSubObjects 是否导出子对象
	*/
	static void ExportExposedProperties(UObject* InObject, TSharedPtr<FJsonObject>& InJsonObject, bool bExportSubObjects = false);
	
	/*
	* 导出源对象暴露的参数到目标对象
	* @param InSourceObject 源对象
	* @param InTargetObject 目标对象
	* @param bExportSubObjects 是否导出子对象
	*/
	static void ExportPropertiesToObject(UObject* InSourceObject, UObject* InTargetObject, bool bExportSubObjects = false);

	//////////////////////////////////////////////////////////////////////////
	// Regex
	/*
	* 正则表达式匹配字串，匹配到返回true，没匹配到返回false
	* @param InSourceStr 源字符串
	* @param InPattern 正则表达式匹配规则
	* @param OutResult 匹配到的字符串
	*/
	static bool RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult);

	//////////////////////////////////////////////////////////////////////////
	// Char
	static unsigned char ToHex(unsigned char InChar);

	static unsigned char FromHex(unsigned char InChar);

	//////////////////////////////////////////////////////////////////////////
	// String
	static bool SortString(const FString& InStr1, const FString& InStr2)
	{
		int32 Num1 = 0;
		int32 Num2 = 0;
		for(int32 k = 0; k < (InStr1.Len() < InStr2.Len() ? InStr1.Len() : InStr2.Len()); k++)
		{
			Num1 = InStr1.GetCharArray()[k];
			Num2 = InStr2.GetCharArray()[k];
			if(Num1 != Num2)
			{
				break;
			}
		}
		return Num1 >= Num2;
	}
	
	static FString BoolToString(bool InBool);

	static bool StringToBool(const FString& InString);
	
	static FString SanitizeFloat(double InFloat, int32 InMaxDigits = -1);

	//////////////////////////////////////////////////////////////////////////
	// Url
	static FString UrlEncode(const FString& InUrl);

	static FString UrlDecode(const FString& InUrl);

	//////////////////////////////////////////////////////////////////////////
	// Array
	template<class T>
	static void SortStringElementArray(TArray<T>& InArray, const TFunction<FString(const T&)>& InElementFunc, bool bAscending = true)
	{
		for(int32 i = 0; i < InArray.Num(); i++)
		{
			for(int32 j = i; j < InArray.Num(); j++)
			{
				const FString Str1 = InElementFunc(InArray[i]);
				const FString Str2 = InElementFunc(InArray[j]);
				if(bAscending ? SortString(Str1, Str2) : !SortString(Str1, Str2))
				{
					InArray.Swap(i, j);
				}
			}
		}
	}

	template <typename T>
	static void ShuffleArray(TArray<T>& InArray)
	{
		int32 N = InArray.Num();
		for (int32 I = 0; I < N; ++I)
		{
			InArray.Swap(I, FMath::RandRange(I, N - 1));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Texture
	static UTexture2D* LoadTextureFromFile(const FString& InFilePath);

	static void SaveTextureToFile(UTexture2D* InTexture, const FString& InFilePath);

	static UTexture2D* CompositeTextures(const TArray<UTexture2D*>& InTextures, FVector2D InTexSize, UTexture2D* InTemplate = nullptr);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "GlobalToolsBPLibrary.generated.h"

/*
* 数据保存档案
*/
struct FSaveDataArchive : public FObjectAndNameAsStringProxyArchive
{
	FSaveDataArchive(FArchive& InInnerArchive) : FObjectAndNameAsStringProxyArchive(InInnerArchive, false)
	{
		ArIsSaveGame = true;
	}
};

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UGlobalToolsBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	// Enum
public:
	/*
	 * 将枚举值转换为字符串
	 * @param InEnumName 枚举名称
	 * @param InEnumValue 枚举值
	 */
	UFUNCTION(BlueprintPure, Category = "GlobalTools")
	static FString EnumValueToString(const FString& InEnumName, int32 InEnumValue);

	//////////////////////////////////////////////////////////////////////////
	// Serialize
public:
	/*
	* 将目标对象数据保存到内存
	* @param InObject 目标对象
	* @param OutObjectData 保存到的字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "GlobalTools")
	static void SaveObjectToMemory(UObject* InObject, TArray<uint8>& OutObjectData);

	/*
	* 从内存加载目标对象数据
	* @param InObject 目标对象
	* @param InObjectData 目标对象字节数组
	*/
	UFUNCTION(BlueprintCallable, Category = "GlobalTools")
	static void LoadObjectFromMemory(UObject* InObject, const TArray<uint8>& InObjectData);

	/*
	* 解析目标对象带有 ExposeOnSpawn 标签的参数
	* @param InObject 目标对象
	* @param InParams 符合导出格式的参数键值对
	* @param bParamHavePropertyType Param的key是否含有属性类的前缀表示：<int> <Array>...
	*/
	UFUNCTION(BlueprintCallable,Category = "GlobalTools")
	static void SerializeExposedParam(UObject* InObject, const TMap<FString,FString>& InParams, bool bParamHavePropertyType = true);
	/*
	* 导出目标类带有 ExposeOnSpawn 标签的参数
	* @param InClass 目标类
	* @param OutParams 导出参数的存放键值对
	* @param bDisplayPropertyType Param的key是否显示属性类的前缀表示：<int> <Array>...
	*/
	UFUNCTION(BlueprintCallable,Category = "GlobalTools")
	static void ExportExposedParam(UClass* InClass, TMap<FString,FString>& OutParams, bool bDisplayPropertyType = true);

	//////////////////////////////////////////////////////////////////////////
	// Regex
public:
	/*
	* 正则表达式匹配字串，匹配到返回true，没匹配到返回false
	* @param InSourceStr 源字符串
	* @param InPattern 正则表达式匹配规则
	* @param OutResult 匹配到的字符串
	*/
	UFUNCTION(BlueprintPure, Category = "GlobalTools")
	static bool RegexMatch(const FString& InSourceStr, const FString& InPattern, TArray<FString>& OutResult);
};

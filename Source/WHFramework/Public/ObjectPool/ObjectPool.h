// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "UObject/NoExportTypes.h"
#include "ObjectPool.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UObjectPool : public UObject
{
	GENERATED_BODY()

public:
	UObjectPool();

private:
	/// 限制大小
	UPROPERTY()
	int32 Limit;
	/// 引用类型
	UPROPERTY()
	TSubclassOf<UObject> Type;
	/// 引用列表
	UPROPERTY()
	TArray<UObject*> List;

public:
	/**
	* 初始化
	* @param InLimit 限制大小
	* @param InType 类型
	*/
	void Initialize(int32 InLimit, TSubclassOf<UObject> InType);
	/**
	* 生成引用
	*/
	UObject* Spawn();
	/**
	* 回收引用
	* @param InObject 回收引用
	*/
	void Despawn(UObject* InObject);
	/**
	* 清理引用
	*/
	void Clear();

public:
	int32 GetLimit() const;

	int32 GetCount() const;

	TArray<UObject*> GetQueue() const;
};

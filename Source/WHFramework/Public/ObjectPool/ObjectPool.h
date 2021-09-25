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
	UPROPERTY(VisibleAnywhere)
	int32 Limit;
	/// 对象类型
	UPROPERTY(VisibleAnywhere)
	TSubclassOf<UObject> Type;
	/// 对象数量
	UPROPERTY(VisibleAnywhere)
	int32 Count;
	/// 对象列表
	TQueue<UObject*> Queue;

public:
	/**
	* 初始化
	* @param InLimit 限制大小
	* @param InType 类型
	*/
	void Initialize(int32 InLimit, TSubclassOf<UObject> InType);
	/**
	* 生成对象
	*/
	UObject* Spawn();
	/**
	* 回收对象
	* @param InObject 对象
	*/
	void Despawn(UObject* InObject);
	/**
	* 清理对象
	*/
	void Clear();

public:
	int32 GetLimit() const;

	int32 GetCount() const;
};

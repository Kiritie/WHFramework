// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterModuleTypes.h"

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

protected:
	/// 限制大小
	UPROPERTY(VisibleAnywhere)
	int32 Limit;
	/// 对象数量
	UPROPERTY(VisibleAnywhere)
	int32 Count;
	/// 对象类型
	UPROPERTY(VisibleAnywhere)
	TSubclassOf<UObject> Type;
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
	UObject* Spawn(const TArray<FParameter>& InParams);
	virtual UObject* SpawnImpl();
	/**
	* 回收对象
	* @param InObject 对象
	*/
	void Despawn(UObject* InObject);
	virtual void DespawnImpl(UObject* InObject);
	/**
	* 清理对象
	*/
	void Clear();

public:
	UFUNCTION(BlueprintPure)
	int32 GetLimit() const { return Limit; }

	UFUNCTION(BlueprintPure)
	int32 GetCount() const { return Count; }

	UFUNCTION(BlueprintPure)
	TSubclassOf<UObject> GetType() const { return Type; }

	TQueue<UObject*>& GetQueue() { return Queue; }
};

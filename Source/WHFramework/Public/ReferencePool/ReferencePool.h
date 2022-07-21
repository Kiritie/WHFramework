// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ReferencePool.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UReferencePool : public UObject
{
	GENERATED_BODY()

public:
	UReferencePool();

private:
	/// 引用类型
	UPROPERTY(VisibleAnywhere)
	TSubclassOf<UObject> Type;
	/// 引用对象
	UPROPERTY()
	UObject* Object;

public:
	/**
	* 初始化
	* @param InType 类型
	*/
	void Initialize(TSubclassOf<UObject> InType);
	/**
	* 创建引用
	*/
	void Create(UObject* InObject = nullptr);
	/**
	* 获取引用
	*/
	UObject& Get();
	/**
	* 设置引用
	*/
	void Set(UObject* InObject);
	/**
	* 重置引用
	*/
	void Reset();
	/**
	* 清理引用
	*/
	void Clear();

public:
	bool IsEmpty() const;
};

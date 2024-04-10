// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "WHFrameworkCoreTypes.h"

class WHFRAMEWORKCORE_API FManagerBase : public FUniqueClass
{
public:	
	// ParamSets default values for this actor's properties
	FManagerBase();

	virtual ~FManagerBase() override;

	static const FUniqueType Type;
		
public:
	/**
	* 当初始化
	*/
	virtual void OnInitialize();
	/**
	* 当刷新
	*/
	virtual void OnRefresh(float DeltaSeconds);
	/**
	* 当销毁
	*/
	virtual void OnTermination();
};

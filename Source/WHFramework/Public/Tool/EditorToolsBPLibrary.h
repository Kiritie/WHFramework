// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "EditorToolsBPLibrary.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UEditorToolsBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/* 
	 * 当前是否为编辑器状态运行
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EditorTools")
	static bool IsInEditor() { return GIsEditor; }
};

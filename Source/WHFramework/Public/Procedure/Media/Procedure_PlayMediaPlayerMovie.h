// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure_PlayMediaPlayerMovie.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UProcedure_PlayMediaPlayerMovie : public UProcedureBase
{
	GENERATED_BODY()

	
public:
	UProcedure_PlayMediaPlayerMovie();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
public:
	virtual void OnInitialize_Implementation() override;
	
	virtual void OnEnter_Implementation(UProcedureBase* InLastProcedure) override;

	virtual void OnLeave_Implementation() override;

protected:
	UFUNCTION()
	void ServerOnMoviePlayFinished(const FName& InMovieName);

	//////////////////////////////////////////////////////////////////////////
	/// Character
public:
	/// 角色名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Media")
	FName PlayerName;
	
	/// 视频播放名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Media")
	FName MovieName;
};

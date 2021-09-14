// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModule.h"

#include "Function/Sound/WHSoundHelperBPLibrary.h"
#include "SpeechRecognition/SpeechRecognitionHelperBPLibrary.h"

// Sets default values
AAssetModule::AAssetModule()
{
	ModuleName = FName("AssetModule");

}

#if WITH_EDITOR
void AAssetModule::OnGenerate_Implementation()
{
	
}

void AAssetModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AAssetModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	if(KnowledgeDataAsset)
	{
		for (auto& Iter : KnowledgeDataAsset->KnowledgeInfos)
		{
			if(!Iter.Value.QuestionSound && Iter.Value.QuestionSoundInfos.Num() > 0)
			{
				Iter.Value.QuestionSound = UWHSoundHelperBPLibrary::GetSynthesizeSpeechSounds(this, Iter.Value.QuestionSoundInfos);	
			}
			if(!Iter.Value.TeachingSound && Iter.Value.TeachingSoundInfos.Num() > 0)
			{
				Iter.Value.TeachingSound = UWHSoundHelperBPLibrary::GetSynthesizeSpeechSounds(this, Iter.Value.TeachingSoundInfos);	
			}
			if(Iter.Value.GrammarInfo.IsValid())
			{
				Iter.Value.Grammar = USpeechRecognitionHelperBPLibrary::CreateGrammarFull(Iter.Value.GrammarInfo);
			}
		}
	}
}

void AAssetModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAssetModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAssetModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AAssetModule::PreloadingSynthesizeSpeechSounds_Implementation(USynthesizeSpeechDownloadManager* SynthesizeSpeechDownloadManager)
{
	if(KnowledgeDataAsset)
	{
		for(auto& It : KnowledgeDataAsset->KnowledgeInfos)
		{
			It.Value.TryDownloadSentencesInfo(SynthesizeSpeechDownloadManager);
		}
	}

	if(CharacterDataAsset)
	{
		for(auto& It : CharacterDataAsset->CharacterInfos)
		{
			It.Value.TryDownloadSentencesInfo(SynthesizeSpeechDownloadManager);
		}
	}
}

FKnowledgeInfo AAssetModule::GetKnowledgeInfoByName(const FString& InName) const
{
	if(ensureEditor(KnowledgeDataAsset && KnowledgeDataAsset->KnowledgeInfos.Contains(InName)))
	{
		return KnowledgeDataAsset->KnowledgeInfos[InName];
	}
	return FKnowledgeInfo();
}

TSubclassOf<ATeachingItem> AAssetModule::GetTeachingItemClassByName(const FString& InName) const
{
	if(ensureEditor(KnowledgeDataAsset && KnowledgeDataAsset->TeachingItemClass))
	{
		return KnowledgeDataAsset->TeachingItemClass;
	}
	return nullptr; 
}

FCharacterInfo AAssetModule::GetCharacterInfoByName(const FName InName) const
{
	if (ensureEditor(CharacterDataAsset && CharacterDataAsset->CharacterInfos.Contains(InName)))
	{
		return CharacterDataAsset->CharacterInfos[InName];
	}
	return FCharacterInfo();
}

FTeacherMediaInfo AAssetModule::GetTeacherMediaDataAssetByName(const FName InName) const
{
	if (ensureEditor(TeacherMediaDataAsset && TeacherMediaDataAsset->TeacherMediaInfos.Contains(InName)))
	{
		return TeacherMediaDataAsset->TeacherMediaInfos[InName];
	}
	return FTeacherMediaInfo();
}

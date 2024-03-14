// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/SDownloadListItem.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"
#include "HAL/PlatformFileManager.h"
#include "Http/FileDownloader/FileDownloader.h"
#include "Zip/ZipManager.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SDownloadListItem::SDownloadListItem()
{
	bRecentItem = false;
	LastBytesReceived = 0;
	UpdateRemainTime = 0.f;
}

void SDownloadListItem::Construct(const FArguments& InArgs, TSharedPtr<SBasePanel> InDownloadPanel, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	DownloadPanel = InDownloadPanel;
	
	DownloadListItem = InArgs._DownloadListItem;
	DownloadListItem->FileDownloader->OnProgress.AddRaw(this, &SDownloadListItem::HandleDownloadProgress);
	DownloadListItem->FileDownloader->OnComplete.AddRaw(this, &SDownloadListItem::HandleDownloadComplete);

	bRecentItem = InArgs._IsRecentItem;
	
	STableRow::Construct
	(
		STableRow::FArguments()
		.Style(FWHFrameworkSlateStyle::Get(), "Main.List.Download")
		.ShowWires(false)
		.Padding(InArgs._Padding)
		.Content()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0.f, 0.f, 20.f, 0.f)
			[
				SNew(SImage)
				.Image(DownloadListItem->FileIcon)
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(FMargin(0.f, 0.f, 0.f, 5.f))
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
					.Text(FText::FromString(DownloadListItem->FileDownloader->FileName))
				]

				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.Text_Lambda([this](){ return FText::FromString(DownloadListItem->FileDownloader->FileSavePath); })
				]
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(1.f)
			[
				SNew(SSpacer)
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0.f, 0.f, 10.f, 0.f)
			[
				SAssignNew(Txt_DownloadProgress, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0.f, 0.f, 10.f, 0.f)
			[
				SAssignNew(Txt_DownloadRate, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(30.f)
				.HeightOverride(30.f)
				[
					SNew(SButton)
					.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Button.NoBorder_1"))
					.OnClicked(this, &SDownloadListItem::OnClearButtonClicked)
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Clear"))
						]
					]
				]
			]
		],
		InOwnerTableView
	);
}

void SDownloadListItem::HandleDownloadProgress(int64 BytesSent, int64 BytesReceived, int64 FullSize)
{
	const float Progress = (float)BytesReceived / FullSize;
	if(Progress <= 0.99f || DownloadListItem->FileDownloader->FileExtension != TEXT("zip"))
	{
		if(UpdateRemainTime <= 0.f)
		{
			Txt_DownloadProgress->SetText(FText::FromString(FString::Printf(TEXT("%d MB/%d MB (%d%%)"), (int32)(BytesReceived / BytesOfMB), (int32)(FullSize / BytesOfMB), (int32)(Progress * 100))));
			Txt_DownloadRate->SetText(FText::FromString(FString::Printf(TEXT("%.2f MB/s"), LastBytesReceived > 0.f ? (float)(BytesReceived - LastBytesReceived) / BytesOfMB : 0.f)));

			LastBytesReceived = BytesReceived;
			UpdateRemainTime = 1.f;
		}
		UpdateRemainTime -= 0.01f;
	}
	else
	{
		Txt_DownloadProgress->SetText(FText::FromString(TEXT("Unzipping")));
		Txt_DownloadRate->SetText(FText::GetEmpty());
	}
}

void SDownloadListItem::HandleDownloadComplete(EDownloadToStorageResult Result, const FString& SavePath)
{
	DownloadListItem->FileDownloader->OnProgress.RemoveAll(this);
	DownloadListItem->FileDownloader->OnComplete.RemoveAll(this);
	
	if(DownloadListItem->FileDownloader->FileExtension == TEXT("zip"))
	{
		FString FilePath, FileName, FileExtension;
		FPaths::Split(SavePath, FilePath, FileName, FileExtension);
		FZipManager::Unpack(SavePath, FilePath);
	}

	DownloadPanel->Refresh();
}

FReply SDownloadListItem::OnClearButtonClicked()
{
	if(!bRecentItem)
	{
		DownloadListItem->FileDownloader->CancelDownload();
	}
	else
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.DeleteFile(*DownloadListItem->FileDownloader->FileSavePath);
	}
	DownloadListItem->FileDownloader->DestroyDownload();

	DownloadPanel->Refresh();

	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

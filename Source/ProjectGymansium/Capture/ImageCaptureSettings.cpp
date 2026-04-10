// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageCaptureSettings.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogImageCaptureSettings, Log, All);

bool FImageCaptureSettings::LoadFromJsonFile(const FString& FilePath, FImageCaptureSettings& OutSettings)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogImageCaptureSettings, Warning, TEXT("Could not read settings file: %s — using defaults"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogImageCaptureSettings, Warning, TEXT("Failed to parse JSON from: %s — using defaults"), *FilePath);
		return false;
	}

	if (JsonObject->HasField(TEXT("bEnabled")))
	{
		OutSettings.bEnabled = JsonObject->GetBoolField(TEXT("bEnabled"));
	}

	if (JsonObject->HasField(TEXT("CaptureIntervalSeconds")))
	{
		OutSettings.CaptureIntervalSeconds = FMath::Max(JsonObject->GetNumberField(TEXT("CaptureIntervalSeconds")), 0.01);
	}

	if (JsonObject->HasField(TEXT("ResolutionWidth")))
	{
		OutSettings.ResolutionWidth = FMath::Max(static_cast<int32>(JsonObject->GetNumberField(TEXT("ResolutionWidth"))), 1);
	}

	if (JsonObject->HasField(TEXT("ResolutionHeight")))
	{
		OutSettings.ResolutionHeight = FMath::Max(static_cast<int32>(JsonObject->GetNumberField(TEXT("ResolutionHeight"))), 1);
	}

	if (JsonObject->HasField(TEXT("CaptureMode")))
	{
		OutSettings.CaptureMode = JsonObject->GetStringField(TEXT("CaptureMode"));
	}

	if (JsonObject->HasField(TEXT("OutputSubfolder")))
	{
		OutSettings.OutputSubfolder = JsonObject->GetStringField(TEXT("OutputSubfolder"));
	}

	UE_LOG(LogImageCaptureSettings, Log, TEXT("Loaded capture settings from %s — Enabled=%s Mode=%s Res=%dx%d Interval=%.2fs"),
		*FilePath,
		OutSettings.bEnabled ? TEXT("true") : TEXT("false"),
		*OutSettings.CaptureMode,
		OutSettings.ResolutionWidth,
		OutSettings.ResolutionHeight,
		OutSettings.CaptureIntervalSeconds);

	return true;
}

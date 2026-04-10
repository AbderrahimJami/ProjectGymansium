// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ProjectGymansium : ModuleRules
{
	public ProjectGymansium(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "StructUtils", "Schola", "ScholaTraining", "ScholaProtobuf" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "RenderCore" });

		var incPath = Path.Combine(ModuleDirectory, "Capture");
		PrivateIncludePaths.Add(incPath);
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

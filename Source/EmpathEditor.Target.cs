// Copyright 2018 Team Empath All Rights Reserved

using UnrealBuildTool;
using System.Collections.Generic;

public class EmpathEditorTarget : TargetRules
{
	public EmpathEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		ExtraModuleNames.AddRange( new string[] { "Empath", "EmpathTools" } );
	}
}

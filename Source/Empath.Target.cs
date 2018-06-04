// Copyright 2018 Team Empath All Rights Reserved

using UnrealBuildTool;
using System.Collections.Generic;

public class EmpathTarget : TargetRules
{
	public EmpathTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

		ExtraModuleNames.AddRange( new string[] { "Empath" } );
    }

}

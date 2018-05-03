// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "LintRules"

// Asset Naming
const FText Rule_1 = LOCTEXT("1", "Asset name is not formed correctly. {Correction} See http://ue4.style/#1");

// Folder structure
const FText Rule_2_1_1 = LOCTEXT("2.1.1", "Asset folder path is not named correctly. Always use PascalCase. Fix '{Correction}'. See http://ue4.style/#2.1.1");
const FText Rule_2_1_2 = LOCTEXT("2.1.2", "Asset folder path is not named correctly. Never use spaces in asset paths. Fix '{Correction}'. See http://ue4.style/#2.1.2");
const FText Rule_2_1_3 = LOCTEXT("2.1.3", "Asset folder path is not named correctly. Never use Unicode or other symbols. Fix '{Correction}'. See http://ue4.style/#2.1.3");

const FText Rule_2_2 = LOCTEXT("2.2", "Asset is not in a top level folder. See http://ue4.style/#2.2");

const FText Rule_2_4 = LOCTEXT("2.4", "Map is not in a folder called 'Maps'. See http://ue4.style/#2.4");

const FText Rule_2_6_1 = LOCTEXT("2.6.1", "Asset is in a folder named 'Assets'. This is redundant. All assets are assets. See http://ue4.style/#2.6.1");
const FText Rule_2_6_2 = LOCTEXT("2.6.2", "Asset is in a folder named '{Correction}'. This is redundant if adhering to style guide. See http://ue4.style/#2.6.2");

// Blueprints
const FText Rule_3_1_Errors = LOCTEXT("3.1.1", "Blueprint is failing to compile due to errors. See http://ue4.style/#3.1");
const FText Rule_3_1_Warnings = LOCTEXT("3.1.2", "Blueprint is compiling but has warnings. See http://ue4.style/#3.1");

const FText Rule_3_2_1_2 = LOCTEXT("3.2.1.2", "{Type} variable '{Name}' is not named correctly. All variable names should be in the form of PascalCase. See http://ue4.style/#3.2.1.2");
const FText Rule_3_2_1_3 = LOCTEXT("3.2.1.3", "{Type} variable '{Name}' is not named correctly. All booleans should be named in PascalCase but prefixed with a lowercase 'b'. See http://ue4.style/#3.2.1.3");
const FText Rule_3_2_1_4_1 = LOCTEXT("3.2.1.4.1", "{Type} variable '{Name}' is not named correctly. Do not include 'Is'. Only functions should ask questions. See http://ue4.style/#3.2.1.4.1");
const FText Rule_3_2_1_6 = LOCTEXT("3.2.1.6", "{Type} variable '{Name}' is not named correctly. Do not include atomic type names in atomics. See http://ue4.style/#3.2.1.6");
const FText Rule_3_2_1_8 = LOCTEXT("3.2.1.8", "{Type} variable '{Name}' is not named correctly. Arrays should be named a plural noun and not end in 'Array'. See http://ue4.style/#3.2.1.8");

const FText Rule_3_2_2 = LOCTEXT("3.2.2", "{Type} variable '{Name}' is marked as Expose On Spawn but is not Editable. Exposed variables must be editable. See http://ue4.style/#3.2.2");
const FText Rule_3_2_2_1 = LOCTEXT("3.2.2", "{Type} variable '{Name}' is marked as Editable but does not have a tool tip defined. Editable variables must have a tooltip defined. See http://ue4.style/#3.2.2.1");

const FText Rule_3_2_3_NoCategory = LOCTEXT("3.2.3.1", "{Type} variable '{Name}' is marked as Editable but is not categorized. Editable variables belong in a 'Config' category. See http://ue4.style/#3.2.3");
const FText Rule_3_2_3_BadCategory = LOCTEXT("3.2.3.2", "{Type} variable '{Name}' is marked as Editable but has category {Category}. Editable variables belong in a 'Config' category. See http://ue4.style/#3.2.3");

const FText Rule_3_2_6 = LOCTEXT("3.2.7", "{Type} variable '{Name}' is marked as Transient but has a non-zero default value. See http://ue4.style/#3.2.6");
const FText Rule_3_2_7 = LOCTEXT("3.2.8", "{Type} variable '{Name}' is marked as Config. Do not use Config variables in Blueprint. See http://ue4.style/#3.2.7");

const FText Rule_3_3_2 = LOCTEXT("3.3.2", "Function '{Name}' does not have any return nodes. All functions should have a return node. See http://ue4.style/#3.3.2");
const FText Rule_3_3_3 = LOCTEXT("3.3.3", "Function '{Name}' exceeds node limit of {Limit}. Currently has {Count} nodes. See http://ue4.style/#3.3.3");
const FText Rule_3_3_4 = LOCTEXT("3.3.4", "Function '{Name}' is Public but does not have a description. See http://ue4.style/#3.3.4");

// Static Meshes
const FText Rule_4_1_1 = LOCTEXT("4.1.1", "Mesh has missing UVs. See http://ue4.style/#4.1.1");
const FText Rule_4_1_2 = LOCTEXT("4.1.2", "Mesh has overlapping or out of bound lightmap UVs. If you can't track this down, see the UE4 log file Linter produced for more details. See http://ue4.style/#4.1.2");

// Particle Systems
const FText Rule_5_1 = LOCTEXT("5.1", "ParticleSystem '{Name}' has an emitter named with default name 'Particle Emitter'. Please rename. See http://ue4.style/#5.1");

// Levels / Worlds
const FText Rule_6_1 = LOCTEXT("6.1", "Level should not have any map warnings or errors. Please open this level and address them. You can force a map check by running the console command 'map check' in the editor. See http://ue4.style/#6.1");

#undef LOCTEXT_NAMESPACE
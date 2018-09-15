//-------------------------------------------------------------------------------------------------
// This file deals with reading the configuration ini for ScriptCrunch
//-------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include <utility>
#include <vector>
#include <string>
#include "utility.h"
#include "SimpleIni.h"
#include "Configuration.h"

// The section and key IDs are stored in Configuration.h
#define GetIniSetting(section, key) ini.GetValue(section, key, 0, 0); \
	if(val == NULL) \
	{ \
		ErrorString = "Could not find " + tcstostring(key) + " key in section [" + tcstostring(section) + "]"; \
		return CF_ERRINISTRUCTURE; \
	}

void ClearSettings(SCSettings* Settings)
{
	if(Settings == NULL)
		return;
	// DTE
	Settings->DTEEnable = false;
	Settings->DTEBegin = 0;
	Settings->DTEEnd = 0;
	// Dictionary
	Settings->DictEnable = false;
	Settings->DictBegin = 0;
	Settings->DictEnd = 0;
	Settings->DictEntrySize = 0;
	Settings->DictMinString = 0;
	Settings->DictMaxString = 0;
	Settings->DictMaxString = 0;
	Settings->DictUseWholeWords = false;
	// Script
	Settings->ScriptFiles.clear();
	Settings->LineComments.clear();
	Settings->BlockComments.clear();
	Settings->LineStartComments.clear();
	Settings->IgnorePhrases.clear();
	Settings->IgnoreBlocks.clear();
	// Table
	Settings->InputTable = "";
	Settings->OutputTable = "";
	Settings->OutputFrequencyTable = "";
	Settings->PerformSizeAnalysis = false;
	// Output
	Settings->InsertFile = "";
	Settings->EndTag = "";
	Settings->FixedStringLen = 0;
	Settings->PadString = "";
}

ConfigurationFile::ConfigurationFile()
{
}

ConfigurationFile* ConfigurationFile::Instance()
{
	static ConfigurationFile inst;
	return &inst;
}

const string& ConfigurationFile::GetParseErrorString()
{
	return ErrorString;
}

int ConfigurationFile::ReadConfig(TCHAR* Filename)
{
	SI_Error rc; // INI class's error return code
	CSimpleIni ini(true, true, false); // Open INI file with UTF-8 encoding, no multikey, no multiline

	rc = ini.LoadFile(Filename);

	if(rc == SI_FILE) // Error
	{
		ErrorString = "Could not open file " + tcstostring(Filename);
		return CF_ERROPENPARSE;
	}

	// Clear the SCSettings struct
	ClearSettings(&Settings);

	const TCHAR* val = 0;

	//---------------------------------------------------
	// Read DTE section
	//---------------------------------------------------
	
	// DTEEnable [Required]
	val = GetIniSetting(INI_DTE, INI_DTEEnable);
	if(_tcscmp(val, INI_true) == 0)
		Settings.DTEEnable = true;
	else if(_tcscmp(val, INI_false) == 0)
		Settings.DTEEnable = false;
	else
	{
		ErrorString = "DTEEnable does not have a valid setting";
		return CF_ERRINISTRUCTURE;
	}

	// DTEBegin [Required]
	val = GetIniSetting(INI_DTE, INI_DTEBegin);
	Settings.DTEBegin = _tcstoul(val, NULL, 16);

	// DTEEnd [Required]
	val = GetIniSetting(INI_DTE, INI_DTEEnd);
	Settings.DTEEnd = _tcstoul(val, NULL, 16);

	//---------------------------------------------------
	// Read Dictionary section
	//---------------------------------------------------

	// DictEnable [Required]
	val = GetIniSetting(INI_Dict, INI_DictEnable);
	if(_tcscmp(val, INI_true) == 0)
		Settings.DictEnable = true;
	else if(_tcscmp(val, INI_false) == 0)
		Settings.DictEnable = false;
	else
		return CF_ERRINISTRUCTURE;

	// DictBegin [Required]
	val = GetIniSetting(INI_Dict, INI_DictBegin);
	Settings.DictBegin = _tcstoul(val, NULL, 16);

	// DictEnd [Required]
	val = GetIniSetting(INI_Dict, INI_DictEnd);
	Settings.DictEnd = _tcstoul(val, NULL, 16);

	/*// DictEntrySize
	val = GetIniSetting(INI_Dict, INI_DictEntrySize);
	Settings.DictEntrySize = _tcstoul(val, NULL, 10);*/

	// DictUseWholeWords [Required]
	val = GetIniSetting(INI_Dict, INI_DictUseWholeWords);
	if(_tcscmp(val, INI_true) == 0)
		Settings.DictUseWholeWords = true;
	else if(_tcscmp(val, INI_false) == 0)
		Settings.DictUseWholeWords = false;
	else
	{
		ErrorString = "DictUseWholeWords does not have a valid setting";
		return CF_ERRINISTRUCTURE;
	}

	// DictEntrySize [Required]
	val = GetIniSetting(INI_Dict, INI_DictEntrySize);
	Settings.DictEntrySize = _tcstoul(val, NULL, 0);
	if(Settings.DictEntrySize != 1 && Settings.DictEntrySize != 2)
	{
		ErrorString = "DictEntrySize contains an invalid value (only 1 or 2 allowed)";
		return CF_ERRINISTRUCTURE;
	}

	// DictMinString [Required]
	val = GetIniSetting(INI_Dict, INI_DictMinString);
	Settings.DictMinString = _tcstoul(val, NULL, 0);

	// DictMaxString [Required]
	val = GetIniSetting(INI_Dict, INI_DictMaxString);
	Settings.DictMaxString = _tcstoul(val, NULL, 0);

	//-------------------------------------------------------------------------
	// Read Script section
	// The functions in here parse strings in entries such as:
	// BlockComments=/*,*/,<,>
	// Into the proper structure with strings of "/*", "*/", "<", ">"
	//-------------------------------------------------------------------------

	string s; // For parsing

	// ScriptFile [Required]
	CSimpleIni::TNamesDepend values;
	if(ini.GetAllValues(INI_Script, INI_ScriptFile, values))
	{
		CSimpleIni::TNamesDepend::const_iterator i = values.begin();
		for(; i != values.end(); i++)
			Settings.ScriptFiles.push_back(tcstostring(*i));
	}
	else
	{
		ErrorString = "ScriptFile does not have a valid setting";
		return CF_ERRINISTRUCTURE;
	}
	values.clear();

	// IgnorePhrase [Optional]
	if(ini.GetAllValues(INI_Script, INI_IgnorePhrase, values))
	{
		CSimpleIni::TNamesDepend::const_iterator i = values.begin();
		for(; i != values.end(); i++)
		{
			if(_tcslen(*i) != 0)
				Settings.IgnorePhrases.push_back(tcstostring(*i));
		}
	}
	values.clear();

	// LineComments [Required]
	val = GetIniSetting(INI_Script, INI_LineComments);
	s = tcstostring(val);
	while(s.length() != 0)
	{
		size_t pos1 = s.find_first_not_of(" ,");
		s.erase(0, pos1);
		size_t pos2 = s.find_first_of(" ,");
		if(pos2 == string::npos) // Not found, so the entry occupies the rest of the string
			pos2 = s.length();

		Settings.LineComments.push_back(s.substr(0, pos2));
		s.erase(0, pos2);
	}

	// BlockComments [Required]
	val = GetIniSetting(INI_Script, INI_BlockComments);
	s = tcstostring(val);
	while(s.length() != 0)
	{
		StringPair sp;
		size_t pos1 = s.find_first_not_of(" ,");
		s.erase(0, pos1);
		size_t pos2 = s.find_first_of(" ,");
		if(pos2 == string::npos) // Not found, so the entry occupies the rest of the string
			pos2 = s.length();

		sp.first = s.substr(0, pos2);
		s.erase(0, pos2);

		pos1 = s.find_first_not_of(" ,");
		s.erase(0, pos1);
		pos2 = s.find_first_of(" ,");
		if(pos2 == string::npos) // Not found, so the entry occupies the rest of the string
			pos2 = s.length();

		if(pos2 == 0) // No second entry
		{
			ErrorString = "BlockComments does not have an even number of strings";
			return CF_ERRINISTRUCTURE;
		}

		sp.second = s.substr(0, pos2);
		s.erase(0, pos2);

		Settings.BlockComments.push_back(sp);
	}

	// IgnoreBlocks [Required]
	val = GetIniSetting(INI_Script, INI_IgnoreBlocks);
	s = tcstostring(val);
	while(s.length() != 0)
	{
		StringPair sp;
		size_t pos1 = s.find_first_not_of(" ,");
		s.erase(0, pos1);
		size_t pos2 = s.find_first_of(" ,");
		if(pos2 == string::npos) // Not found, so the entry occupies the rest of the string
			pos2 = s.length();

		sp.first = s.substr(0, pos2);
		s.erase(0, pos2);

		pos1 = s.find_first_not_of(" ,");
		s.erase(0, pos1);
		pos2 = s.find_first_of(" ,");
		if(pos2 == string::npos) // Not found, so the entry occupies the rest of the string
			pos2 = s.length();

		if(pos2 == 0) // No second entry
		{
			ErrorString = "IgnoreBlocks does not have an even number of strings";
			return CF_ERRINISTRUCTURE;
		}

		sp.second = s.substr(0, pos2);
		s.erase(0, pos2);

		Settings.IgnoreBlocks.push_back(sp);
	}

	// LineStartComments [Required]
	val = GetIniSetting(INI_Script, INI_LineStartComments);
	s = tcstostring(val);
	while(s.length() != 0)
	{
		size_t pos1 = s.find_first_not_of(" ,");
		s.erase(0, pos1);
		size_t pos2 = s.find_first_of(" ,");
		if(pos2 == string::npos) // Not found, so the entry occupies the rest of the string
			pos2 = s.length();

		Settings.LineStartComments.push_back(s.substr(0, pos2));
		s.erase(0, pos2);
	}

	//-------------------------------------------------------------------------
	// Read Output section
	//-------------------------------------------------------------------------

	// OutputTable [Required]
	val = GetIniSetting(INI_Table, INI_OutputTable);
	Settings.OutputTable = tcstostring(val);

	// InputTable [Optional]
	val = ini.GetValue(INI_Table, INI_InputTable, 0, 0);
	if(val != NULL)
		Settings.InputTable = tcstostring(val);

	// OutputFrequencyTable [Optional]
	val = ini.GetValue(INI_Table, INI_OutputFrequencyTable, 0, 0);
	if(val != NULL)
		Settings.OutputFrequencyTable = tcstostring(val);

	// PerformSizeAnalysis [Required]
	val = GetIniSetting(INI_Table, INI_PerformSizeAnalysis);
	if(_tcscmp(val, INI_true) == 0)
		Settings.PerformSizeAnalysis = true;
	else if(_tcscmp(val, INI_false) == 0)
		Settings.PerformSizeAnalysis = false;
	else
	{
		ErrorString = "PerformSizeAnalysis does not have a valid setting";
		return CF_ERRINISTRUCTURE;
	}

	//-------------------------------------------------------------------------
	// Read Insert section
	//-------------------------------------------------------------------------

	// InsertFile [Required]
	val = GetIniSetting(INI_Insert, INI_InsertFile);
	Settings.InsertFile = tcstostring(val);

	// EndTag [Required]
	val = GetIniSetting(INI_Insert, INI_EndTag);
	Settings.EndTag = tcstostring(val);

	// FixedStringLen [Required]
	val = GetIniSetting(INI_Insert, INI_FixedStringLen);
	Settings.FixedStringLen = _tcstoul(val, NULL, 0);

	// PadString [Required]
	val = GetIniSetting(INI_Insert, INI_PadString);
	Settings.PadString = tcstostring(val);

	return 0;
}
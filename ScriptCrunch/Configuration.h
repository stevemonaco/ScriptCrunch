#pragma once

#include "stdafx.h"
#include <utility>
#include <vector>
#include <string>

using namespace std;

typedef pair<string,string> StringPair;
typedef vector<string>::iterator VecStringIt;
typedef vector<StringPair>::iterator VecStringPairIt;

// ConfigurationFile errors
static const int CF_SUCCESS   = 0;
static const int CF_ERROPENPARSE = -1;
static const int CF_ERRINISTRUCTURE = -2;
static const int CF_ERRUNKNOWN   = -10;

// INI File Sections
static const TCHAR* INI_DTE = _T("DTE");
static const TCHAR* INI_Dict = _T("Dictionary");
static const TCHAR* INI_Script = _T("Script");
static const TCHAR* INI_Table = _T("Table");
static const TCHAR* INI_Insert = _T("Insert");

// INI Keys
static const TCHAR* INI_DTEEnable = _T("DTEEnable");
static const TCHAR* INI_DTEBegin = _T("DTEBegin");
static const TCHAR* INI_DTEEnd = _T("DTEEnd");
static const TCHAR* INI_DictEnable = _T("DictEnable");
static const TCHAR* INI_DictBegin = _T("DictBegin");
static const TCHAR* INI_DictEnd = _T("DictEnd");
static const TCHAR* INI_DictEntrySize = _T("DictEntrySize");
static const TCHAR* INI_DictUseWholeWords = _T("DictUseWholeWords");
static const TCHAR* INI_DictMinString = _T("DictMinString");
static const TCHAR* INI_DictMaxString = _T("DictMaxString");
static const TCHAR* INI_ScriptFile = _T("ScriptFile");
static const TCHAR* INI_IgnorePhrase = _T("IgnorePhrase");
static const TCHAR* INI_IgnoreBlocks = _T("IgnoreBlocks");
static const TCHAR* INI_LineComments = _T("LineComments");
static const TCHAR* INI_BlockComments = _T("BlockComments");
static const TCHAR* INI_LineStartComments = _T("LineStartComments");
static const TCHAR* INI_LineAppendOnWrap = _T("LineAppendOnWrap");
static const TCHAR* INI_OutputTable = _T("OutputTable");
static const TCHAR* INI_InputTable = _T("InputTable");
static const TCHAR* INI_OutputFrequencyTable = _T("OutputFrequencyTable");
static const TCHAR* INI_PerformSizeAnalysis = _T("PerformSizeAnalysis");
static const TCHAR* INI_InsertFile = _T("InsertFile");
static const TCHAR* INI_EndTag = _T("EndTag");
static const TCHAR* INI_FixedStringLen = _T("FixedStringLen");
static const TCHAR* INI_PadString = _T("PadString");

// INI File Strings
static const TCHAR* INI_true = _T("true");
static const TCHAR* INI_false = _T("false");

// Contains ScriptCrunch settings from the ini file
struct SCSettings
{
	// DTE
	bool DTEEnable;
	uint DTEBegin;
	uint DTEEnd;
	// Dictionary
	bool DictEnable;
	uint DictBegin;
	uint DictEnd;
	uint DictEntrySize;
	uint DictMinString;
	uint DictMaxString;
	bool DictUseWholeWords;
	// Script
	vector<string> ScriptFiles;
	vector<string> IgnorePhrases;        // Ignored by text analysis, included in mock script insert
	vector<string> LineComments;
	vector<StringPair> BlockComments;  // First string of the pair is the start comment, second is the end
	vector<StringPair> IgnoreBlocks;   // Same as above
	vector<string> LineStartComments;
	// Table
	string InputTable;
	string OutputTable;
	string OutputFrequencyTable;
	bool PerformSizeAnalysis;
	// Output
	string InsertFile;
	string EndTag;
	int FixedStringLen;
	string PadString;
};

// Clear all of ScriptCrunch's ini settings in the given struct
void ClearSettings(SCSettings* Settings);

//-----------------------------------------------------------------------------------------------------------
// ConfigurationFile class
// Contains the functionality needed to read the ScriptCrunch INI file in a singleton class
//-----------------------------------------------------------------------------------------------------------

class ConfigurationFile
{
public:
	static ConfigurationFile* Instance();
	int ReadConfig(TCHAR* Filename);
	const string& GetParseErrorString();
private:
	ConfigurationFile();
	string ErrorString;
protected:
	ConfigurationFile(const ConfigurationFile&);
	ConfigurationFile& operator=(const ConfigurationFile&);

public:
	SCSettings Settings;
};
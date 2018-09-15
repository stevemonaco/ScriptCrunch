#include "stdafx.h"
#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <map>
#include <algorithm>
#include <ctime>
#include "Configuration.h"
#include "ScriptCrunch.h"
#include "utility.h"
#include "Table.h"

using namespace std;

const char* AlphaNum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123456789";

// For frequency analysis
vector<string> StringTable; // Holds all of the contiguous text strings to be sorted for occurances
string ScriptBuf; // Holds the script for script insert analysis
vector<FrequencyPair> MasterDTEFreqTable; // Holds the results for the DTE table
vector<FrequencyPair> MasterDictFreqTable; // Holds the results for the Dictionary table
bool bAppendTableSuccess = false; // To make sure the new table is correctly outputted for PerformSizeAnalysis
int DictEntrySize = 0;
int MinStringLen = 0;
ConfigurationFile* cf = NULL;

// For statistics
int PrevTotalChars = 0;
float PrevAvgCharsPerLine = 0.0;
int PrevTotalLines = 0;

int _tmain(int argc, _TCHAR* argv[])
{
	// Time profiling
	clock_t GenerateStartTime, GenerateEndTime, ProgramStartTime, ProgramEndTime;

	ProgramStartTime = clock();

	printf("ScriptCrunch v1.0 by Klarth\n");
	if(argc != 2)
	{
		_tprintf(_T("Usage: %s <Config.txt>\n"), argv[0]);
		return 1;
	}
	
	cf = ConfigurationFile::Instance();
	_tprintf(_T("\nParsing Configuration %s: "), argv[1]);
	int rc = cf->ReadConfig(argv[1]);
	if(rc != CF_SUCCESS)
	{
		printf("Failed!\n%s\n", cf->GetParseErrorString().c_str());
		return 2;
	}

	printf("[Success]\n");

	DictEntrySize = cf->Settings.DictEntrySize;
	if(cf->Settings.DTEEnable)
		MinStringLen = 2;
	else if(cf->Settings.DictEnable)
		MinStringLen = cf->Settings.DictMinString;

	//-------------------------------------------------------------------------
	// Gather script and parse out comments
	//-------------------------------------------------------------------------

	if(cf->Settings.ScriptFiles.empty()) // No script...
	{
		printf("There were no script files listed in %s\n", argv[1]);
		return 3;
	}

	for(VecStringIt i = cf->Settings.ScriptFiles.begin(); i != cf->Settings.ScriptFiles.end(); i++)
	{
		printf("Parsing Script %s ", i->c_str());
		if(!ParseScript(i->c_str()))
		{
			printf("Fatal error attempting to open %s\n", i->c_str());
			return 4;
		}

		printf("[Success]\n");
	}

	// Test what the parser grabs for the script insert
	/*FILE* fscript = fopen("scriptoutput.txt", "wt");
	fwrite(ScriptBuf.data(), 1, ScriptBuf.size(), fscript);
	fclose(fscript);*/

	// Remove ignore settings from the analyzer
	printf("Removing ignore strings from the analyzer");
	RemoveIgnoreStrings();

	// Remove empty lines
	StringTableIt stit = StringTable.begin();
	while(stit != StringTable.end())
	{
		if((int)stit->length() < MinStringLen)
		{
			StringTableIt i = stit;
			stit++;
			StringTable.erase(i);
		}
		else
			stit++;
	}
	printf(" [Done]");

	// Test what the parser grabs for the analysis
	/*FILE* f = fopen("analyzeoutput.txt", "wt");
	for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
		fprintf(f, "%s\n", i->c_str());
	fclose(f);*/

	PrecalculateStatistics();

	// Time to generate the frequency table
	GenerateStartTime = clock();
	printf("\nGenerating the Frequency Tables\n");
	GenerateFrequencyTable();
	GenerateEndTime = clock();
	printf("[Finished] %d seconds\n\n", (GenerateEndTime - GenerateStartTime) / 1000);

	// Output Table File
	if(!OutputTableFile(cf->Settings.OutputTable.c_str()))
	{
		printf("Failed to open %s to output the Table File\n", cf->Settings.OutputTable.c_str());
		return 5;
	}

	// Output the file for insertion
	if(!cf->Settings.InsertFile.empty())
	{
		printf("Opening %s for Insert File output", cf->Settings.InsertFile.c_str());
		if(!OutputInsertFile(cf->Settings.InsertFile.c_str()))
		{
			printf(": Failed to open %s to output the Insert File\n", cf->Settings.InsertFile.c_str());
			return 5;
		}
		else
			printf(" [Success]\n");
	}

	// Frequency table
	if(!cf->Settings.OutputFrequencyTable.empty())
	{
		printf("Opening %s for Frequency Table output", cf->Settings.OutputFrequencyTable.c_str());
		if(!OutputFrequencyTable(cf->Settings.OutputFrequencyTable.c_str()))
		{
			printf(": Failed to open %s to output the Frequency Table\n", cf->Settings.OutputFrequencyTable.c_str());
			return 6;
		}
		else
			printf(" [Success]\n");
	}

	printf("\n");

	// Statistics	
	if(bAppendTableSuccess && cf->Settings.PerformSizeAnalysis)
	{
		PerformSizeAnalysis();
	}
	else
	{
		printf("PerformSizeAnalysis was not selected or table append failed\n");
		printf("Mock script insertion stats can't be obtained.\n");
		OutputStatistics();
	}

	printf("\n");

	ProgramEndTime = clock();
	printf("Program runtime: %d seconds\n", (ProgramEndTime - ProgramStartTime) / 1000);

	return 0;
}

bool ParseScript(const char* Filename)
{
	ifstream f(Filename);
	if(!f.is_open())
		return false;

	string line = "";
	bool bInsideBlockComment = false;
	bool bSkipToNextIteration = false;
	bool bAppendNextString = false;  // If a string ends at the end of a line and continues on the next without
	                                 // breaks, we append it for a longer run
	string EndBlock;
	size_t BlockPos, LinePos; // Positions of a block and linecomment in a string, the first to
							  // appear in the string takes precedence

	StringTable.push_back(line); // Put a new blank line per file, then add to it

	while(!f.eof())
	{
		bSkipToNextIteration = false;
		getline(f, line);

		if(line.empty()) // Blank lines
			continue;
		
		if(bInsideBlockComment)
		{
			size_t pos = line.find(EndBlock, 0);
			if(pos == string::npos)
				continue; // No end block string, next line
			else
			{
				line.erase(0, pos+EndBlock.length());
				goto parsepartialline; // Parse the rest of the text for comments
			}
		}
		
		//-----------------------------------------------------------------------------------------
		// Search for comments
		//-----------------------------------------------------------------------------------------

		// Search LineStartComments first
		for(VecStringIt i = cf->Settings.LineStartComments.begin();
			i != cf->Settings.LineStartComments.end(); i++)
		{
			if(line.compare(0, i->length(), *i) == 0) // Compare one LineStartComment to the current line
			{
				bSkipToNextIteration = true;
				break; // Found a comment, don't add text, move to next iteration
			}
		}

parsepartialline: // LineComments and BlockComments

		if(bSkipToNextIteration)
			continue;

		// Search LineComments next
		VecStringIt firstlinematch = cf->Settings.LineComments.end();
		size_t firstpos = line.length();
		for(VecStringIt i = cf->Settings.LineComments.begin();
			i != cf->Settings.LineComments.end(); i++)
		{
			LinePos = line.find(*i, 0);
			if(LinePos != string::npos) // Found a comment
			{
				if(LinePos == 0) // At the start, can discard it all
				{
					bSkipToNextIteration = true;
					break; // Next iteration
				}
				else // Found a comment somewhere in the middle
				{
					if(LinePos < firstpos) // Found a comment earlier than the previous comment
					{
						firstpos = LinePos;
						firstlinematch = i;
					}
				}
			}
		}

		if(bSkipToNextIteration)
			continue;

		if(firstlinematch != cf->Settings.LineComments.end())
			LinePos = firstpos;
		else
			LinePos = string::npos;

		// Search BlockComments last
		// Variables to hold the earliest match on a line, in case of multiple comments
		VecStringPairIt firstblockmatch = cf->Settings.BlockComments.end();
		firstpos = line.length();
		for(VecStringPairIt i = cf->Settings.BlockComments.begin();
			i != cf->Settings.BlockComments.end(); i++)
		{
			BlockPos = line.find(i->first, 0);
			if(BlockPos != string::npos) // If we found a comment
			{
				if(BlockPos < firstpos) // Add the earliest block comment on the line
				{
					firstpos = BlockPos;
					firstblockmatch = i;
					EndBlock = i->second;
				}
			}
		}

		if(firstblockmatch == cf->Settings.BlockComments.end()) // no match
			BlockPos = string::npos;
		else
		{
			BlockPos = firstpos;
			EndBlock = firstblockmatch->second;
		}

		bool doLineComment = true;

		// Determine whether the BlockComment or LineComment came first
		if(BlockPos != string::npos && LinePos != string::npos)
		{
			if(LinePos < BlockPos)
			{
				doLineComment = true;
				EndBlock = ""; // Irrelevant now because the line comment came first
			}
			else
				doLineComment = false;
		}

		if(doLineComment && LinePos != string::npos)
		{
			StringTable.back() += line.substr(0, LinePos);
			continue; // Iterate the main loop with the next line
		}
		else if(!EndBlock.empty()) // BlockComment
		{
			if(BlockPos != 0) // Push previous text before the comment
				StringTable.back() += line.substr(0, BlockPos);

			line.erase(0, BlockPos);
			size_t pos2 = line.find(EndBlock);
			if(pos2 != string::npos) // Found an end match on the same line
			{
				line.erase(0, pos2+EndBlock.length());
				EndBlock = ""; // Won't need this now
				if(!line.empty())
					goto parsepartialline; // Parse the rest of the text for LineComments or BlockComments
				else // Empty line
					continue; // Next line
			}
			else // Gotta find an end match on another line
			{
				bInsideBlockComment = true;
				continue; // Next line
			}
		}

		StringTable.back() += line;
	} // End line by line file reading loop

	if(cf->Settings.PerformSizeAnalysis)
	{
		for(StringTableIt it = StringTable.begin(); it != StringTable.end(); it++)
			ScriptBuf += *it;
	}

	return true;
}

void PrecalculateStatistics()
{
	PrevTotalChars = 0;

	PrevTotalLines = (int)StringTable.size();
	for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
		PrevTotalChars += (int)i->length();
	PrevAvgCharsPerLine = (float)PrevTotalChars / PrevTotalLines;
}

void GenerateFrequencyTable()
{
	// Allocate vector space
	if(cf->Settings.DTEEnable)
		MasterDTEFreqTable.reserve(cf->Settings.DTEEnd - cf->Settings.DTEBegin + 1);
	if(cf->Settings.DictEnable)
		MasterDictFreqTable.reserve(cf->Settings.DictEnd - cf->Settings.DictBegin + 1);

	// Dictionary only
	if(cf->Settings.DictEnable && cf->Settings.DictUseWholeWords && !cf->Settings.DTEEnable)
	{
		GenerateDictFrequencyTable();
	}
	// Substring only
	else if(cf->Settings.DictEnable && !cf->Settings.DictUseWholeWords && !cf->Settings.DTEEnable)
	{
		GenerateSubstringFrequencyTable();
	}
	// DTE Only
	else if(cf->Settings.DTEEnable && !cf->Settings.DictEnable)
	{
		GenerateDTEFrequencyTable();
	}
	// Dictionary + DTE
	else if(cf->Settings.DTEEnable && cf->Settings.DictEnable && cf->Settings.DictUseWholeWords)
	{
		GenerateDictFrequencyTable();
		// Remove the dictionary strings from the StringTable
		printf("Removing the Dictionary Strings From the Script Input ");
		for(VecFreqPairIt i = MasterDictFreqTable.begin(); i != MasterDictFreqTable.end(); i++)
			RemoveSubstringFromTable(i->first);
		printf("[Done]\n");
		GenerateDTEFrequencyTable();
	}
	// DTE + Substrings
	else if(cf->Settings.DictEnable && cf->Settings.DTEEnable && !cf->Settings.DictUseWholeWords)
	{
		//GenerateDTEFrequencyTable();
		GenerateSubstringFrequencyTable();
		printf("[Done]\n");
		GenerateDTEFrequencyTable();
		//GenerateDTESubstringFrequencyTable();
	}
}

void GenerateDTEFrequencyTable()
{
	FrequencyMap DTEFreq;
	int TotalPasses = cf->Settings.DTEEnd - cf->Settings.DTEBegin + 1;
	printf("Beginning DTE Analysis: %d passes required\nPass#: 1", TotalPasses);

	for(int a = 1; a <= TotalPasses; a++)
	{
		if(a % 5 == 0) // Print pass number
			printf(" %d", a);

		for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
		{
			if(i->length() < 2)
				continue;

			for(size_t j = 0; j < i->length() - 1; j++)
			{
				string s = i->substr(j,2);

				FrequencyMapIt it = DTEFreq.lower_bound(s);
				if(it != DTEFreq.end() && !(DTEFreq.key_comp()(s, it->first)))
					it->second++;
				else
					DTEFreq.insert(it, FrequencyMap::value_type(s, 1));
			}
		}

		FrequencyMapIt MaxVal = max_element(DTEFreq.begin(), DTEFreq.end(), FreqPairOccurancePred);

		if(MaxVal != DTEFreq.end()) // Max element found...
		{
			MasterDTEFreqTable.push_back(*MaxVal);
			RemoveSubstringFromTable(MaxVal->first);
			DTEFreq.erase(MaxVal);
		}

		// Reset values
		for(FrequencyMapIt i = DTEFreq.begin(); i != DTEFreq.end(); i++)
			i->second = 0;
	}

	printf(" [Done]\n");
}

void GenerateDictFrequencyTable()
{
	FrequencyMap DictFreq;
	FrequencyMapIt DictIt;

	printf("Beginning Dictionary Analysis ");
	// Gather frequency of whole words first and fill the dictionary space
	for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
	{
		if(i->length() < cf->Settings.DictMinString)
			continue;

		size_t spos = 0; // Search position
		size_t begpos = 0;
		size_t endpos = 0;
		size_t len = 0;
		
		while(spos+1 != i->length())
		{
			begpos = i->find_first_of(AlphaNum, spos);
			if(begpos == string::npos) // No start of a word
				break; // Go to next string

			endpos = i->find_first_not_of(AlphaNum, begpos);

			if(endpos == string::npos) // Goes to end of the string
			{
				len = i->length() - begpos;
				if(len < cf->Settings.DictMinString || len > cf->Settings.DictMaxString)
					break; // Next string

				string s = i->substr(begpos, len);
				DictIt = DictFreq.find(s);
				if(DictIt == DictFreq.end())
				{
					DictFreq.insert(FrequencyMap::value_type(s, 1));
				}
				else
					DictIt->second++;

				break; // Next string
			}
			else // String with more data at the end
			{
				len = endpos - begpos;
				if(len < cf->Settings.DictMinString || len > cf->Settings.DictMaxString)
				{
					spos = endpos;
					continue; // Next iteration of the same string
				}

				string s = i->substr(begpos, len);
				DictIt = DictFreq.find(s);
				if(DictIt == DictFreq.end())
				{
					DictFreq.insert(FrequencyMap::value_type(s, 1));
				}
				else
					DictIt->second++;

				spos = endpos;
			}
		}
	}

	// Finished frequency analysis, sort results, and add to the table.
	vector<FrequencyPair> DictTemp;
	DictTemp.reserve(DictFreq.size());
	DictTemp.assign(DictFreq.begin(), DictFreq.end());
	sort(DictTemp.begin(), DictTemp.end(), SortFrequencyMapDictSizeDesc);

	size_t MaxEntries = cf->Settings.DictEnd - cf->Settings.DictBegin + 1;
	if(DictTemp.size() < MaxEntries)
		MaxEntries = DictTemp.size();

	for(size_t i = 0; i < MaxEntries; i++)
		MasterDictFreqTable.push_back(DictTemp[i]);
	DictTemp.clear();

	printf("[Done]\n");
}

void GenerateSubstringFrequencyTable()
{
	FrequencyHashMap DictFreq;

	int TotalPasses = cf->Settings.DictEnd - cf->Settings.DictBegin + 1;
	printf("Beginning Substring Analysis: %d passes required\nPass#: 1", TotalPasses);

	for(int a = 1; a <= TotalPasses; a++)
	{
		if(a % 5 == 0)
			printf(" %d", a);

		// One pass
		for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
		{
			if(i->length() < cf->Settings.DictMinString)
				continue;

			// Add each substring to the table
			int maxcompj = (int) i->length() - cf->Settings.DictMinString - 1;
			if(maxcompj < 0)
				maxcompj = 0;

			for(int j = 0; j <= maxcompj; j++) // For each position of the string
			{
				int maxcompk = cf->Settings.DictMaxString;
				if(maxcompk > (int)i->length() - j)
					maxcompk = (int)i->length() - j;

				for(int k = cf->Settings.DictMinString; k <= maxcompk; k++)
				{
					string s = i->substr(j,k);

					FrequencyMapIt it = DictFreq.lower_bound(s);
					if(it != DictFreq.end() && !(DictFreq.key_comp()(s, it->first)))
					{
						it->second++;
					}
					else
						it = DictFreq.insert(it, FrequencyMap::value_type(s, 1));
				}
			}
		}

		FrequencyHashMapIt MaxVal = max_element(DictFreq.begin(), DictFreq.end(), FreqPairOccurancePredDictSize);

		if(MaxVal != DictFreq.end())
		{
			MasterDictFreqTable.push_back(*MaxVal);
			RemoveSubstringFromTable(MaxVal->first);
			DictFreq.erase(MaxVal);
		}

		// Reset values
		for(FrequencyHashMapIt i = DictFreq.begin(); i != DictFreq.end();)
		{
			if(i->second == 0) // Entry that previously had a match, but no longer has one
			{
				FrequencyHashMapIt removeit = i;
				i++;
				DictFreq.erase(removeit);
			}
			else
			{
				i->second = 0;
				i++;
			}
		}
		/*if(a == 10 || a == 20 || (a % 32) == 0)
			DictFreq.clear();
		else
		{
			for(FrequencyHashMapIt i = DictFreq.begin(); i != DictFreq.end(); i++)
				i->second = 0;
		}*/
	}

	printf(" [Done]\n");
}

void GenerateDTESubstringFrequencyTable()
{
	FrequencyHashMap DictFreq;
	FrequencyMap DTEFreq;

	int DTEEntries = cf->Settings.DTEEnd - cf->Settings.DTEBegin + 1;
	int DictEntries = cf->Settings.DictEnd - cf->Settings.DictBegin + 1;

	int TotalPasses = DTEEntries + DictEntries;
	printf("Beginning Substring Analysis: %d passes required\nPass #: 1", TotalPasses);

	for(int a = 1; a <= TotalPasses; a++)
	{
		if(a % 5 == 0)
			printf(" %d", a);

		// One pass
		for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
		{
			if(i->length() < 2)
				continue;

			// Add each substring to the table
			if((int)MasterDictFreqTable.size() < DictEntries)
			{
				int maxcompj = (int) i->length() - cf->Settings.DictMinString - 1;
				if(maxcompj < 0)
					maxcompj = 0;

				for(int j = 0; j <= maxcompj; j++) // For each position of the string
				{
					int maxcompk = cf->Settings.DictMaxString;
					if(maxcompk > (int)i->length() - j)
						maxcompk = (int)i->length() - j;

					for(int k = cf->Settings.DictMinString; k <= maxcompk; k++)
					{
						string s = i->substr(j,k);

						FrequencyMapIt it = DictFreq.lower_bound(s);
						if(it != DictFreq.end() && !(DictFreq.key_comp()(s, it->first)))
							it->second++;
						else
							it = DictFreq.insert(it, FrequencyMap::value_type(s, 1));
					}
				}
			}

			// Add each DTE pair to the table
			if((int)MasterDTEFreqTable.size() < DTEEntries)
			{
				for(size_t j = 0; j < i->length() - 1; j++)
				{
					string s = i->substr(j,2);

					FrequencyMapIt it = DTEFreq.lower_bound(s);
					if(it != DTEFreq.end() && !(DTEFreq.key_comp()(s, it->first)))
						it->second++;
					else
						DTEFreq.insert(it, FrequencyMap::value_type(s, 1));
				}
			}
		}

		// Get max value
		FrequencyHashMapIt MaxDictVal = max_element(DictFreq.begin(), DictFreq.end(), FreqPairOccurancePredDictSize);
		int DictBytesSaved = (int)(MaxDictVal->first.length() - DictEntrySize) * MaxDictVal->second;
		FrequencyMapIt MaxDTEVal = max_element(DTEFreq.begin(), DTEFreq.end(), FreqPairOccurancePred);
		int DTEBytesSaved = MaxDTEVal->second;

		// If both tables aren't full
		if((int)MasterDictFreqTable.size() < DictEntries && (int)MasterDTEFreqTable.size() < DTEEntries)
		{
			if(DictBytesSaved > DTEBytesSaved)
			{
				MasterDictFreqTable.push_back(*MaxDictVal);
				RemoveSubstringFromTable(MaxDictVal->first);
				DictFreq.erase(MaxDictVal);
			}
			else
			{
				MasterDTEFreqTable.push_back(*MaxDTEVal);
				RemoveSubstringFromTable(MaxDTEVal->first);
				DTEFreq.erase(MaxDTEVal);
			}
			for(FrequencyHashMapIt i = DictFreq.begin(); i != DictFreq.end(); i++)
				i->second = 0;
			for(FrequencyMapIt i = DTEFreq.begin(); i != DTEFreq.end(); i++)
				i->second = 0;
			continue; // Next iteration to find the next best match
		}

		if((int)MasterDictFreqTable.size() >= DictEntries) // Dict table is full, put in DTE
		{
			MasterDTEFreqTable.push_back(*MaxDTEVal);
			RemoveSubstringFromTable(MaxDTEVal->first);
			DTEFreq.erase(MaxDTEVal);
			for(FrequencyMapIt i = DTEFreq.begin(); i != DTEFreq.end(); i++)
				i->second = 0;
		}

		if((int)MasterDTEFreqTable.size() >= DTEEntries) // DTE table is full, put in Dict
		{
			MasterDictFreqTable.push_back(*MaxDictVal);
			RemoveSubstringFromTable(MaxDictVal->first);
			DictFreq.erase(MaxDictVal);
			for(FrequencyHashMapIt i = DictFreq.begin(); i != DictFreq.end(); i++)
				i->second = 0;
		}

	}
}

bool OutputTableFile(const char* Filename)
{
	FILE* f = fopen(Filename, "wt");
	if(f == NULL)
		return false;

	if(!cf->Settings.InputTable.empty()) // Append
	{
		printf("Attempting to combine %s with the generated table", cf->Settings.InputTable.c_str());
		FILE* inputtable = fopen(cf->Settings.InputTable.c_str(), "rt");
		if(inputtable == NULL)
			printf(": Input table %s does not exist, skipping the size analysis\n");
		else
		{
			char line[500];
			while(fgets(line, 500, inputtable) != NULL)
				fprintf(f, "%s", line);

			fclose(inputtable);
			fprintf(f, "\n");
			bAppendTableSuccess = true;
			printf(" [Success]\n");
		}
	}

	// Print DTE Table
	if(cf->Settings.DTEEnable)
	{
		uint DTEPos = cf->Settings.DTEBegin;
		for(size_t i = 0; i < MasterDTEFreqTable.size() && DTEPos <= cf->Settings.DTEEnd; i++, DTEPos++)
			fprintf(f, "%02X=%s\n", DTEPos, MasterDTEFreqTable[i].first.c_str());
	}

	// Print Dictionary/Substring Table
	if(cf->Settings.DictEnable)
	{
		uint DictPos = cf->Settings.DictBegin;
		for(size_t i = 0; i < MasterDictFreqTable.size() && DictPos <= cf->Settings.DictEnd; i++, DictPos++)
		{
			if(DictEntrySize == 1)
				fprintf(f, "%02X=%s\n", DictPos, MasterDictFreqTable[i].first.c_str());
			else if(DictEntrySize == 2)
				fprintf(f, "%04X=%s\n", DictPos, MasterDictFreqTable[i].first.c_str());
		}
	}
	
	fclose(f);
	return true;
}

bool OutputInsertFile(const char* Filename)
{
	FILE* f = fopen(Filename, "wt");
	if(f == NULL)
		return false;

	// Print DTE Entries
	if(cf->Settings.DTEEnable)
	{
		uint DTEPos = cf->Settings.DTEBegin;
		for(size_t i = 0; i < MasterDTEFreqTable.size() && DTEPos <= cf->Settings.DTEEnd; i++, DTEPos++)
			fprintf(f, "%s\n", MasterDTEFreqTable[i].first.c_str());
	}

	// Print Dictionary/Substring Table
	if(cf->Settings.DictEnable)
	{
		uint DictPos = cf->Settings.DictBegin;
		for(size_t i = 0; i < MasterDictFreqTable.size() && DictPos <= cf->Settings.DictEnd; i++, DictPos++)
		{
			// Print Entry
			fprintf(f, "%s", MasterDictFreqTable[i].first.c_str());

			// Pad String
			if(cf->Settings.FixedStringLen != 0) // Pad string
			{
				int PadCount = cf->Settings.FixedStringLen - (int)MasterDictFreqTable[i].first.length();
				if(PadCount < 0)
					PadCount = 0;

				for(int j = 0; j < PadCount; j++)
					fprintf(f, "%s", cf->Settings.PadString.c_str());
			}

			// End tag (can be blank) and newline
			fprintf(f, "%s\n", cf->Settings.EndTag.c_str());
		}
	}
	
	fclose(f);
	return true;
}

bool OutputFrequencyTable(const char* Filename)
{
	FILE* f = fopen(Filename, "wt");
	if(f == NULL)
		return false;
	
	// Print DTE Table
	if(cf->Settings.DTEEnable)
	{
		uint DTEPos = cf->Settings.DTEBegin;
		for(size_t i = 0; i < MasterDTEFreqTable.size() && DTEPos <= cf->Settings.DTEEnd; i++, DTEPos++)
			fprintf(f, "%s=%d\n", MasterDTEFreqTable[i].first.c_str(), MasterDTEFreqTable[i].second);
	}

	// Print Dictionary/Substring Table
	if(cf->Settings.DictEnable)
	{
		uint DictPos = cf->Settings.DictBegin;
		for(size_t i = 0; i < MasterDictFreqTable.size() && DictPos <= cf->Settings.DictEnd; i++, DictPos++)
			fprintf(f, "%s=%d\n", MasterDictFreqTable[i].first.c_str(), MasterDictFreqTable[i].second);
	}
	
	fclose(f);
	return true;
}

void OutputStatistics()
{
	printf("Approximate Script Statistics\n");

	int BytesSaved = 0;
	int PostTotalChars = 0;
	float PostAvgCharsPerLine = 0.0;

	for(VecFreqPairIt i = MasterDTEFreqTable.begin(); i < MasterDTEFreqTable.end(); i++)
		BytesSaved += i->second;  // One byte saved per occurance for DTE

	for(VecFreqPairIt i = MasterDictFreqTable.begin(); i < MasterDictFreqTable.end(); i++)
		BytesSaved += (int)((i->first.length() - DictEntrySize) * i->second);

	PostTotalChars = PrevTotalChars - BytesSaved;
	PostAvgCharsPerLine = (float)PostTotalChars / PrevTotalLines;
	printf("Compressable Data at Start: %d chars, %d lines, %.1f Avg / Line\n",
		PrevTotalChars, PrevTotalLines, PrevAvgCharsPerLine);
	printf("Roughly %d bytes saved\n", BytesSaved);
	printf("Uncompressed Data at End: %d chars, %.1f Avg / Line\n", PostTotalChars, PostAvgCharsPerLine);
	printf("[%d / %d] = %.1f%% Compression\n", BytesSaved, PrevTotalChars, (float)BytesSaved/PrevTotalChars*100);
}

void PerformSizeAnalysis()
{
	unsigned int ErrorCharPos = 0;
	printf("Performing Script Encoding with the Input Table %s\n", cf->Settings.InputTable.c_str());
	Table Input;
	printf("Opening Table");
	int testval = Input.OpenTable(cf->Settings.InputTable.c_str());

	switch(testval) // Check error codes
	{
		case TBL_OK:
			printf(" [Done]\n");
			break;
		case TBL_PARSE_ERROR:
			printf(": The table was formatted improperly\n");
			return;
		case TBL_OPEN_ERROR:
			printf("The table could not be opened\n");
			return;
	}

	printf("Encoding Script");
	int InputHexSize = Input.EncodeStream(ScriptBuf, ErrorCharPos);
	if(InputHexSize == -1)
	{
		printf(": Unrecognized character %c in the script\n", ScriptBuf[ErrorCharPos]);
		return;
	}
	printf(" [Done]\n");

	//for(ListTblStringIt i = Input.StringTable.begin(); i != Input.StringTable.end(); i++)
	//	InputHexSize += (int)i->Text.size();

	printf("Input Script Encoding: ScriptBytes=%d, HexBytes=%d\n", ScriptBuf.length(), InputHexSize);

	printf("Performing Script Encoding with the Output Table %s\n", cf->Settings.OutputTable.c_str());
	Table Output;
	printf("Opening Table");
	testval = Output.OpenTable(cf->Settings.OutputTable.c_str());

	switch(testval) // Check error codes
	{
		case TBL_OK:
			printf(" [Done]\n");
			break;
		case TBL_PARSE_ERROR:
			printf(": The table was formatted improperly\n");
			return;
		case TBL_OPEN_ERROR:
			printf(": The table could not be opened\n");
			return;
	}

	printf("Encoding Script");
	int OutputHexSize = Output.EncodeStream(ScriptBuf, ErrorCharPos);
	if(OutputHexSize == -1)
	{
		printf(": Unrecognized character %c in the script\n", ScriptBuf[ErrorCharPos]);
		return;
	}
	printf(" [Done]\n");

	FILE* f = fopen("inserttest.bin", "wb");

	for(ListTblStringIt i = Output.StringTable.begin(); i != Output.StringTable.end(); i++)
		fwrite(i->Text.data(), 1, i->Text.size(), f);

	fclose (f);

	int BytesSaved = InputHexSize - OutputHexSize;
	printf("Output Script Encoding: ScriptBytes=%d, HexBytes=%d\n", ScriptBuf.length(), OutputHexSize);
	printf("[%d / %d] = %.1f%% Compression; %d bytes saved\n", BytesSaved, InputHexSize, (float)BytesSaved / InputHexSize * 100.0, BytesSaved);
}

void RemoveIgnoreStrings()
{
	// Remove IgnoreBlocks first
	bool bInBlock = false;
	// StringTable at this point holds one script per entry
	for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
	{
		// New script file
		string EndBlock = "";
		size_t blockpos = i->length();

parseagain: // Until a whole script file is parsed

		if(bInBlock)
		{
			size_t searchpos = i->find(EndBlock, 0);
			if(searchpos != string::npos) // Found the end block value
			{
				if(searchpos == 0) // At the beginning, no need to split
				{
					i->erase(0, EndBlock.length());
					blockpos = i->length();
				}
				else if(i->length() == (searchpos + EndBlock.length())) // At the end, no split
				{
					i->erase(blockpos, searchpos + EndBlock.length() - blockpos);
				}
				else // In the middle, need to split
				{
					string s = "";
					/*StringTableIt insertit = i;
					insertit++;
					insertit = StringTable.insert(insertit, s);
					i->swap(*insertit);
					*i = insertit->substr(0, blockpos-1);
					insertit->erase(0, searchpos + EndBlock.length());
					
					EndBlock = "";
					blockpos = i->length();*/

					// Add the end of the string and erase it.

					StringTableIt insertit = i;
					insertit++;
					insertit = StringTable.insert(insertit, s);

					size_t len = i->length() - searchpos - EndBlock.length(); // Don't copy the Block characters
					insertit->reserve(len);
					*insertit = i->substr(searchpos+EndBlock.length(), len);

					if(insertit->length() < 2) // Don't bother adding strings of 1 length
					{
						StringTable.erase(insertit);
						//StringTableIt insertit = i;
						//insertit++;
						//StringTable.insert(insertit, s);
					}
					i->erase(blockpos, i->length()-blockpos);
					// This is to reduce the string capacity, which can result in memory usage of
					// hundreds of MBs otherwise
					string temp = *i;
					i->swap(temp);
					EndBlock = "";
					blockpos = i->length();

					goto parseagain;
				}
				bInBlock = false;
			}
			else // No end comment to the end of the script file
			{
				i->erase(blockpos, i->length()-blockpos);
				continue; // Next script file
			}
		}

		for(VecStringPairIt j = cf->Settings.IgnoreBlocks.begin(); j != cf->Settings.IgnoreBlocks.end(); j++)
		{
			size_t searchpos = i->find(j->first, 0);
			if(searchpos != string::npos)
			{
				if(searchpos < blockpos)
				{
					blockpos = searchpos;
					EndBlock = j->second;
					bInBlock = true;
				}
			}
		}

		if(bInBlock)
			goto parseagain;
	}

	// Remove IgnorePhrases last
	for(VecStringIt j = cf->Settings.IgnorePhrases.begin(); j != cf->Settings.IgnorePhrases.end(); j++)
		RemoveSubstringFromTable(*j);
}
void RemoveSubstringFromTable(const string& RemoveString)
{
	size_t pos = 0;
	size_t charstodel = 0;
	size_t TotalIts = StringTable.size(); // Number of original strings to check
	size_t j = 0;

	for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++, j++)
	{
		// Search the last occurance of a string so we delete at the end and won't need a copy operation
		// for substrings in the middle
		while((pos = i->rfind(RemoveString)) != string::npos) // While there are occurances
		{
			if(pos == 0) // At the beginning, no need to split
			{
				i->erase(0, RemoveString.length());
			}
			else if(i->length() == (pos + RemoveString.length())) // At the end, no split
			{
				i->erase(pos, RemoveString.length());
			}
			else // In the middle, need to split
			{
				// Add the end of the string and erase it.
				string s = i->substr(pos+RemoveString.length(), i->length()-pos); // Don't copy the RemoveString characters
				if(s.length() >= 2) // Don't bother adding strings of 1 length
				{
					StringTableIt insertit = i;
					insertit++;
					StringTable.insert(insertit, s);
				}
				i->erase(pos, i->length()-pos);
			}
		}
	}
}

inline bool SortFrequencyMapDesc(const FrequencyPair& a, const FrequencyPair& b)
{
	return a.second > b.second;
}

inline bool SortFrequencyMapDictSizeDesc(const FrequencyPair& a, const FrequencyPair& b)
{
	return ((a.first.length() - DictEntrySize) * a.second) > ((b.first.length() - DictEntrySize) * b.second);
}

inline bool FreqPairOccurancePredDictSize(const FrequencyPair& a, const FrequencyPair& b)
{
	return ((a.first.length() - DictEntrySize) * a.second) < ((b.first.length() - DictEntrySize) * b.second);
}

inline bool FreqPairOccurancePred(const FrequencyPair& a, const FrequencyPair& b)
{
	return a.second < b.second;
}

// Old code or code that isn't working fully
/*void RemoveSubstringFromTable(const string& RemoveString, FrequencyMap& FreqMap, int MinSize, int MaxSize)
{
	size_t pos = 0;
	size_t charstodel = 0;
	size_t TotalIts = StringTable.size(); // Number of original strings to check
	size_t j = 0;

	for(StringTableIt i = StringTable.begin(); i != StringTable.end() && j < TotalIts; i++, j++)
	{
		// Perform Frequency Table adjustments
		// This saves a LOT of CPU compared to reanalyzing a frequency table for every pass
		size_t minpos, maxpos; // String positions on where to perform the map readjustments
		pos = i->length() - 1;  // Start from end of the string
		while((pos = i->rfind(RemoveString, pos)) != string::npos) // While there are occurances
		{
			minpos = pos - (RemoveString.length() - 1) - (MaxSize - 1);
			if((int)minpos < 0)
				minpos = 0;
			maxpos = pos + (RemoveString.length() - 1) + (MaxSize);

			AdjustFrequency(i, FreqMap, (int)minpos, (int)maxpos, MinSize, MaxSize);
			pos--;
			if((int)pos < 0)
				break;
		}

		// Search the last occurance of a string so we delete at the end and won't need a copy operation
		// for substrings in the middle
		/*while((pos = i->rfind(RemoveString)) != string::npos) // While there are occurances
		{
			if(pos == 0) // At the beginning, no need to split
			{
				i->erase(0, RemoveString.length());
			}
			else if(i->length() == (pos + RemoveString.length())) // At the end, no split
			{
				i->erase(pos, RemoveString.length());
			}
			else // In the middle, need to split
			{
				// Add the end of the string and erase it.
				string s = i->substr(pos+RemoveString.length(), i->length()-pos); // Don't copy the RemoveString characters
				if(s.length() >= 2) // Don't bother adding strings of 1 length
					StringTable.push_back(s);
				i->erase(pos, i->length()-pos);
			}
		}*/ /*
	}
}

inline void AdjustFrequency(StringTableIt it, FrequencyMap& FreqMap,
							int MinPos, int MaxPos, int MinSize, int MaxSize)
{
	bool isMarked; // Marked strings were already subtracted from the frequency
	for(int i = MinPos; i <= MaxPos - MinSize; i++)
	{
		isMarked = false;
		int maxcomps = MaxSize;
		if(maxcomps >= MaxPos-i)
			maxcomps = MaxPos-i;
		for(int j = MinSize; j <= maxcomps; j++)
		{
			for(VecIntPairIt k = it->MarkedText.begin();
				k != it->MarkedText.end(); k++)
			{
				if(i >= k->first && (i+j) <= k->second) // Already marked
				{
					isMarked = true;
					break;
				}
			}

			if(!isMarked)
			{
				IntPair match(MinPos, MaxPos);
				string s = it->Line.substr(i, j);
				it->MarkedText.push_back(match);
				FreqMap[s]--;
			}
		}
	}
}*/

/*void GenerateSubstringFrequencyTable()
{
	FrequencyMap DictFreq;

	int TotalPasses = cf->Settings.DictEnd - cf->Settings.DictBegin + 1;
	printf("Beginning Substring Analysis: Building the Initial Frequency Table");

	// One pass
	for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
	{
		if(i->Line.length() < cf->Settings.DictMinString)
			continue;

		// Add each substring to the table
		int maxcompj = (int) i->Line.length() - cf->Settings.DictMinString;
		for(int j = 0; j <= maxcompj; j++) // For each position of the string
		{
			int maxcompk = cf->Settings.DictMaxString;
			if(maxcompk > (int)i->Line.length() - j)
				maxcompk = (int)i->Line.length() - j;

			for(int k = cf->Settings.DictMinString; k <= maxcompk; k++)
			{
				string s = i->Line.substr(j,k);

				FrequencyMapIt it = DictFreq.lower_bound(s);
				if(it != DictFreq.end() && !(DictFreq.key_comp()(s, it->first)))
					it->second++;
				else
					DictFreq.insert(it, FrequencyMap::value_type(s, 1));
			}
		}
	}

	printf("[Done]\nGenerating best matches: %d adjustment passes required\nPass #: 1", TotalPasses);

	for(int i = 1; i <= TotalPasses; i++)
	{
		if(i % 5 == 0)
			printf(" %d", i);

		FrequencyMapIt MaxVal = max_element(DictFreq.begin(), DictFreq.end(), FreqPairOccurancePred);
		MasterDictFreqTable.push_back(*MaxVal);
		if(i != TotalPasses) // Won't need to do this last pass
			RemoveSubstringFromTable(MaxVal->first, DictFreq, cf->Settings.DictMinString, cf->Settings.DictMaxString);
	}
}*/

/*void GenerateDTEFrequencyTable()
{
	FrequencyMap DTEFreq;
	int TotalPasses = cf->Settings.DTEEnd - cf->Settings.DTEBegin + 1;
	printf("Beginning DTE Analysis: Building the Initial Frequency Table ");

	// First pass builds the Frequency Table
	for(StringTableIt i = StringTable.begin(); i != StringTable.end(); i++)
	{
		if(i->Line.length() < 2)
			continue;

		for(size_t j = 0; j < i->Line.length() - 2; j++)
		{
			string s = i->Line.substr(j,2);

			FrequencyMapIt it = DTEFreq.lower_bound(s);
			if(it != DTEFreq.end() && !(DTEFreq.key_comp()(s, it->first))) // Found in the map
				it->second++;
			else // Not found
				DTEFreq.insert(it, FrequencyMap::value_type(s, 1));
		}
	}

	printf("[Done]\nGenerating best matches: %d adjustment passes required\nPass #: 1", TotalPasses);

	for(int i = 1; i <= TotalPasses; i++)
	{
		if(i % 5 == 0)
			printf(" %d", i);

		FrequencyMapIt MaxVal = max_element(DTEFreq.begin(), DTEFreq.end(), FreqPairOccurancePred);
		MasterDTEFreqTable.push_back(*MaxVal);
		if(i != TotalPasses) // Won't need to do this last pass
			RemoveSubstringFromTable(MaxVal->first, DTEFreq, 2, 2);
	}

	printf(" [Done]\n");
}*/

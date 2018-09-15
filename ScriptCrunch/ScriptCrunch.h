#pragma once
#include "stdafx.h"
#include <utility>
#include <vector>
#include <string>

using namespace std;

typedef pair<string,int> FrequencyPair;
typedef pair<int,int> IntPair;
typedef map<string,int> FrequencyMap;
typedef map<string,int> FrequencyHashMap;
typedef map<string,int>::iterator FrequencyMapIt;
typedef map<string,int>::iterator FrequencyHashMapIt;
typedef vector<FrequencyPair>::iterator VecFreqPairIt;
typedef vector<pair<int,int>>::iterator VecIntPairIt;
typedef vector<string>::iterator StringTableIt;

bool ParseScript(const char* Filename);

void GenerateFrequencyTable();
void GenerateDTEFrequencyTable();
void GenerateDictFrequencyTable();
void GenerateSubstringFrequencyTable();
void GenerateDTESubstringFrequencyTable();

void PrecalculateStatistics();
bool OutputTableFile(const char* Filename);
bool OutputFrequencyTable(const char* Filename);
bool OutputInsertFile(const char* Filename);
void OutputStatistics();
void PerformSizeAnalysis();

void RemoveIgnoreStrings();
void RemoveSubstringFromTable(const string& RemoveString);

//void RemoveSubstringFromTable(const string& RemoveString, FrequencyMap& FreqMap, int MinSize, int MaxSize);
//inline void AdjustFrequency(StringTableIt i, FrequencyMap& FreqMap, int MinPos, int MaxPos, int MinSize, int MaxSize);

inline bool SortFrequencyMapDictSizeDesc(const FrequencyPair& a, const FrequencyPair& b);
inline bool SortFrequencyMapDesc(const FrequencyPair& a, const FrequencyPair& b);
inline bool FreqPairOccurancePred(const FrequencyPair& a, const FrequencyPair& b);
inline bool FreqPairOccurancePredDictSize(const FrequencyPair& a, const FrequencyPair& b);
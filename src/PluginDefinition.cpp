//this file is part of notepad++
//Copyright (C)2022 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"
#include "menuCmdID.h"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;
bool g_sortSections = true;
bool g_sortKeys = true;

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE /*hModule*/)
{}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{

    //--------------------------------------------//
    //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
    //--------------------------------------------//
    // with function :
    // setCommand(int index,                      // zero based number to indicate the order of command
    //            TCHAR *commandName,             // the command name that you want to see in plugin menu
    //            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
    //            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
    //            bool check0nInit                // optional. Make this menu item be checked visually
    //            );
    setCommand(0, TEXT("Format INI"), formatIni, NULL, false);

    setCommand(1, TEXT("Sort Sections"), toggleSectionSort, NULL, g_sortSections);
    setCommand(2, TEXT("Sort Keys"), toggleKeySort, NULL, g_sortKeys);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//
struct IniSection {
    std::string header;
    std::vector<std::string> lines;

    bool operator<(const IniSection& other) const {
        return header < other.header;
    }
};

std::string trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, (last - first + 1));
}

std::vector<char> formatWhitespace(std::vector<char> buffer) {
    std::string str(buffer.begin(), buffer.end());
    std::stringstream ss(str);
    std::string line;
    std::string result = "";
    bool lastWasEmpty = false;

    while (std::getline(ss, line)) {
        std::string trimmed = trim(line);

        if (trimmed.empty()) {
            if (!lastWasEmpty && !result.empty()) {
                result += "\r\n";
                lastWasEmpty = true;
            }
        }
        else {
            result += trimmed + "\r\n";
            lastWasEmpty = false;
        }
    }
    return std::vector<char> (result.begin(), result.end());
}

std::vector<IniSection> parseSections(std::vector<char> buffer) {
    std::vector<IniSection> sections;
    std::string str(buffer.begin(), buffer.end());
    std::stringstream ss(str);
    std::string line;
    IniSection currentSection;
    currentSection.header = "";
    while (std::getline(ss, line)) {
        std::string trimmed = trim(line);
        if (line.size() > 2 && trimmed.front() == '[' && trimmed.back() == ']') {
            if (!currentSection.header.empty() || !currentSection.lines.empty()) {
                sections.push_back(currentSection);
            }
            currentSection.header = trimmed;
            currentSection.lines.clear();
        }
        else {
            currentSection.lines.push_back(line);
        }
    }
    sections.push_back(currentSection);

    return sections;
}

std::vector<IniSection> formatSections(std::vector<IniSection> sections) {
    auto startIt = (sections.size() > 0 && sections[0].header.empty()) ? sections.begin() + 1 : sections.begin();
    std::sort(startIt, sections.end());

    return sections;
}

std::vector<char> rebuildBuffer(std::vector<IniSection> sections) {
    std::string result = "";
    for (size_t i = 0; i < sections.size(); ++i) {
        bool hasContent = false;

        if (!sections[i].header.empty()) {
            result += sections[i].header + "\r\n";
            hasContent = true;
        }

        for (const auto& l : sections[i].lines) {
            std::string trimmedLine = trim(l);
            if (!trimmedLine.empty()) {
                result += trimmedLine + "\r\n";
                hasContent = true;
            }
        }

        if (hasContent && i < sections.size() - 1) {
            result += "\r\n";
        }
    }

    size_t last = result.find_last_not_of(" \t\r\n");
    if (last != std::string::npos) {
        result = result.substr(0, last + 1) + "\r\n";
    }

    return std::vector<char>(result.begin(), result.end());
}

bool normalizeKeyValue(const std::string& line, std::string& outKey, std::string& outLine)
{
    auto pos = line.find('=');
    if (pos == std::string::npos)
        return false;

    std::string key = trim(line.substr(0, pos));
    std::string value = trim(line.substr(pos + 1));

    if (key.empty())
        return false;

    outKey = key;
    outLine = key + "=" + value;
    return true;
}

void formatKeys(IniSection& section)
{
    std::vector<std::pair<std::string, std::string>> kvPairs;
    std::vector<std::string> others;

    for (const auto& line : section.lines) {
        std::string key, normalized;
        if (normalizeKeyValue(line, key, normalized)) {
            kvPairs.emplace_back(key, normalized);
        }
        else {
            std::string t = trim(line);
            if (!t.empty())
                others.push_back(t);
        }
    }

    std::sort(kvPairs.begin(), kvPairs.end(),
        [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

    section.lines.clear();

    for (const auto& kv : kvPairs)
        section.lines.push_back(kv.second);

    for (const auto& l : others)
        section.lines.push_back(l);
}

void formatIni()
{
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    HWND curScintilla = (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

    int len = ::SendMessage(curScintilla, SCI_GETLENGTH, 0, 0);
    if (len == 0) return;

    std::vector<char> buffer(len);
    ::SendMessage(curScintilla, SCI_GETTEXT, len, (LPARAM)buffer.data());

    buffer = formatWhitespace(buffer);

    std::vector<IniSection> sections = parseSections(buffer);
    if (g_sortSections) {
        sections = formatSections(sections);
    }
    if (g_sortKeys) {
        for (auto& sec : sections) {
            formatKeys(sec);
        }
    }

    buffer = rebuildBuffer(sections);
    std::string result(buffer.begin(), buffer.end());
    ::SendMessage(curScintilla, SCI_SETTEXT, 0, (LPARAM)result.c_str());
}

void toggleSectionSort() {
    g_sortSections = !g_sortSections;
    ::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[2]._cmdID, g_sortSections);
}
void toggleKeySort() {
    g_sortKeys = !g_sortKeys;
    ::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[3]._cmdID, g_sortKeys);
}
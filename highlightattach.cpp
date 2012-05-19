/*
 * Attach on highlight ZNC module
 * Copyright (C) sk89q <http://www.sk89q.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

// This module will re-attached detached channels whenever someone says
// something that matches a list of regular expressions.
//
// Use /msg *highlightattach help for help

#include "Chan.h"
#include "Modules.h"
#include <pcrecpp.h>

class CPattern
{
public:
    CPattern(const CString& sPattern) {
        m_sPattern = sPattern;
    }
    
    bool Matches(const CString& sMessage) {
        pcrecpp::RE re(m_sPattern);
        return (re.error().length() == 0) && re.PartialMatch(sMessage);
    }
    
    const CString& GetPattern() {
        return m_sPattern;
    }
    
    virtual ~CPattern() {
    }

private:
    CString m_sPattern;
    
};

class CHighlightAttach : public CModule
{
public:
    MODCONSTRUCTOR(CHighlightAttach) {
        MCString::iterator it;
        for (it = BeginNV(); it != EndNV(); ++it) {
            Add(it->first);
        }
        
        AddHelpCommand();
        AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CHighlightAttach::ListCmd), "", "List all patterns");
        AddCommand("Add", static_cast<CModCommand::ModCmdFunc>(&CHighlightAttach::AddCmd), "<pattern>", "Add a regex pattern to match in messages");
        AddCommand("Delete", static_cast<CModCommand::ModCmdFunc>(&CHighlightAttach::DeleteCmd), "<pattern>", "Remove a pattern to match in messages");
    }

    virtual ~CHighlightAttach() {
    }
    
    virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
        return true;
    }

    virtual EModRet OnChanMsg(CNick& nick, CChan& channel, CString& sMessage) {
        if (Matches(sMessage)) {
            Attach(channel);
        }
        
        return CONTINUE;
    }

    virtual EModRet OnChanAction(CNick& nick, CChan& channel, CString& sMessage) {
        if (Matches(sMessage)) {
            Attach(channel);
        }
        
        return CONTINUE;
    }

    virtual EModRet OnChanNotice(CNick& nick, CChan& channel, CString& sMessage) {
        if (Matches(sMessage)) {
            Attach(channel);
        }
        
        return CONTINUE;
    }
    
    bool IsValid(const CString& sPattern) {
        pcrecpp::RE reMatcher(sPattern);
        return reMatcher.error().length() == 0;
    }
    
    bool Add(const CString& sPattern) {
        // We don't want duplicates
        vector<CPattern>::iterator it = m_vPatterns.begin();
        for (; it != m_vPatterns.end(); ++it) {
            if (it->GetPattern() == sPattern)
                return false;
        }
        
        CPattern pattern(sPattern);
        m_vPatterns.push_back(pattern);
        SetNV(sPattern, "");

        return true;
    }
    
    bool Delete(const CString& sPattern) {
        vector<CPattern>::iterator it = m_vPatterns.begin();
        for (; it != m_vPatterns.end(); ++it) {
            if (it->GetPattern() == sPattern) {
                m_vPatterns.erase(it);
                DelNV(it->GetPattern());
                return true;
            }
        }
        
        return false;
    }
    
    bool Matches(CString& sMessage) {
        vector<CPattern>::iterator it = m_vPatterns.begin();
        for (; it != m_vPatterns.end(); ++it) {
            if (it->Matches(sMessage))
                return true;
        }
        
        return false;
    }
    
private:
    vector<CPattern> m_vPatterns;

    void Attach(CChan& channel) {
        if (!channel.IsDetached())
            return;
        
        channel.JoinUser();
    }

    void ListCmd(const CString& sCommand) {
        CTable Table;
        Table.AddColumn("Pattern");

        vector<CPattern>::iterator it = m_vPatterns.begin();
        for (; it != m_vPatterns.end(); ++it) {
            Table.AddRow();
            Table.SetCell("Pattern", it->GetPattern());
        }

        if (Table.size()) {
            PutModule(Table);
        } else {
            PutModule("No patterns known.");
        }
    }

    void AddCmd(const CString& sCommand) {
        CString sPattern = sCommand.Token(1);
        
        if (IsValid(sPattern)) {
            if (Add(sPattern)) {
                PutModule("Pattern added");
            } else {
                PutModule("Pattern already exists");
            }
        } else {
            PutModule("Pattern has errors");
        }
    }

    void DeleteCmd(const CString& sCommand) {
        CString sPattern = sCommand.Token(1);
        
        if (Delete(sPattern)) {
            PutModule("Pattern deleted");
        } else {
            PutModule("Pattern was not in the list");
        }
    }
};

MODULEDEFS(CHighlightAttach, "Re-attaches on highlighted names")

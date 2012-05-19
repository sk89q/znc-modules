/*
 * Auto-disconnect ZNC module
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

// This module will disconnect you from the IRC network if you've been
// disconnected from ZNC for a configurable extended amount of time.
// If you've been disconnected, reconnecting to ZNC will reconnect you
// to the network.
//
// Use /msg *autodisconnect Delay # to configure the delay in seconds

#include "main.h"
#include "User.h"
#include "Modules.h"
#include "IRCSock.h"
#include <time.h>

class CCheckTimer : public CTimer
{
public:
    CCheckTimer(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription) : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {
        Ping();
    }

    void Ping() {
        m_lastActivity = time(NULL);
    }

protected:
    virtual void RunJob() {
        CUser *pUser = m_pModule->GetUser();
        CIRCSock *pIRCSock = pUser->GetIRCSock();

        if (pUser->GetIRCConnectEnabled() && Inactive()) {
            if (pIRCSock && !pIRCSock->IsConnected())
                pIRCSock->Close();
            else if (pIRCSock)
                pIRCSock->Quit();

            pUser->SetIRCConnectEnabled(false);
        }
    }

private:
    time_t m_lastActivity;
    
    bool Inactive();
};

class CAutoDisconnect : public CModule
{
public:
    MODCONSTRUCTOR(CAutoDisconnect) {
        m_iDelay = GetNV("DisconnectDelay").ToUInt();
        if (m_iDelay < 1)
            m_iDelay = 60 * 15;
        
        AddHelpCommand();
        AddCommand("Delay", static_cast<CModCommand::ModCmdFunc>(&CAutoDisconnect::DelayCmd), "<seconds>", "Set the delay");
    }

    virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
        m_pTimer = new CCheckTimer(this, 2, 0, "Check timer",
                                   "Check that the user is still connected.");
        AddTimer(m_pTimer);
        return true;
    }

    virtual ~CAutoDisconnect() {
    }

    virtual void OnClientLogin() {
        CUser *pUser = GetUser();

        if (!pUser->GetIRCConnectEnabled()) {
            pUser->SetIRCConnectEnabled(true);
            pUser->CheckIRCConnect();
        }
    }

    virtual void OnClientDisconnect() {
        m_pTimer->Ping();
    }

    virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
        CUser *pUser = GetUser();
        CIRCSock *pIRCSock = pUser->GetIRCSock();

        if (sMessage.Equals(".leave", false)) {
            PutStatus("You've been disconnected!");

            if (pIRCSock && !pIRCSock->IsConnected())
                pIRCSock->Close();
            else if (pIRCSock)
                pIRCSock->Quit();

            pUser->SetIRCConnectEnabled(false);

            return HALTCORE;
        }

        return CONTINUE;
    }

    u_int GetDelay() {
        return m_iDelay;
    }

    virtual CString GetWebMenuTitle() {
        return "Auto-Disconnect";
    }

    virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
        if (sPageName != "index") {
            return false;
        }

        if (WebSock.IsPost()) {
            u_int iDelay = WebSock.GetRawParam("delay", true).ToUInt();

            if (iDelay > 0) {
                m_iDelay = iDelay;
                SetNV("DisconnectDelay", CString(iDelay));
            }
        }

        CTemplate& Row = Tmpl.AddRow("delay");
        Row["delay"] = CString(m_iDelay);

        return true;
    }

private:
    CCheckTimer *m_pTimer;
    u_int m_iDelay;

    void DelayCmd(const CString& sCommand) {
        u_int iDelay = sCommand.Token(1, true).ToUInt();

        if (iDelay <= 0) {
            PutModule("Must be number of seconds > 0");
        } else {
            m_iDelay = iDelay;
            SetNV("DisconnectDelay", CString(iDelay));
            PutModule("Delay set");
        }
    }

};

inline bool CCheckTimer::Inactive()
{
    return m_pModule->GetUser()->GetClients().size() == 0 &&
           difftime(time(NULL), m_lastActivity) > ((CAutoDisconnect*) m_pModule)->GetDelay();
}

MODULEDEFS(CAutoDisconnect, "Auto-disconnects after a delay.")

/*  PADnull
 *  Copyright (C) 2004-2009 PCSX2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <string>

#include "Pad.h"
#include "null/config.inl"

extern std::string s_strIniPath;
PluginConf Ini;

EXPORT_C_(void)
PADabout()
{
    SysMessage("PADnull: A simple null plugin.");
}

EXPORT_C_(void)
PADconfigure()
{
    PADLoadConfig();
    ConfigureLogging();
    PADSaveConfig();
}

void PADLoadConfig()
{
    const std::string iniFile(s_strIniPath + "/Padnull.ini");

    if (!Ini.Open(iniFile, READ_FILE)) {
        g_plugin_log.WriteLn("failed to open %s", iniFile.c_str());
        PADSaveConfig();  //save and return
        return;
    }

    conf.Log = Ini.ReadInt("logging", 0);
    Ini.Close();
}

void PADSaveConfig()
{
    const std::string iniFile(s_strIniPath + "/Padnull.ini");

    if (!Ini.Open(iniFile, WRITE_FILE)) {
        g_plugin_log.WriteLn("failed to open %s", iniFile.c_str());
        return;
    }

    Ini.WriteInt("logging", conf.Log);
    Ini.Close();
}

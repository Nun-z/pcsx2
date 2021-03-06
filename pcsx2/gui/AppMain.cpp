/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "App.h"
#include "GS.h"

#include "Plugins.h"
#include "ps2/BiosTools.h"

#include "Utilities/IniInterface.h"
#include "Utilities/AppTrait.h"

Pcsx2App& wxGetApp() {
   static Pcsx2App pcsx2;
   return pcsx2;
}


std::unique_ptr<AppConfig> g_Conf;

AspectRatioType iniAR;
bool switchAR;

static bool HandlePluginError( BaseException& ex )
{
	return false;
}

class PluginErrorEvent : public pxExceptionEvent
{
	typedef pxExceptionEvent _parent;

public:
	PluginErrorEvent( BaseException* ex=NULL ) : _parent( ex ) {}
	PluginErrorEvent( const BaseException& ex ) : _parent( ex ) {}

	virtual ~PluginErrorEvent() = default;
	virtual PluginErrorEvent *Clone() const { return new PluginErrorEvent(*this); }

protected:
	void InvokeEvent();
};

class PluginInitErrorEvent : public pxExceptionEvent
{
	typedef pxExceptionEvent _parent;

public:
	PluginInitErrorEvent( BaseException* ex=NULL ) : _parent( ex ) {}
	PluginInitErrorEvent( const BaseException& ex ) : _parent( ex ) {}

	virtual ~PluginInitErrorEvent() = default;
	virtual PluginInitErrorEvent *Clone() const { return new PluginInitErrorEvent(*this); }

protected:
	void InvokeEvent();

};

void PluginErrorEvent::InvokeEvent()
{
	if( !m_except ) return;

	ScopedExcept deleteMe( m_except );
	m_except = NULL;

	if( !HandlePluginError( *deleteMe ) )
		Console.Error( L"User-canceled plugin configuration; Plugins not loaded!" );
}

void PluginInitErrorEvent::InvokeEvent()
{
	if( !m_except ) return;

	ScopedExcept deleteMe( m_except );
	m_except = NULL;

	if( !HandlePluginError( *deleteMe ) )
		Console.Error( L"User-canceled plugin configuration after plugin initialization failure.  Plugins unloaded." );
}

// Returns a string message telling the user to consult guides for obtaining a legal BIOS.
// This message is in a function because it's used as part of several dialogs in PCSX2 (there
// are multiple variations on the BIOS and BIOS folder checks).
wxString BIOS_GetMsg_Required()
{
	return pxE(L"PCSX2 requires a PS2 BIOS in order to run.  For legal reasons, you *must* obtain a BIOS from an actual PS2 unit that you own (borrowing doesn't count).  Please consult the FAQs and Guides for further instructions."
		);
}

class BIOSLoadErrorEvent : public pxExceptionEvent
{
	typedef pxExceptionEvent _parent;

public:
	BIOSLoadErrorEvent(BaseException* ex = NULL) : _parent(ex) {}
	BIOSLoadErrorEvent(const BaseException& ex) : _parent(ex) {}

	virtual ~BIOSLoadErrorEvent() = default;
	virtual BIOSLoadErrorEvent *Clone() const { return new BIOSLoadErrorEvent(*this); }

protected:
	void InvokeEvent();

};

static bool HandleBIOSError(BaseException& ex)
{
	return false;
}

void BIOSLoadErrorEvent::InvokeEvent()
{
	if (!m_except) return;

	ScopedExcept deleteMe(m_except);
	m_except = NULL;

	if (!HandleBIOSError(*deleteMe))
	{
		Console.Warning("User canceled BIOS configuration.");
	}
}

// --------------------------------------------------------------------------------------
//  Pcsx2AppMethodEvent
// --------------------------------------------------------------------------------------
// Unlike pxPingEvent, the Semaphore belonging to this event is typically posted when the
// invoked method is completed.  If the method can be executed in non-blocking fashion then
// it should leave the semaphore postback NULL.
//
class Pcsx2AppMethodEvent : public pxActionEvent
{
	typedef pxActionEvent _parent;
	wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(Pcsx2AppMethodEvent);

protected:
	FnPtr_Pcsx2App	m_Method;

public:
	virtual ~Pcsx2AppMethodEvent() = default;
	virtual Pcsx2AppMethodEvent *Clone() const { return new Pcsx2AppMethodEvent(*this); }

	explicit Pcsx2AppMethodEvent( FnPtr_Pcsx2App method=NULL, SynchronousActionState* sema=NULL )
		: pxActionEvent( sema )
	{
		m_Method = method;
	}

	explicit Pcsx2AppMethodEvent( FnPtr_Pcsx2App method, SynchronousActionState& sema )
		: pxActionEvent( sema )
	{
		m_Method = method;
	}
	
	Pcsx2AppMethodEvent( const Pcsx2AppMethodEvent& src )
		: pxActionEvent( src )
	{
		m_Method = src.m_Method;
	}
		
	void SetMethod( FnPtr_Pcsx2App method )
	{
		m_Method = method;
	}
	
protected:
	void InvokeEvent()
	{
		if( m_Method ) (wxGetApp().*m_Method)();
	}
};


wxIMPLEMENT_DYNAMIC_CLASS( Pcsx2AppMethodEvent, pxActionEvent );

// --------------------------------------------------------------------------------------
//  Pcsx2AppTraits (implementations)  [includes pxMessageOutputMessageBox]
// --------------------------------------------------------------------------------------
// This is here to override pxMessageOutputMessageBox behavior, which itself is ONLY used
// by wxWidgets' command line processor.  The default edition is totally inadequate for
// displaying a readable --help command line list, so I replace it here with a custom one
// that formats things nicer.
//

wxMessageOutput* Pcsx2AppTraits::CreateMessageOutput()
{
	return _parent::CreateMessageOutput();
}

wxAppTraits* Pcsx2App::CreateTraits()
{
	return new Pcsx2AppTraits;
}

// ----------------------------------------------------------------------------
//         Pcsx2App Event Handlers
// ----------------------------------------------------------------------------

extern bool renderswitch;

void DoFmvSwitch(bool on)
{
	if (g_Conf->GSWindow.FMVAspectRatioSwitch != FMV_AspectRatio_Switch_Off)
   {
		if (on)
      {
			switchAR = true;
			iniAR = g_Conf->GSWindow.AspectRatio;
		}
      else
      {
			switchAR = false;
		}
	}

	if (EmuConfig.Gamefixes.FMVinSoftwareHack)
   {
		CoreThread.Pause();
		renderswitch = !renderswitch;
		CoreThread.Resume();
	}
}

bool Pcsx2App::HasPendingSaves() const
{
	AffinityAssert_AllowFrom_MainUI();
	return !!m_PendingSaves;
}

// A call to this method informs the app that there is a pending save operation that must be
// finished prior to exiting the app, or else data loss will occur.  Any call to this method
// should be matched by a call to ClearPendingSave().
void Pcsx2App::StartPendingSave()
{
	if( AppRpc_TryInvokeAsync(&Pcsx2App::StartPendingSave) ) return;
	++m_PendingSaves;
}

// If this method is called inappropriately then the entire pending save system will become
// unreliable and data loss can occur on app exit.  Devel and debug builds will assert if
// such calls are detected (though the detection is far from fool-proof).
void Pcsx2App::ClearPendingSave()
{
	if( AppRpc_TryInvokeAsync(&Pcsx2App::ClearPendingSave) ) return;

	--m_PendingSaves;
	pxAssertDev( m_PendingSaves >= 0, "Pending saves count mismatch (pending count is less than 0)" );
}

// NOTE: Plugins are *not* applied by this function.  Changes to plugins need to handled
// manually.  The PluginSelectorPanel does this, for example.
void AppApplySettings( const AppConfig* oldconf )
{
	AffinityAssert_AllowFrom_MainUI();
#ifdef __LIBRETRO__
	CoreThread.Pause();
#else
	ScopedCoreThreadPause paused_core;
#endif

	g_Conf->Folders.ApplyDefaults();

	// Ensure existence of necessary documents folders.  Plugins and other parts
	// of PCSX2 rely on them.

	g_Conf->Folders.MemoryCards.Mkdir();
	g_Conf->Folders.Savestates.Mkdir();
	g_Conf->Folders.Snapshots.Mkdir();
	g_Conf->Folders.Cheats.Mkdir();
	g_Conf->Folders.CheatsWS.Mkdir();

	g_Conf->EmuOptions.BiosFilename = g_Conf->FullpathToBios();

	CorePlugins.SetSettingsFolder( GetSettingsFolder().ToString() );

	// Update the compression attribute on the Memcards folder.
	// Memcards generally compress very well via NTFS compression.

	#ifdef __WXMSW__
	NTFS_CompressFile( g_Conf->Folders.MemoryCards.ToString(), g_Conf->McdCompressNTFS );
	#endif
	sApp.DispatchEvent( AppStatus_SettingsApplied );

#ifdef __LIBRETRO__
//	CoreThread.Resume();
#else
	paused_core.AllowResume();
#endif

}

// Invokes the specified Pcsx2App method, or posts the method to the main thread if the calling
// thread is not Main.  Action is blocking.  For non-blocking method execution, use
// AppRpc_TryInvokeAsync.
//
// This function works something like setjmp/longjmp, in that the return value indicates if the
// function actually executed the specified method or not.
//
// Returns:
//   FALSE if the method was not posted to the main thread (meaning this IS the main thread!)
//   TRUE if the method was posted.
//
bool Pcsx2App::AppRpc_TryInvoke( FnPtr_Pcsx2App method )
{
	if( wxThread::IsMain() ) return false;

	SynchronousActionState sync;
	PostEvent( Pcsx2AppMethodEvent( method, sync ) );
	sync.WaitForResult();

	return true;
}

// Invokes the specified Pcsx2App method, or posts the method to the main thread if the calling
// thread is not Main.  Action is non-blocking.  For blocking method execution, use
// AppRpc_TryInvoke.
//
// This function works something like setjmp/longjmp, in that the return value indicates if the
// function actually executed the specified method or not.
//
// Returns:
//   FALSE if the method was not posted to the main thread (meaning this IS the main thread!)
//   TRUE if the method was posted.
//
bool Pcsx2App::AppRpc_TryInvokeAsync( FnPtr_Pcsx2App method )
{
	if( wxThread::IsMain() ) return false;
	PostEvent( Pcsx2AppMethodEvent( method ) );
	return true;
}

// Posts a method to the main thread; non-blocking.  Post occurs even when called from the
// main thread.
void Pcsx2App::PostAppMethod( FnPtr_Pcsx2App method )
{
	PostEvent( Pcsx2AppMethodEvent( method ) );
}

SysMainMemory& Pcsx2App::GetVmReserve()
{
	if (!m_VmReserve) m_VmReserve = std::unique_ptr<SysMainMemory>(new SysMainMemory());
	return *m_VmReserve;
}
void Pcsx2App::SysExecute()
{
	Pcsx2App::SysExecute(CDVD_SourceType::NoDisc);
}
void Pcsx2App::SysExecute( CDVD_SourceType cdvdsrc, const wxString& elf_override )
{
	ProcessMethod( AppSaveSettings );

	// if something unloaded plugins since this messages was queued then it's best to ignore
	// it, because apparently too much stuff is going on and the emulation states are wonky.
	if( !CorePlugins.AreLoaded() ) return;

	DbgCon.WriteLn( Color_Gray, "(SysExecute) received." );

	CoreThread.ResetQuick();
	CDVDsys_SetFile(CDVD_SourceType::Iso, g_Conf->CurrentIso );
	CDVDsys_ChangeSource( cdvdsrc);

	if( !CoreThread.HasActiveMachine() )
		CoreThread.SetElfOverride( elf_override );

	CoreThread.Resume();
}

// Returns true if there is a "valid" virtual machine state from the user's perspective.  This
// means the user has started the emulator and not issued a full reset.
// Thread Safety: The state of the system can change in parallel to execution of the
// main thread.  If you need to perform an extended length activity on the execution
// state (such as saving it), you *must* suspend the Corethread first!
__fi bool SysHasValidState()
{
	return CoreThread.HasActiveMachine();
}

SysMainMemory& GetVmMemory()
{
	return wxGetApp().GetVmReserve();
}

SysCoreThread& GetCoreThread()
{
	return CoreThread;
}

SysMtgsThread& GetMTGS()
{
	return mtgsThread;
}

SysCpuProviderPack& GetCpuProviders()
{
	return *wxGetApp().m_CpuProviders;
}

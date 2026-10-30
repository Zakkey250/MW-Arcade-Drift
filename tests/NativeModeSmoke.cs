using System;
using System.IO;
using System.Threading;
using System.Runtime.InteropServices;
class NativeModeSmoke {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr LoadLibraryW(string path);
 [DllImport("kernel32.dll",CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module,string name);
 [StructLayout(LayoutKind.Sequential,Pack=8)] struct State {
  public uint size,version,manualMode,effectiveMode,forced,ready,vehicleReady,appliedMode;
  public ulong tick,owner,token,expires;
 }
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int GetMode(ref State state);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int Acquire(ulong owner,uint ttl,out ulong token);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int Renew(ulong owner,ulong token,uint ttl);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int Release(ulong owner,ulong token);
 static T Bind<T>(IntPtr module,string name){var address=GetProcAddress(module,name);if(address==IntPtr.Zero)throw new Exception("Missing export "+name);return (T)(object)Marshal.GetDelegateForFunctionPointer(address,typeof(T));}
 static void Check(bool result,string reason){if(!result)throw new Exception(reason);}
 static int Main(string[] args){
  try {
   Check(IntPtr.Size==4&&args.Length==2,"x86 host and arguments required");
   bool logging=args[1]=="on";string dll=Path.GetFullPath(args[0]),log=Path.ChangeExtension(dll,".log");
   var module=LoadLibraryW(dll);Check(module!=IntPtr.Zero,"LoadLibrary failed");
   var get=Bind<GetMode>(module,"MWArcadeDriftGetModeV1");var acquire=Bind<Acquire>(module,"MWArcadeDriftAcquireSpecialV1");
   var renew=Bind<Renew>(module,"MWArcadeDriftRenewSpecialV1");var release=Bind<Release>(module,"MWArcadeDriftReleaseSpecialV1");
   Check(GetProcAddress(module,"MWArcadeDriftGetState")!=IntPtr.Zero,"legacy state API retained");
   Check(GetProcAddress(module,"MWArcadeDriftNativeHandlingVersionV1")!=IntPtr.Zero,"EventLoader native handling contract retained");
   if(logging){bool rejected=false;for(int i=0;i<50;i++){Thread.Sleep(100);if(File.Exists(log)&&File.ReadAllText(log).Contains("REJECTED exe=")){rejected=true;break;}}Check(rejected,"wrong host must reject before hooks");}
   else {Thread.Sleep(700);Check(!File.Exists(log),"OFF must not create log");}
   Check(!File.Exists(Path.ChangeExtension(dll,".csv")),"wrong host must not create telemetry");
   State state=new State{size=64,version=1};Check(Marshal.SizeOf(typeof(State))==64&&get(ref state)!=0,"x86 mode ABI round trip");
   Check(state.ready==0&&state.vehicleReady==0&&state.forced==0,"unsupported host never ready or forced");
   ulong token;Check(acquire(10,1000,out token)==0&&token==0,"unsupported host cannot acquire override");
   Check(renew(10,1,1000)==0&&release(10,1)==0,"invalid renew and release rejected");
   state.size=63;Check(get(ref state)==0,"wrong struct size rejected");state.size=64;state.version=2;Check(get(ref state)==0,"wrong API version rejected");
   Console.WriteLine("PASS x86 native exports, 64-byte ABI, old API, unsupported-host rejection, invalid requests, logging "+(logging?"ON":"OFF"));return 0;
  }catch(Exception e){Console.Error.WriteLine(e);return 1;}
 }
}

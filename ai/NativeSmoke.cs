using System;
using System.Runtime.InteropServices;
using System.Threading;
class NativeSmoke {
 [DllImport("kernel32",CharSet=CharSet.Unicode)]static extern IntPtr LoadLibrary(string p);
 [DllImport("kernel32")]static extern IntPtr GetProcAddress(IntPtr h,string n);
 [DllImport("kernel32")]static extern bool FreeLibrary(IntPtr h);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate uint Query(IntPtr p);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate uint Acquire(IntPtr p,IntPtr token);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate uint Release(ulong owner,ulong token);
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate uint State(ulong owner,ulong token,IntPtr p);
 static void Check(bool v,string s){if(!v)throw new Exception(s);}
 static T Fn<T>(IntPtr h,string n){var p=GetProcAddress(h,n);Check(p!=IntPtr.Zero,n);return (T)(object)Marshal.GetDelegateForFunctionPointer(p,typeof(T));}
 static void Main(string[] args){
  var h=LoadLibrary(args[0]);Check(h!=IntPtr.Zero,"LoadLibrary");Thread.Sleep(200);
  var caps=Fn<Query>(h,"MWArcadeDriftAI_GetCapabilitiesV1");var acquire=Fn<Acquire>(h,"MWArcadeDriftAI_AcquireV1");var refresh=Fn<Query>(h,"MWArcadeDriftAI_RefreshV1");var release=Fn<Release>(h,"MWArcadeDriftAI_ReleaseV1");var state=Fn<State>(h,"MWArcadeDriftAI_GetStateV1");
  var intent=Fn<Query>(h,"MWArcadeDriftAI_SetIntentV1");var path=Fn<Query>(h,"MWArcadeDriftAI_SetPathAssistV1");
  Check(GetProcAddress(h,"MWArcadeDriftAI_NativeHandlingV1")!=IntPtr.Zero&&GetProcAddress(h,"MWArcadeDriftAI_NativeTireRaceV1")!=IntPtr.Zero,"native handling exports");
  Check(intent(IntPtr.Zero)==2&&intent(new IntPtr(1))==2,"malformed intent rejected");
  Check(path(IntPtr.Zero)==2&&path(new IntPtr(1))==2,"malformed path assistance rejected");
  var p=Marshal.AllocHGlobal(128);var t=Marshal.AllocHGlobal(8);
  for(int i=0;i<128;i++)Marshal.WriteByte(p,i,0);Marshal.WriteInt32(p,32);Marshal.WriteInt32(p,4,1);
  Check(caps(p)==0&&Marshal.ReadInt32(p,8)==0,"wrong-host ready must be zero");
  Check(Marshal.ReadInt32(p,12)==16&&Marshal.ReadInt32(p,16)==6,"capabilities");Check(caps(IntPtr.Zero)==2,"null caps");
  Marshal.WriteInt32(p,64);Marshal.WriteInt32(p,8,0x20000);Marshal.WriteInt32(p,12,1);Marshal.WriteInt32(p,16,1);Marshal.WriteInt32(p,20,500);Marshal.WriteInt64(p,24,1);Marshal.WriteInt64(p,32,1);
  Check(acquire(p,t)==1&&Marshal.ReadInt64(t)==0,"wrong host cannot acquire");Check(acquire(new IntPtr(1),t)==2,"invalid binding pointer");
  Check(refresh(new IntPtr(1))==2,"invalid refresh pointer must not leave registry locked");Check(release(1,1)==7,"registry not locked after bad pointer");
  Marshal.WriteInt32(p,128);Check(state(1,1,p)==7,"no binding");Check(state(1,1,IntPtr.Zero)==2,"null state");
  Check(caps(p)==2,"wrong size");Marshal.WriteInt32(p,32);Check(caps(p)==0,"caps after malformed calls");
  Marshal.FreeHGlobal(p);Marshal.FreeHGlobal(t);FreeLibrary(h);Console.WriteLine("PASS x86 real AI exports, 32/64/32/128 ABI, wrong-host rejection and malformed-pointer recovery");
 }
}

# Read-only executable scanner: no DOOM process writes, cheat CVars or game launch.
# Run from the repository root: powershell -ExecutionPolicy Bypass -File .\tools\Inspect-DoomWorldPhysics.ps1
# Or supply -ExePath "D:\SteamLibrary\steamapps\common\DOOM\DOOMx64vk.exe".
param([string]$ExePath, [string]$ReportPath = (Join-Path $PSScriptRoot 'DOOM-native-physics-report.txt'), [switch]$ValidateOnly)
$ErrorActionPreference='Stop'
if (-not $ExePath -and -not $ValidateOnly) {
  $steamx86=[Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
  $steam64=[Environment]::GetEnvironmentVariable('ProgramFiles')
  $candidates=@(
    (Join-Path $steamx86 'Steam\steamapps\common\DOOM\DOOMx64vk.exe'),
    (Join-Path $steam64 'Steam\steamapps\common\DOOM\DOOMx64vk.exe'),
    'D:\SteamLibrary\steamapps\common\DOOM\DOOMx64vk.exe',
    'C:\SteamLibrary\steamapps\common\DOOM\DOOMx64vk.exe'
  )
  $ExePath=$candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if (-not $ValidateOnly -and (-not $ExePath -or -not (Test-Path -LiteralPath $ExePath -PathType Leaf))) {
  throw 'DOOMx64vk.exe not located. Pass -ExePath with its exact path.'
}
$source=@'
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
public static class KharvoxPhysicsInspector2026 {
    struct Sec { public string Name; public int Rva, Raw, Size; }
    static int ReadInt(byte[] b,int o){return BitConverter.ToInt32(b,o);}
    static int ReadShort(byte[] b,int o){return BitConverter.ToUInt16(b,o);}
    static int ToFile(int r,List<Sec> a) {
        foreach(var s in a)if(r>=s.Rva && r<s.Rva+s.Size)return s.Raw+r-s.Rva;
        return -1;
    }
    static int ToRva(int f,List<Sec> a) {
        foreach(var s in a)if(f>=s.Raw && f<s.Raw+s.Size)return s.Rva+f-s.Raw;
        return -1;
    }
    static string H(int i){return i<0?"unmapped":"0x"+i.ToString("X");}
    static string Dump(byte[] b,int f,int n) {
        if(f<0||f+n>b.Length)return "unavailable";
        var r=new StringBuilder();
        for(int i=0;i<n;i++){if(i>0)r.Append(" ");r.Append(b[f+i].ToString("X2"));}
        return r.ToString();
    }
    static List<int> FindString(byte[] b,string s) {
        byte[] n=Encoding.ASCII.GetBytes(s+"\0");
        var result=new List<int>();
        int offset=0;
        while(offset<b.Length-n.Length && result.Count<12) {
            int at=Array.IndexOf(b,n[0],offset);
            if(at<0||at+n.Length>b.Length)break;
            bool yes=true;for(int k=1;k<n.Length;k++)if(b[at+k]!=n[k]){yes=false;break;}
            if(yes)result.Add(at);
            offset=at+1;
        }
        return result;
    }
    static List<int> FindRipRefs(byte[] b,List<Sec> sections,int target) {
        var found=new List<int>();
        foreach(var s in sections) {
            if(s.Name!=".text")continue;
            int end=Math.Min(b.Length,s.Raw+s.Size);
            for(int at=s.Raw;at+7<=end;at++) {
                // REX.W LEA including R8-R15 destination registers. The earlier
                // scanner missed 4C 8D references, so "none" was inconclusive.
                if((b[at]!=0x48&&b[at]!=0x4c)
                    ||b[at+1]!=0x8d||(b[at+2]&0xc7)!=0x05)continue;
                int ip=s.Rva+at-s.Raw;
                if(ip+7+ReadInt(b,at+3)==target) {
                    found.Add(ip);if(found.Count>=24)return found;
                }
            }
        }
        return found;
    }
    // Print native code windows near known xrefs as RVA-addressed hex rows,
    // so a disassembler can reconstruct function calls without game access.
    // Never interpret arbitrary bytes here as a callable ABI.
    static void AppendCodeWindow(StringBuilder output,byte[] b,
        List<Sec> sections,int center,int before,int after) {
        output.AppendLine("  Code window around "+H(center)
            +" ("+before+" bytes before, "+after+" after):");
        int start=(center-before)&~15;
        int end=(center+after+15)&~15;
        for(int ip=start;ip<end;ip+=16) {
            int file=ToFile(ip,sections);
            output.AppendLine("    "+H(ip)+": "+Dump(b,file,16));
        }
    }
    // On-disk PE class RTTI/vtable tables often hold absolute VA pointers,
    // not RIP-relative LEAs. Find exact aligned references as *data leads*.
    static List<int> FindAbsoluteVaRefs(byte[] b,List<Sec> sections,long targetVa) {
        var refs=new List<int>();
        foreach(var sec in sections) {
            if(sec.Name!=".rdata"&&sec.Name!=".data")continue;
            int end=Math.Min(b.Length,sec.Raw+sec.Size);
            int start=(sec.Raw+7)&~7;
            for(int file=start;file+8<=end;file+=8) {
                if(BitConverter.ToInt64(b,file)!=targetVa)continue;
                refs.Add(sec.Rva+file-sec.Raw);
                if(refs.Count>=24)return refs;
            }
        }
        return refs;
    }
    static void AppendRefList(StringBuilder output,string label,List<int> refs) {
        var values=new List<string>();
        foreach(int addr in refs)values.Add(H(addr));
        output.AppendLine(label+(values.Count==0
            ?"none":String.Join(", ",values.ToArray())));
    }
    public static string Scan(string path) {
        byte[] b=File.ReadAllBytes(path);
        if(b.Length<4096||b[0]!=0x4d||b[1]!=0x5a)throw new Exception("Not an MZ executable");
        int pe=ReadInt(b,0x3c);
        if(pe<0||pe+256>=b.Length||ReadInt(b,pe)!=0x4550)throw new Exception("Not a PE executable");
        int count=ReadShort(b,pe+6), pos=pe+24+ReadShort(b,pe+20);
        bool pe64=ReadShort(b,pe+24)==0x20b;
        long imageBase=pe64?BitConverter.ToInt64(b,pe+24+24)
            :BitConverter.ToUInt32(b,pe+24+28);
        var ss=new List<Sec>();
        for(int i=0;i<count;i++){
            int o=pos+i*40;if(o+40>b.Length)break;
            ss.Add(new Sec {Name=Encoding.ASCII.GetString(b,o,8).TrimEnd('\0'),
                Rva=ReadInt(b,o+12),Size=ReadInt(b,o+16),Raw=ReadInt(b,o+20)});
        }
        var output=new StringBuilder();
        output.AppendLine("RIPSHIFT DOOM NATIVE PHYSICS - READ ONLY");
        output.AppendLine("Game executable bytes: "+b.Length);
        output.AppendLine("PE sections: "+ss.Count);
        output.AppendLine("PE image base: 0x"+imageBase.ToString("X"));
        int[] calls={0xd48abf,0xd48b30};
        foreach(int r in calls) {
            int f=ToFile(r,ss), dest=-1;
            if(f>=0&&f+5<b.Length&&b[f]==0xe8)dest=r+5+ReadInt(b,f+1);
            output.AppendLine("Native FocusTracker call "+H(r)+" -> "+H(dest)
                +(dest==0x1637ce0?" verified trace request queue":" different/unavailable"));
            output.AppendLine("  bytes: "+Dump(b,f,16));
        }
        output.AppendLine("Trace request builder 0x1637CE0: "+Dump(b,ToFile(0x1637ce0,ss),32));
        output.AppendLine("Call target is a request queue, NOT a proven physics hit accessor.");
        string[] names={"g_dragEntity","idDragEntity","idPhysics_RigidBody3DOF",
            "idPhysics_RigidBody","idProp_Moveable","idProp2",
            "physicsAttributes","isStatic","ApplyImpulse","AddForce"};
        foreach(string name in names) {
            output.AppendLine("NATIVE STRING: "+name);
            var matches=FindString(b,name);
            if(matches.Count==0){output.AppendLine("  Not found");continue;}
            foreach(int at in matches) {
                int r=ToRva(at,ss);
                output.AppendLine("  string RVA="+H(r));
                if(r<0)continue;
                var refs=FindRipRefs(b,ss,r);
                AppendRefList(output,"  code references (RIP LEA): ",refs);
                if(pe64)AppendRefList(output,"  absolute VA data refs: ",
                    FindAbsoluteVaRefs(b,ss,imageBase+r));
                // Exact code xref address windows only, no assumptions
                // about the surrounding function or a vtable layout.
                int shown=0;
                foreach(int codeRef in refs) {
                    if(shown++>=4)break;
                    AppendCodeWindow(output,b,ss,codeRef,64,128);
                }
            }
        }
        output.AppendLine();
        output.AppendLine("FOCUSED NATIVE CALLSITE WINDOWS");
        int[] pivots={0xd48abf,0xd48b30,0x1637ce0,0xd48938,
            0x122136,0x1b4224,0x1344d54,0x1cdb34,0x13c2374};
        foreach(int pivot in pivots)
            AppendCodeWindow(output,b,ss,pivot,96,192);
        output.AppendLine();
        output.AppendLine("All offsets are leads only. No method ABI or writable");
        output.AppendLine("physics target is confirmed by a string or hex window.");
        return output.ToString();
    }
}
'@
if (-not ('KharvoxPhysicsInspector2026' -as [type])) {
  Add-Type -TypeDefinition $source -Language CSharp
}
if ($ValidateOnly) { Write-Host 'Native physics inspector compiled successfully'; return }
$resolved=(Resolve-Path -LiteralPath $ExePath).Path
$report=[KharvoxPhysicsInspector2026]::Scan($resolved)
[IO.File]::WriteAllText($ReportPath,$report,[Text.Encoding]::UTF8)
Write-Host "Read-only physics report: $ReportPath"

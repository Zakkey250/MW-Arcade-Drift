using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Drawing;
using System.Windows.Forms;
using System.Collections.Generic;
using System.Web.Script.Serialization;
using System.Globalization;
using System.Diagnostics;
using System.Security.Cryptography;

class Field {
 public string key {get;set;} public string section {get;set;} public string ja {get;set;} public string en {get;set;} public double min {get;set;} public double max {get;set;} public double value {get;set;}
}
class Editor : Form {
 static JavaScriptSerializer json=new JavaScriptSerializer();
 string root,language="auto",active="",driftHash,cameraHash;
 Dictionary<string,object> drift,camera,baseDrift,baseCamera;
 List<Field> fields;bool changing,dirty;int lastTarget;
 bool testing; DialogResult testDecision; string lastError;
 ComboBox targets=new ComboBox(),languages=new ComboBox(),mode=new ComboBox();
 TabControl tabs=new TabControl();Label location=new Label(),status=new Label();
 Button save=new Button(),refresh=new Button(),browse=new Button();
 public string T(string ja,string en){return Japanese?ja:en;}
 bool Japanese {get{return language=="ja"||(language=="auto"&&CultureInfo.CurrentUICulture.TwoLetterISOLanguageName=="ja");}}
 static Dictionary<string,object> Read(string path){return json.Deserialize<Dictionary<string,object>>(File.ReadAllText(path,Encoding.UTF8));}
 static Dictionary<string,object> Clone(Dictionary<string,object> value){return json.Deserialize<Dictionary<string,object>>(json.Serialize(value));}
 static string Hash(string path){using(var sha=SHA256.Create())return Convert.ToBase64String(sha.ComputeHash(File.ReadAllBytes(path)));}
 static string Pretty(object value){
  string s=json.Serialize(value);var b=new StringBuilder();int depth=0;bool quoted=false,escape=false;
  foreach(char c in s){if(quoted){b.Append(c);if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;continue;}
   if(c=='"'){quoted=true;b.Append(c);}else if(c=='{'||c=='['){b.Append(c).Append('\n');depth++;b.Append(' ',depth*2);}else if(c=='}'||c==']'){b.Append('\n');depth--;b.Append(' ',depth*2).Append(c);}else if(c==','){b.Append(c).Append('\n').Append(' ',depth*2);}else if(c==':')b.Append(": ");else b.Append(c);
  }return b.ToString()+"\n";
 }
 string Folder {get{return active==""?root:Path.Combine(root,"vehicles",active);}}
 string DriftFile {get{return Path.Combine(Folder,active==""?"default_drift.json":"drift.json");}}
 string CameraFile {get{return Path.Combine(Folder,active==""?"default_camera.json":"camera.json");}}
 static void Atomic(string file,string content){
  string tmp=file+"."+Guid.NewGuid().ToString("N")+".tmp";
  File.WriteAllText(tmp,content,new UTF8Encoding(false));
  try{if(File.Exists(file))File.Replace(tmp,file,null);else File.Move(tmp,file);}finally{if(File.Exists(tmp))File.Delete(tmp);}
 }
 public Editor(string path){
  root=Path.GetFullPath(path);fields=json.Deserialize<List<Field>>(File.ReadAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"fields.json"),Encoding.UTF8));
  MinimumSize=new Size(880,660);Size=new Size(1060,850);StartPosition=FormStartPosition.CenterScreen;Font=new Font("Yu Gothic UI",10);Text="MW Arcade Drift — Profile Editor";
  var layout=new TableLayoutPanel{Dock=DockStyle.Fill,RowCount=4,ColumnCount=1,Padding=new Padding(16)};
  layout.RowStyles.Add(new RowStyle(SizeType.Absolute,48));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,58));layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,70));Controls.Add(layout);
  var bar=new FlowLayoutPanel{Dock=DockStyle.Fill,WrapContents=false};targets.Width=230;mode.Width=155;languages.Width=170;foreach(var c in new[]{targets,mode,languages})c.DropDownStyle=ComboBoxStyle.DropDownList;
  browse.Width=100;refresh.Width=90;bar.Controls.AddRange(new Control[]{targets,mode,languages,refresh,browse});layout.Controls.Add(bar,0,0);
  location.Dock=DockStyle.Fill;location.AutoEllipsis=true;layout.Controls.Add(location,0,1);tabs.Dock=DockStyle.Fill;layout.Controls.Add(tabs,0,2);
  var footer=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=2};footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,170));status.Dock=DockStyle.Fill;save.Dock=DockStyle.Fill;footer.Controls.Add(status,0,0);footer.Controls.Add(save,1,0);layout.Controls.Add(footer,0,3);
  targets.SelectedIndexChanged+=(s,e)=>{if(changing)return;if(!ConfirmDirty()){changing=true;targets.SelectedIndex=lastTarget;changing=false;return;}LoadTarget();};
  mode.SelectedIndexChanged+=(s,e)=>{if(changing)return;baseDrift=Clone(drift);baseCamera=Clone(camera);RenderTabs();};
  languages.SelectedIndexChanged+=(s,e)=>{if(changing)return;language=new[]{"auto","ja","en"}[languages.SelectedIndex];try{SaveLanguage();baseDrift=Clone(drift);baseCamera=Clone(camera);Translate();RenderTabs();}catch(Exception ex){Error(ex);}};
  tabs.SelectedIndexChanged+=(s,e)=>UpdateLocation();
  refresh.Click+=(s,e)=>{if(ConfirmDirty())OpenRoot();};browse.Click+=(s,e)=>{if(!ConfirmDirty())return;using(var d=new FolderBrowserDialog()){d.Description=T("MWArcadeDriftフォルダーを選択","Select the MWArcadeDrift folder");d.SelectedPath=root;if(d.ShowDialog()==DialogResult.OK){string previousRoot=root;root=d.SelectedPath;if(!OpenRoot())root=previousRoot;UpdateLocation();}}};save.Click+=(s,e)=>Save();
  FormClosing+=(s,e)=>{if(!ConfirmDirty())e.Cancel=true;};OpenRoot();
 }
 void Error(Exception ex){if(testing){lastError=ex.Message;return;}MessageBox.Show(this,T("適用できませんでした。\n","Could not apply changes.\n")+ex.Message,"MW Arcade Drift",MessageBoxButtons.OK,MessageBoxIcon.Warning);}
 DialogResult AskToSave(){
  if(testing)return testDecision;
  using(var dialog=new Form{Text=T("未保存の変更","Unsaved changes"),ClientSize=new Size(520,150),StartPosition=FormStartPosition.CenterParent,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false,ShowInTaskbar=false,Font=Font}){
   dialog.Controls.Add(new Label{Text=T("編集中の変更を保存しますか？\n破棄すると、未保存の変更は失われます。","Save your edits?\nDiscarding will lose all unsaved changes."),Left=18,Top=18,Width=480,Height=64});
   var yes=new Button{Text=T("保存","Save"),Left=165,Top=100,Width=105,DialogResult=DialogResult.Yes};
   var no=new Button{Text=T("破棄","Discard"),Left=280,Top=100,Width=105,DialogResult=DialogResult.No};
   var cancel=new Button{Text=T("キャンセル","Cancel"),Left=395,Top=100,Width=105,DialogResult=DialogResult.Cancel};
   dialog.Controls.AddRange(new Control[]{yes,no,cancel});dialog.AcceptButton=cancel;dialog.CancelButton=cancel;
   return dialog.ShowDialog(this);
  }
 }
 bool ConfirmDirty(){if(!dirty)return true;var r=AskToSave();if(r!=DialogResult.Yes&&r!=DialogResult.No)return false;if(r==DialogResult.Yes)return Save();dirty=false;return true;}
 void Translate(){
  int mi=Math.Max(0,mode.SelectedIndex);changing=true;mode.Items.Clear();mode.Items.AddRange(new object[]{T("簡易モード","Simple"),T("アドバンス","Advanced")});mode.SelectedIndex=mi;
  languages.Items.Clear();languages.Items.AddRange(new object[]{T("言語: システム自動","Language: System"),"日本語","English"});languages.SelectedIndex=language=="ja"?1:language=="en"?2:0;
  if(targets.Items.Count>0)targets.Items[0]=T("デフォルト（新規車の基準）","Default (template for new cars)");changing=false;
  save.Text=T("変更を保存","Save changes");refresh.Text=T("再読込","Reload");browse.Text=T("場所を選ぶ","Browse");UpdateLocation();
 }
 bool OpenRoot(){
  try{
   if(!File.Exists(Path.Combine(root,"default_drift.json"))||!File.Exists(Path.Combine(root,"default_camera.json")))throw new IOException(T("default_drift.json と default_camera.json があるフォルダーを選んでください。","Choose a folder containing default_drift.json and default_camera.json."));
   var prefs=Path.Combine(root,"settings.json");if(File.Exists(prefs)){var p=Read(prefs);language=p.ContainsKey("language")?Convert.ToString(p["language"]):"auto";}
   Read(Path.Combine(root,"default_drift.json"));Read(Path.Combine(root,"default_camera.json"));
   changing=true;targets.Items.Clear();targets.Items.Add("Default");var vehicles=Path.Combine(root,"vehicles");if(Directory.Exists(vehicles))foreach(string d in Directory.GetDirectories(vehicles).OrderBy(x=>x)){
    if((File.GetAttributes(d)&FileAttributes.ReparsePoint)==0&&File.Exists(Path.Combine(d,"drift.json"))&&File.Exists(Path.Combine(d,"camera.json")))targets.Items.Add(Path.GetFileName(d));
   }
   targets.SelectedIndex=0;changing=false;Translate();LoadTarget();return true;
  }catch(Exception ex){changing=false;save.Enabled=false;Error(ex);return false;}
 }
 void LoadTarget(){
  string previous=active;int previousIndex=lastTarget;
  try{using(var guard=new FileStream(Path.Combine(root,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
   active=targets.SelectedIndex<=0?"":Convert.ToString(targets.SelectedItem);
   var d=Read(DriftFile);var c=Read(CameraFile);string dh=Hash(DriftFile),ch=Hash(CameraFile);
   drift=d;camera=c;driftHash=dh;cameraHash=ch;lastTarget=targets.SelectedIndex;
   baseDrift=Clone(drift);baseCamera=Clone(camera);dirty=false;save.Enabled=true;RenderTabs();UpdateLocation();
  }}catch(Exception ex){active=previous;changing=true;targets.SelectedIndex=previousIndex;changing=false;save.Enabled=false;Error(ex);UpdateLocation();}
 }
 void UpdateLocation(){
  string name=active==""?T("デフォルト","Default"):active;
  Text=(dirty?"* ":"")+"MW Arcade Drift — "+name;
  location.Text=T("編集中: ","Editing: ")+name+"  /  "+(tabs.SelectedIndex==1?T("カメラ","Camera"):T("ドリフト","Drift"))+"\n"+(tabs.SelectedIndex==1?CameraFile:DriftFile);
  status.Text=(dirty?T("未保存の変更があります。\n","Unsaved changes.\n"):"")+T("保存後、ゲームで Ctrl+D を OFF → ON にして反映。\nデフォルトの変更は、作成済みの車種別設定を上書きしません。","After saving, switch Ctrl+D OFF then ON in game to apply.\nChanging defaults does not overwrite existing vehicle profiles.");
 }
 void Changed(){dirty=true;UpdateLocation();}
 void RenderTabs(){
  if(drift==null||camera==null)return;int index=Math.Max(0,tabs.SelectedIndex);tabs.SuspendLayout();while(tabs.TabPages.Count>0){var old=tabs.TabPages[0];tabs.TabPages.Remove(old);old.Dispose();}
  foreach(string section in new[]{"drift","camera"}){
   var page=new TabPage(section=="drift"?T("ドリフト","Drift"):T("カメラ","Camera"));var panel=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.TopDown,WrapContents=false,AutoScroll=true,Padding=new Padding(14)};page.Controls.Add(panel);tabs.TabPages.Add(page);
   var values=section=="drift"?drift:camera;var enabled=new CheckBox{Text=section=="drift"?T("この車両のドリフトを有効にする","Enable drift for this profile"):T("ドリフトカメラを有効にする","Enable drift camera"),Width=700,Height=34,Checked=!values.ContainsKey("enabled")||Convert.ToBoolean(values["enabled"])};
   enabled.CheckedChanged+=(s,e)=>{values["enabled"]=enabled.Checked;Changed();};panel.Controls.Add(enabled);
   if(mode.SelectedIndex==1){foreach(var f in fields.Where(f=>f.section==section))AddAdvanced(panel,values,f);}
   else {
    panel.Controls.Add(new Label{Text=T("50 = 読み込み時の値。関連する項目をまとめて調整します。","50 = values when loaded. Each slider adjusts a related group."),Width=760,Height=35});
    if(section=="drift"){
     AddSimple(panel,T("旋回力","Cornering"),new[]{"targetYawMax","targetPathSpeed","highSpeedGain","pathAssistLimit"},false);
     AddSimple(panel,T("滑り角・ハンドブレーキ","Slip angle / handbrake"),new[]{"baseSlipRad","slideSlipRad","handbrakeSlipRad","handbrakeTurnGain"},false);
     AddSimple(panel,T("速度維持","Speed retention"),new[]{"refundFraction","recoveryAccel","refundMaxAccel"},false);
    }else{
     AddSimple(panel,T("カメラの近さ","Camera closeness"),new[]{"minDistance","closeFraction","sideOffset"},false);
     AddSimple(panel,T("回り込み・前方の見通し","Orbit / road view"),new[]{"orbitGain","orbitMaxRad","lookAhead"},false);
     AddSimple(panel,T("カメラの穏やかさ","Camera smoothness"),new[]{"orbitSpeed","orbitAccel","zoomSpeed","zoomAccel","rollSpeed","rollAccel"},true);
    }
   }
  }tabs.SelectedIndex=index>1?0:index;tabs.ResumeLayout();UpdateLocation();
 }
 void AddAdvanced(FlowLayoutPanel panel,Dictionary<string,object> values,Field f){
  var row=new Panel{Width=780,Height=68};var label=new Label{Text=Japanese?f.ja:f.en,Left=0,Top=0,Width=410,Height=24};var track=new TrackBar{Left=0,Top=24,Width=620,Height=40,Minimum=0,Maximum=1000,TickStyle=TickStyle.None};var number=new NumericUpDown{Left=635,Top=26,Width=120,DecimalPlaces=4,Minimum=(decimal)f.min,Maximum=(decimal)f.max,Increment=.001m};
  double value=values.ContainsKey(f.key)?Convert.ToDouble(values[f.key],CultureInfo.InvariantCulture):f.value;value=Math.Max(f.min,Math.Min(f.max,value));bool busy=false;number.Value=(decimal)value;track.Value=(int)Math.Round((value-f.min)/(f.max-f.min)*1000);
  track.ValueChanged+=(s,e)=>{if(busy)return;busy=true;double n=f.min+(f.max-f.min)*track.Value/1000;number.Value=(decimal)n;values[f.key]=n;busy=false;Changed();};
  number.ValueChanged+=(s,e)=>{if(busy)return;busy=true;double n=(double)number.Value;track.Value=(int)Math.Round((n-f.min)/(f.max-f.min)*1000);values[f.key]=n;busy=false;Changed();};row.Controls.AddRange(new Control[]{label,track,number});panel.Controls.Add(row);
 }
 void AddSimple(FlowLayoutPanel panel,string title,string[] names,bool inverse){
  var row=new Panel{Width=780,Height=110};var label=new Label{Text=title,Width=660,Height=24};var val=new Label{Text="50",Left=700,Width=60};var track=new TrackBar{Top=28,Width=740,Minimum=0,Maximum=100,Value=50,TickFrequency=10};
  var hint=new Label{Top=78,Width=755,Height=28,Text=String.Join(" / ",fields.Where(f=>names.Contains(f.key)).Select(f=>Japanese?f.ja:f.en).ToArray())};
  track.ValueChanged+=(s,e)=>{double t=track.Value/100.0;foreach(var key in names){var f=fields.First(x=>x.key==key);var basis=f.section=="drift"?baseDrift:baseCamera;var target=f.section=="drift"?drift:camera;double b=basis.ContainsKey(key)?Convert.ToDouble(basis[key]):f.value;
    double factor=inverse?1.6-1.2*t:.7+.6*t;if(key=="minDistance")factor=1.25-.5*t;
    target[key]=Math.Max(f.min,Math.Min(f.max,b*factor));
   }val.Text=track.Value.ToString();Changed();};row.Controls.AddRange(new Control[]{label,val,track,hint});panel.Controls.Add(row);
 }
 void SaveLanguage(){
  using(var guard=new FileStream(Path.Combine(root,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
   var p=Path.Combine(root,"settings.json");var settings=File.Exists(p)?Read(p):new Dictionary<string,object>{{"schemaVersion",2}};settings["language"]=language;Atomic(p,Pretty(settings));
  }
 }
 bool Save(){
  string stage=Path.Combine(Path.GetTempPath(),"MWArcadeDrift-"+Guid.NewGuid().ToString("N"));
  try{
   Directory.CreateDirectory(stage);File.WriteAllText(Path.Combine(stage,"drift.json"),Pretty(drift),new UTF8Encoding(false));File.WriteAllText(Path.Combine(stage,"camera.json"),Pretty(camera),new UTF8Encoding(false));
   var checker=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ConfigCheck.exe");var start=new ProcessStartInfo(checker,"\""+stage+"\""){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true};
   using(var p=Process.Start(start)){string result=p.StandardOutput.ReadToEnd();p.WaitForExit();if(p.ExitCode!=0)throw new InvalidDataException(result);}
   using(var guard=new FileStream(Path.Combine(root,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
    if(Hash(DriftFile)!=driftHash||Hash(CameraFile)!=cameraHash)throw new IOException(T("別の操作でファイルが変更されました。再読込してください。","Files changed outside this editor. Reload first."));
    string backup=Path.Combine(root,"backups",DateTime.Now.ToString("yyyyMMdd-HHmmss-fff")+"-"+Guid.NewGuid().ToString("N").Substring(0,6));Directory.CreateDirectory(backup);File.WriteAllText(Path.Combine(backup,"target.txt"),DriftFile+"\n"+CameraFile,Encoding.UTF8);File.Copy(DriftFile,Path.Combine(backup,"drift.json"));File.Copy(CameraFile,Path.Combine(backup,"camera.json"));
    try{Atomic(DriftFile,Pretty(drift));Atomic(CameraFile,Pretty(camera));}catch{Atomic(DriftFile,File.ReadAllText(Path.Combine(backup,"drift.json")));Atomic(CameraFile,File.ReadAllText(Path.Combine(backup,"camera.json")));throw;}
   }driftHash=Hash(DriftFile);cameraHash=Hash(CameraFile);dirty=false;baseDrift=Clone(drift);baseCamera=Clone(camera);RenderTabs();status.Text=T("保存しました。ゲームで Ctrl+D を OFF → ON にしてください。","Saved. Switch Ctrl+D OFF then ON in game.");return true;
  }catch(Exception ex){Error(ex);return false;}finally{if(Directory.Exists(stage))Directory.Delete(stage,true);}
 }
 static void Check(bool value,string name){if(!value)throw new Exception("TEST FAILED: "+name);}
 public void SelfTest(){
  testing=true;language="en";Translate();
  var original=File.ReadAllText(DriftFile);drift["targetYawMax"]=.65;Changed();
  testDecision=DialogResult.Cancel;Check(!ConfirmDirty()&&dirty,"cancel preserves unsaved edits");
  var close=new FormClosingEventArgs(CloseReason.UserClosing,false);OnFormClosing(close);Check(close.Cancel&&dirty,"window close is cancelled");
  if(targets.Items.Count>1){targets.SelectedIndex=1;Check(targets.SelectedIndex==0&&active==""&&dirty,"cancel target switch preserves current target");}
  drift["targetYawMax"]=99;testDecision=DialogResult.Yes;Check(!ConfirmDirty()&&dirty&&File.ReadAllText(DriftFile)==original,"invalid save blocks exit without touching files");
  drift["targetYawMax"]=.65;File.AppendAllText(DriftFile," ");Check(!Save()&&dirty,"external conflict does not discard buffer");File.WriteAllText(DriftFile,original,new UTF8Encoding(false));
  Check(ConfirmDirty()&&!dirty,"save before close succeeds");Check(Convert.ToDouble(Read(DriftFile)["targetYawMax"])==.65,"saved value");
  Check(Directory.GetDirectories(Path.Combine(root,"backups")).Length>0,"backup exists");
  drift["targetYawMax"]=.7;Changed();testDecision=DialogResult.No;Check(ConfirmDirty()&&!dirty,"discard exits without saving");Check(Convert.ToDouble(Read(DriftFile)["targetYawMax"])==.65,"discard leaves file intact");
  LoadTarget();if(targets.Items.Count>1){targets.SelectedIndex=1;Check(active!=""&&DriftFile.Contains("vehicles"),"vehicle selected separately");targets.SelectedIndex=0;}
  mode.SelectedIndex=0;RenderTabs();var driftPanel=(FlowLayoutPanel)tabs.TabPages[0].Controls[0];var cameraPanel=(FlowLayoutPanel)tabs.TabPages[1].Controls[0];
  Check(driftPanel.Controls.OfType<Panel>().Count()==3&&cameraPanel.Controls.OfType<Panel>().Count()==3,"three simple controls per screen");
  var firstTrack=driftPanel.Controls.OfType<Panel>().First().Controls.OfType<TrackBar>().First();double baseYaw=Convert.ToDouble(drift["targetYawMax"]);double cameraBefore=Convert.ToDouble(camera["orbitGain"]);firstTrack.Value=75;
  Check(Convert.ToDouble(drift["targetYawMax"])>baseYaw&&Convert.ToDouble(camera["orbitGain"])==cameraBefore,"drift group modifies only drift");
  using(var guard=new FileStream(Path.Combine(root,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){Check(!Save()&&dirty,"busy lock preserves edits");}
  dirty=false;LoadTarget();
  foreach(string lang in new[]{"ja","en"}){language=lang;Translate();foreach(int m in new[]{0,1}){mode.SelectedIndex=m;RenderTabs();Check(tabs.TabCount==2,"separate drift and camera tabs");if(m==1){Check(((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().Count()==17,"all drift controls");Check(((FlowLayoutPanel)tabs.TabPages[1].Controls[0]).Controls.OfType<Panel>().Count()==20,"all camera controls");}}}
  dirty=false;File.WriteAllText(Path.Combine(root,"editor-test-result.txt"),"PASS close cancel, target cancel, invalid save, external conflict, save, backup, discard, target selection, localized tabs, simple groups, all advanced fields, shared lock",Encoding.UTF8);
 }
 public void Preview(string folder){
  Directory.CreateDirectory(folder);ShowInTaskbar=false;StartPosition=FormStartPosition.Manual;Location=new Point(-20000,-20000);Show();Application.DoEvents();
  foreach(string lang in new[]{"ja","en"})foreach(int m in new[]{0,1}){language=lang;Translate();mode.SelectedIndex=m;RenderTabs();for(int tab=0;tab<2;tab++){tabs.SelectedIndex=tab;Application.DoEvents();using(var b=new Bitmap(Width,Height)){DrawToBitmap(b,new Rectangle(0,0,Width,Height));b.Save(Path.Combine(folder,lang+"-"+m+"-"+tab+".png"));}}}
  dirty=false;Close();
 }
 [STAThread]static void Main(string[] args){
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  string root=args.Length>0?args[0]:AppDomain.CurrentDomain.BaseDirectory;
  try{using(var form=new Editor(root)){if(args.Length>1&&args[1]=="--self-test")form.SelfTest();else if(args.Length>2&&args[1]=="--preview")form.Preview(args[2]);else Application.Run(form);}}
  catch(Exception ex){if(args.Length>1){File.WriteAllText(Path.Combine(root,"editor-error.txt"),ex.ToString());Environment.ExitCode=1;return;}MessageBox.Show(ex.Message,"MW Arcade Drift",MessageBoxButtons.OK,MessageBoxIcon.Error);Environment.ExitCode=1;}
 }
}

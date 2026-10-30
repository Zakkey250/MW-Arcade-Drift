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
using System.Reflection;

[assembly: AssemblyTitle("MW Arcade Drift Editor")]
[assembly: AssemblyProduct("MW Arcade Drift Editor")]
[assembly: AssemblyVersion("0.2.0.0")]
[assembly: AssemblyFileVersion("0.2.0.0")]

class Field {
 public string key {get;set;} public string section {get;set;} public string ja {get;set;} public string en {get;set;} public double min {get;set;} public double max {get;set;} public double value {get;set;}
}
class Editor : Form {
 static JavaScriptSerializer json=new JavaScriptSerializer();
 string root,baseRoot,language="auto",active="",driftHash,cameraHash;
 bool specialFamily;int lastFamily;
 Dictionary<string,object> drift,camera;
 List<Field> fields,allFields;bool changing,dirty;int lastTarget,lastMode;
 bool testing; DialogResult testDecision; DialogResult? testModeDecision; string lastError;
 ComboBox targets=new ComboBox(),languages=new ComboBox(),mode=new ComboBox(),family=new ComboBox();
 TabControl tabs=new TabControl();Label location=new Label(),status=new Label();
 Button save=new Button(),refresh=new Button(),browse=new Button();
 public string T(string ja,string en){return Japanese?ja:en;}
 bool Japanese {get{return language=="ja"||(language=="auto"&&CultureInfo.CurrentUICulture.TwoLetterISOLanguageName=="ja");}}
 static Dictionary<string,object> Read(string path){return json.Deserialize<Dictionary<string,object>>(File.ReadAllText(path,Encoding.UTF8));}
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
  baseRoot=root=Path.GetFullPath(path);allFields=json.Deserialize<List<Field>>(File.ReadAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"fields.json"),Encoding.UTF8));SelectFields();
  MinimumSize=new Size(1000,660);Size=new Size(1060,850);StartPosition=FormStartPosition.CenterScreen;Font=new Font("Yu Gothic UI",10);Text="MW Arcade Drift — Profile Editor";
  var layout=new TableLayoutPanel{Dock=DockStyle.Fill,RowCount=4,ColumnCount=1,Padding=new Padding(16)};
  layout.RowStyles.Add(new RowStyle(SizeType.Absolute,48));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,58));layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,70));Controls.Add(layout);
  var bar=new FlowLayoutPanel{Dock=DockStyle.Fill,WrapContents=false};targets.Width=230;mode.Width=155;languages.Width=170;family.Width=145;foreach(var c in new[]{targets,mode,languages,family})c.DropDownStyle=ComboBoxStyle.DropDownList;
  browse.Width=100;refresh.Width=90;bar.Controls.AddRange(new Control[]{family,targets,mode,languages,refresh,browse});layout.Controls.Add(bar,0,0);
  location.Dock=DockStyle.Fill;location.AutoEllipsis=true;layout.Controls.Add(location,0,1);tabs.Dock=DockStyle.Fill;layout.Controls.Add(tabs,0,2);
  var footer=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=2};footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));footer.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,170));status.Dock=DockStyle.Fill;save.Dock=DockStyle.Fill;footer.Controls.Add(status,0,0);footer.Controls.Add(save,1,0);layout.Controls.Add(footer,0,3);
  family.SelectedIndexChanged+=(s,e)=>{
   if(changing||family.SelectedIndex<0||family.SelectedIndex==lastFamily)return;
   if(!ConfirmDirty()){changing=true;family.SelectedIndex=lastFamily;changing=false;return;}
   int previous=lastFamily;string previousRoot=root;
   specialFamily=family.SelectedIndex==1;root=specialFamily?Path.Combine(baseRoot,"modes","special"):baseRoot;SelectFields();
   if(OpenRoot())lastFamily=specialFamily?1:0;
   else{specialFamily=previous==1;root=previousRoot;SelectFields();OpenRoot();}
   changing=true;family.SelectedIndex=lastFamily;changing=false;
  };
  targets.SelectedIndexChanged+=(s,e)=>{if(changing)return;if(!ConfirmDirty()){changing=true;targets.SelectedIndex=lastTarget;changing=false;return;}LoadTarget();};
  mode.SelectedIndexChanged+=(s,e)=>{
   if(changing||mode.SelectedIndex<0||mode.SelectedIndex==lastMode)return;
   int selected=mode.SelectedIndex;
   var answer=testing?(testModeDecision??DialogResult.Yes):MessageBox.Show(this,
    T("編集中の値は保持されます。簡易スライダーを動かすと、関連する詳細項目を標準値からまとめて再計算します。\n編集モードを切り替えますか？",
      "Your edited values will be kept. Moving a Simple slider recalculates its related Advanced fields from the shipped defaults.\nSwitch editing mode?"),
    T("編集モードの切替","Switch editing mode"),MessageBoxButtons.YesNo,MessageBoxIcon.Information,MessageBoxDefaultButton.Button2);
   if(answer!=DialogResult.Yes){changing=true;mode.SelectedIndex=lastMode;changing=false;return;}
   try{SaveEditorMode(selected);lastMode=selected;RenderTabs();}
   catch(Exception ex){changing=true;mode.SelectedIndex=lastMode;changing=false;Error(ex);}
  };
  languages.SelectedIndexChanged+=(s,e)=>{if(changing)return;language=new[]{"auto","ja","en"}[languages.SelectedIndex];try{SaveLanguage();Translate();RenderTabs();}catch(Exception ex){Error(ex);}};
  tabs.SelectedIndexChanged+=(s,e)=>UpdateLocation();
  refresh.Click+=(s,e)=>{if(ConfirmDirty())OpenRoot();};browse.Click+=(s,e)=>{if(!ConfirmDirty())return;using(var d=new FolderBrowserDialog()){d.Description=T("MWArcadeDriftフォルダーを選択","Select the MWArcadeDrift folder");d.SelectedPath=root;if(d.ShowDialog()==DialogResult.OK){string previousRoot=root,previousBase=baseRoot;baseRoot=d.SelectedPath;root=specialFamily?Path.Combine(baseRoot,"modes","special"):baseRoot;if(!OpenRoot()){root=previousRoot;baseRoot=previousBase;}UpdateLocation();}}};save.Click+=(s,e)=>Save();
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
 void SelectFields(){
  var specialKeys=new[]{"frontGripPercent","rearGripPercent","rearStaticGripPercent","rearLateralGripPercent","speedLossPercent"};
  fields=allFields.Where(f=>f.section=="camera"||f.section==(specialFamily?"special":"drift")&&(!specialFamily||specialKeys.Contains(f.key))).Select(f=>new Field{key=f.key,section=f.section=="special"?"drift":f.section,ja=f.ja,en=f.en,min=f.min,max=specialFamily&&f.key=="yawAccelLimit"?6:f.max,value=f.value}).ToList();
 }
 void Translate(){
  int mi=mode.Items.Count==0?lastMode:Math.Max(0,mode.SelectedIndex);changing=true;family.Items.Clear();family.Items.AddRange(new object[]{T("アーケードモード","Arcade mode"),T("スペシャルモード","Special mode")});family.SelectedIndex=specialFamily?1:0;mode.Items.Clear();mode.Items.AddRange(new object[]{T("簡易モード","Simple"),T("アドバンス","Advanced")});mode.SelectedIndex=mi;
  languages.Items.Clear();languages.Items.AddRange(new object[]{T("言語: システム自動","Language: System"),"日本語","English"});languages.SelectedIndex=language=="ja"?1:language=="en"?2:0;
  if(targets.Items.Count>0)targets.Items[0]=T("デフォルト（新規車の基準）","Default (template for new cars)");changing=false;
  save.Text=T("変更を保存","Save changes");refresh.Text=T("再読込","Reload");browse.Text=T("場所を選ぶ","Browse");UpdateLocation();
 }
 bool OpenRoot(){
  try{
   if(!File.Exists(Path.Combine(root,"default_drift.json"))||!File.Exists(Path.Combine(root,"default_camera.json")))throw new IOException(T("default_drift.json と default_camera.json があるフォルダーを選んでください。","Choose a folder containing default_drift.json and default_camera.json."));
   var prefs=Path.Combine(baseRoot,"settings.json");if(File.Exists(prefs)){var p=Read(prefs);language=p.ContainsKey("language")?Convert.ToString(p["language"]):"auto";}
   Read(Path.Combine(root,"default_drift.json"));Read(Path.Combine(root,"default_camera.json"));
   changing=true;targets.Items.Clear();targets.Items.Add("Default");var vehicles=Path.Combine(root,"vehicles");if(Directory.Exists(vehicles))foreach(string d in Directory.GetDirectories(vehicles).OrderBy(x=>x)){
    if((File.GetAttributes(d)&FileAttributes.ReparsePoint)==0&&File.Exists(Path.Combine(d,"drift.json"))&&File.Exists(Path.Combine(d,"camera.json")))targets.Items.Add(Path.GetFileName(d));
   }
   targets.SelectedIndex=0;changing=false;Translate();LoadTarget();return true;
  }catch(Exception ex){changing=false;save.Enabled=false;Error(ex);return false;}
 }
 int ReadEditorMode(){
  var file=Path.Combine(Folder,"editor_ui.json");
  if(!File.Exists(file))return 0;
  try{var ui=Read(file);return ui.ContainsKey("mode")&&Convert.ToString(ui["mode"])=="advanced"?1:0;}
  catch{return 0;}
 }
 void LoadTarget(){
  string previous=active;int previousIndex=lastTarget,previousMode=lastMode;
  try{using(var guard=new FileStream(Path.Combine(baseRoot,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
   active=targets.SelectedIndex<=0?"":Convert.ToString(targets.SelectedItem);
   var d=Read(DriftFile);var c=Read(CameraFile);string dh=Hash(DriftFile),ch=Hash(CameraFile);
   drift=d;camera=c;driftHash=dh;cameraHash=ch;lastTarget=targets.SelectedIndex;
   lastMode=ReadEditorMode();changing=true;mode.SelectedIndex=lastMode;changing=false;
   dirty=false;save.Enabled=true;RenderTabs();UpdateLocation();
  }}catch(Exception ex){active=previous;lastMode=previousMode;changing=true;targets.SelectedIndex=previousIndex;mode.SelectedIndex=previousMode;changing=false;save.Enabled=false;Error(ex);UpdateLocation();}
 }
 void UpdateLocation(){
  string name=active==""?T("デフォルト","Default"):active;
  Text=(dirty?"* ":"")+"MW Arcade Drift Editor 0.2.0 — "+(specialFamily?T("スペシャルモード","Special mode"):T("アーケードモード","Arcade mode"))+" — "+name;
  location.Text=T("編集中: ","Editing: ")+name+"  /  "+(tabs.SelectedIndex==1?T("カメラ","Camera"):T("ドリフト","Drift"))+"\n"+(tabs.SelectedIndex==1?CameraFile:DriftFile);
  status.Text=(dirty?T("未保存の変更があります。\n","Unsaved changes.\n"):"")+T("保存後、ゲームで Ctrl+D で一周して対象モードへ戻して反映。\nデフォルトの変更は、作成済みの車種別設定を上書きしません。","After saving, cycle Ctrl+D back to the selected mode to apply.\nChanging defaults does not overwrite existing vehicle profiles.");
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
    panel.Controls.Add(new Label{Text=specialFamily?T("純正性能に対する割合。車種ごとの保存済み設定を表示します。","Percentages of original handling. Shows saved settings for this car."):T("50 = 配布時の標準値。保存済みの調整値を表示します。","50 = shipped defaults. Sliders show saved group values."),Width=760,Height=35});
    if(section=="drift"&&specialFamily){
     AddAdvanced(panel,drift,fields.First(f=>f.key=="frontGripPercent"));
     AddAdvanced(panel,drift,fields.First(f=>f.key=="rearGripPercent"));
     AddAdvanced(panel,drift,fields.First(f=>f.key=="rearStaticGripPercent"));
     AddAdvanced(panel,drift,fields.First(f=>f.key=="rearLateralGripPercent"));
     AddAdvanced(panel,drift,fields.First(f=>f.key=="speedLossPercent"));
     panel.Controls.Add(new Label{Width=760,Height=70,Text=T("前後グリップは純正タイヤ性能に対する割合です。小さいほど滑りやすくなります。\n速度減衰は横滑り中の空気抵抗です。100%で純正、0%で空気抵抗なし。\nタイヤ・ブレーキ・接触による減速は残ります。車種ごとに保存できます。", "Grip is a percentage of the original tyre values; lower values allow more slip.\nDrag applies while sliding: 100% is stock, 0% removes aerodynamic drag.\nTyre friction, braking and impact losses remain. Save separately for each car.")});
    }else if(section=="drift"){
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
  var row=new Panel{Width=780,Height=68,Name=f.key};var label=new Label{Text=Japanese?f.ja:f.en,Left=0,Top=0,Width=600,Height=24};var track=new TrackBar{Left=0,Top=24,Width=620,Height=40,Minimum=0,Maximum=1000,TickStyle=TickStyle.None};var number=new NumericUpDown{Left=635,Top=26,Width=120,DecimalPlaces=f.key=="speedLossPercent"?1:4,Minimum=(decimal)f.min,Maximum=(decimal)f.max,Increment=f.key=="speedLossPercent"?1m:.001m};
  double value=values.ContainsKey(f.key)?Convert.ToDouble(values[f.key],CultureInfo.InvariantCulture):f.value;value=Math.Max(f.min,Math.Min(f.max,value));bool busy=false;number.Value=(decimal)value;track.Value=(int)Math.Round((value-f.min)/(f.max-f.min)*1000);
  track.ValueChanged+=(s,e)=>{if(busy)return;busy=true;double n=f.min+(f.max-f.min)*track.Value/1000;number.Value=(decimal)n;values[f.key]=n;busy=false;Changed();};
  number.ValueChanged+=(s,e)=>{if(busy)return;busy=true;double n=(double)number.Value;track.Value=(int)Math.Round((n-f.min)/(f.max-f.min)*1000);values[f.key]=n;busy=false;Changed();};row.Controls.AddRange(new Control[]{label,track,number});panel.Controls.Add(row);
 }
 double SimpleValue(Field f,int position,bool inverse){
  double t=position/100.0;double factor=inverse?1.6-1.2*t:.7+.6*t;
  // Keep the combined special slip within the native validator's .65 rad cap.
  if(specialFamily&&(f.key=="baseSlipRad"||f.key=="slideSlipRad"||f.key=="handbrakeSlipRad"))factor=t<=.5?.5+t:1+(t-.5)*.48;
  if(f.key=="minDistance")factor=1.25-.5*t;
  return Math.Max(f.min,Math.Min(f.max,f.value*factor));
 }
 int SimplePosition(string[] names,bool inverse){
  // A group can contain individually edited fields. Find the slider position
  // that best represents the saved numeric values; clamped fields are harmless.
  double best=Double.PositiveInfinity;int selected=50;
  for(int position=0;position<=100;position++){
   double error=0;
   foreach(var key in names){
    var f=fields.First(x=>x.key==key);var source=f.section=="drift"?drift:camera;
    double current=source.ContainsKey(key)?Convert.ToDouble(source[key],CultureInfo.InvariantCulture):f.value;
    double scale=Math.Max(.01,Math.Abs(f.value));double difference=(current-SimpleValue(f,position,inverse))/scale;
    error+=difference*difference;
   }
   if(error<best-1e-12||(Math.Abs(error-best)<1e-12&&Math.Abs(position-50)<Math.Abs(selected-50))){best=error;selected=position;}
  }
  return selected;
 }
 void AddSimple(FlowLayoutPanel panel,string title,string[] names,bool inverse){
  int initial=SimplePosition(names,inverse);
  var row=new Panel{Width=780,Height=110};var label=new Label{Text=title,Width=660,Height=24};var val=new Label{Text=initial.ToString(),Left=700,Width=60};var track=new TrackBar{Top=28,Width=740,Minimum=0,Maximum=100,Value=initial,TickFrequency=10};
  var hint=new Label{Top=78,Width=755,Height=28,Text=String.Join(" / ",fields.Where(f=>names.Contains(f.key)).Select(f=>Japanese?f.ja:f.en).ToArray())};
  track.ValueChanged+=(s,e)=>{foreach(var key in names){var f=fields.First(x=>x.key==key);var target=f.section=="drift"?drift:camera;
    target[key]=SimpleValue(f,track.Value,inverse);
   }val.Text=track.Value.ToString();Changed();};row.Controls.AddRange(new Control[]{label,val,track,hint});panel.Controls.Add(row);
 }
 void SaveLanguage(){
  using(var guard=new FileStream(Path.Combine(baseRoot,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
   var p=Path.Combine(baseRoot,"settings.json");var settings=File.Exists(p)?Read(p):new Dictionary<string,object>{{"schemaVersion",2}};settings["language"]=language;Atomic(p,Pretty(settings));
  }
 }
 void SaveEditorMode(int selected){
  using(var guard=new FileStream(Path.Combine(baseRoot,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
   var ui=new Dictionary<string,object>{{"schemaVersion",1},{"mode",selected==1?"advanced":"simple"}};
   Atomic(Path.Combine(Folder,"editor_ui.json"),Pretty(ui));
  }
 }
 bool Save(){
  string stage=Path.Combine(Path.GetTempPath(),"MWArcadeDrift-"+Guid.NewGuid().ToString("N"));
  try{
   Directory.CreateDirectory(stage);File.WriteAllText(Path.Combine(stage,"drift.json"),Pretty(drift),new UTF8Encoding(false));File.WriteAllText(Path.Combine(stage,"camera.json"),Pretty(camera),new UTF8Encoding(false));
   var checker=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ConfigCheck.exe");var start=new ProcessStartInfo(checker,(specialFamily?"--special ":"")+"\""+stage+"\""){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true};
   using(var p=Process.Start(start)){string result=p.StandardOutput.ReadToEnd();p.WaitForExit();if(p.ExitCode!=0)throw new InvalidDataException(result);}
   using(var guard=new FileStream(Path.Combine(baseRoot,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
    if(Hash(DriftFile)!=driftHash||Hash(CameraFile)!=cameraHash)throw new IOException(T("別の操作でファイルが変更されました。再読込してください。","Files changed outside this editor. Reload first."));
    string backup=Path.Combine(root,"backups",DateTime.Now.ToString("yyyyMMdd-HHmmss-fff")+"-"+Guid.NewGuid().ToString("N").Substring(0,6));Directory.CreateDirectory(backup);File.WriteAllText(Path.Combine(backup,"target.txt"),DriftFile+"\n"+CameraFile,Encoding.UTF8);File.Copy(DriftFile,Path.Combine(backup,"drift.json"));File.Copy(CameraFile,Path.Combine(backup,"camera.json"));
    try{Atomic(DriftFile,Pretty(drift));Atomic(CameraFile,Pretty(camera));}catch{Atomic(DriftFile,File.ReadAllText(Path.Combine(backup,"drift.json")));Atomic(CameraFile,File.ReadAllText(Path.Combine(backup,"camera.json")));throw;}
   }driftHash=Hash(DriftFile);cameraHash=Hash(CameraFile);dirty=false;RenderTabs();status.Text=T("保存しました。ゲームで Ctrl+D で一周して対象モードへ戻してください。","Saved. Cycle Ctrl+D back to the selected mode in game.");return true;
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
  LoadTarget();if(targets.Items.Count>1){
   targets.SelectedIndex=1;Check(active!=""&&DriftFile.Contains("vehicles"),"vehicle selected separately");
   testModeDecision=DialogResult.No;mode.SelectedIndex=1;
   Check(mode.SelectedIndex==0&&!File.Exists(Path.Combine(Folder,"editor_ui.json")),"cancelled mode switch changes nothing");
   testModeDecision=DialogResult.Yes;mode.SelectedIndex=1;
   Check(mode.SelectedIndex==1&&Convert.ToString(Read(Path.Combine(Folder,"editor_ui.json"))["mode"])=="advanced","vehicle mode persisted");
   targets.SelectedIndex=0;Check(mode.SelectedIndex==0,"default retains separate mode");
   targets.SelectedIndex=1;Check(mode.SelectedIndex==1,"vehicle mode restored on selection");
   targets.SelectedIndex=0;
  }
  mode.SelectedIndex=0;RenderTabs();var driftPanel=(FlowLayoutPanel)tabs.TabPages[0].Controls[0];var cameraPanel=(FlowLayoutPanel)tabs.TabPages[1].Controls[0];
  Check(driftPanel.Controls.OfType<Panel>().Count()==3&&cameraPanel.Controls.OfType<Panel>().Count()==3,"three simple controls per screen");
  var firstTrack=driftPanel.Controls.OfType<Panel>().First().Controls.OfType<TrackBar>().First();double baseYaw=Convert.ToDouble(drift["targetYawMax"]);double cameraBefore=Convert.ToDouble(camera["orbitGain"]);firstTrack.Value=75;
  Check(Convert.ToDouble(drift["targetYawMax"])>baseYaw&&Convert.ToDouble(camera["orbitGain"])==cameraBefore,"drift group modifies only drift");
  Check(Save(),"simple group save succeeds");
  Check(((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().First().Controls.OfType<TrackBar>().First().Value==75,"saved simple slider stays at 75");
  LoadTarget();Check(((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().First().Controls.OfType<TrackBar>().First().Value==75,"reloaded simple slider stays at 75");
  mode.SelectedIndex=1;mode.SelectedIndex=0;
  driftPanel=(FlowLayoutPanel)tabs.TabPages[0].Controls[0];
  Check(driftPanel.Controls.OfType<Panel>().First().Controls.OfType<TrackBar>().First().Value==75,"mode switching preserves saved simple position");
  Check(Convert.ToString(Read(Path.Combine(root,"editor_ui.json"))["mode"])=="simple","default mode persisted separately");
  OpenRoot();Check(mode.SelectedIndex==0,"default mode restored after reopen");
  driftPanel=(FlowLayoutPanel)tabs.TabPages[0].Controls[0];
  Check(driftPanel.Controls.OfType<Panel>().Count()==3,"simple controls restored after reopen");
  var speedTrack=driftPanel.Controls.OfType<Panel>().ElementAt(2).Controls.OfType<TrackBar>().First();speedTrack.Value=100;
  Check(Save(),"clamped simple group saves");LoadTarget();
  Check(((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().ElementAt(2).Controls.OfType<TrackBar>().First().Value==100,"clamped group restores from its informative field");
  ((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().First().Controls.OfType<TrackBar>().First().Value=74;
  using(var guard=new FileStream(Path.Combine(baseRoot,"profiles.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){Check(!Save()&&dirty,"busy lock preserves edits");}
  dirty=false;LoadTarget();
  foreach(string lang in new[]{"ja","en"}){language=lang;Translate();foreach(int m in new[]{0,1}){mode.SelectedIndex=m;RenderTabs();Check(tabs.TabCount==2,"separate drift and camera tabs");if(m==1){Check(((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().Count()==(specialFamily?4:17),"all drift controls");Check(((FlowLayoutPanel)tabs.TabPages[1].Controls[0]).Controls.OfType<Panel>().Count()==20,"all camera controls");}}}
  dirty=false;var arcadePath=DriftFile;var arcadeBytes=File.ReadAllText(arcadePath);
  family.SelectedIndex=1;Check(specialFamily&&root==Path.Combine(baseRoot,"modes","special"),"special family selected independently");
  drift["baseSlipRad"]=.23;Changed();testDecision=DialogResult.Cancel;family.SelectedIndex=0;
  Check(specialFamily&&family.SelectedIndex==1&&dirty,"family cancel preserves special edits");
  testDecision=DialogResult.Yes;family.SelectedIndex=0;
  Check(!specialFamily&&Convert.ToDouble(Read(Path.Combine(baseRoot,"modes","special","default_drift.json"))["baseSlipRad"])==.23,"family switch saves special with native validation");
  Check(File.ReadAllText(arcadePath)==arcadeBytes,"special save cannot modify arcade profile");
  family.SelectedIndex=1;mode.SelectedIndex=1;RenderTabs();Check(((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().Count()==5,"all special controls");
  mode.SelectedIndex=0;RenderTabs();
  ((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().First().Controls.OfType<TrackBar>().First().Value=100;
  Check(Save(),"native front grip maximum saves");LoadTarget();
  Check(((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().First().Controls.OfType<TrackBar>().First().Value==100,"native front grip maximum restores");
  Check(File.ReadAllText(arcadePath)==arcadeBytes,"special simple save leaves arcade unchanged");
  var defaultSpecialPath=DriftFile;var defaultSpecialBytes=File.ReadAllText(defaultSpecialPath);
  var specialVehicle=Path.Combine(root,"vehicles","SPEED_TEST");Directory.CreateDirectory(specialVehicle);
  var olderProfile=Read(defaultSpecialPath);olderProfile.Remove("speedLossPercent");
  File.WriteAllText(Path.Combine(specialVehicle,"drift.json"),Pretty(olderProfile));File.Copy(CameraFile,Path.Combine(specialVehicle,"camera.json"));
  OpenRoot();targets.SelectedItem="SPEED_TEST";mode.SelectedIndex=0;RenderTabs();
  var speedRow=((FlowLayoutPanel)tabs.TabPages[0].Controls[0]).Controls.OfType<Panel>().Single(r=>r.Name=="speedLossPercent");
  var speedNumber=speedRow.Controls.OfType<NumericUpDown>().Single();
  Check(speedNumber.Value==50&&!dirty,"missing speed setting displays runtime default without modifying file");
  speedNumber.Value=73;Check(Save(),"per-car speed setting saves with native validation");LoadTarget();
  Check(Convert.ToDouble(drift["speedLossPercent"])==73,"per-car speed setting reloads");
  Check(File.ReadAllText(defaultSpecialPath)==defaultSpecialBytes&&File.ReadAllText(arcadePath)==arcadeBytes,"per-car speed preserves default and arcade settings");
  drift["speedLossPercent"]=101;Changed();Check(!Save()&&dirty,"out of range speed rejected without losing edits");
  dirty=false;LoadTarget();
  family.SelectedIndex=0;dirty=false;File.WriteAllText(Path.Combine(root,"editor-test-result.txt"),"PASS close cancel, target cancel, invalid save, external conflict, save, backup, discard, target selection, localized tabs, simple groups, all advanced fields, shared lock, special family isolation, family save/cancel, special simple maximum",Encoding.UTF8);
 }
 public void Preview(string folder){
  testing=true;testModeDecision=DialogResult.Yes;testDecision=DialogResult.No;
  Directory.CreateDirectory(folder);ShowInTaskbar=false;StartPosition=FormStartPosition.Manual;Location=new Point(-20000,-20000);Show();Application.DoEvents();
  foreach(int f in new[]{0,1}){family.SelectedIndex=f;foreach(string lang in new[]{"ja","en"})foreach(int m in new[]{0,1}){language=lang;Translate();mode.SelectedIndex=m;RenderTabs();for(int tab=0;tab<2;tab++){tabs.SelectedIndex=tab;Application.DoEvents();using(var b=new Bitmap(Width,Height)){DrawToBitmap(b,new Rectangle(0,0,Width,Height));b.Save(Path.Combine(folder,(f==1?"special-":"")+lang+"-"+m+"-"+tab+".png"));}}}}
  dirty=false;Close();
 }
 [STAThread]static void Main(string[] args){
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  string root=args.Length>0?args[0]:AppDomain.CurrentDomain.BaseDirectory;
  try{using(var form=new Editor(root)){if(args.Length>1&&args[1]=="--self-test")form.SelfTest();else if(args.Length>2&&args[1]=="--preview")form.Preview(args[2]);else Application.Run(form);}}
  catch(Exception ex){if(args.Length>1){File.WriteAllText(Path.Combine(root,"editor-error.txt"),ex.ToString());Environment.ExitCode=1;return;}MessageBox.Show(ex.Message,"MW Arcade Drift",MessageBoxButtons.OK,MessageBoxIcon.Error);Environment.ExitCode=1;}
 }
}

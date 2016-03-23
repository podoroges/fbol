//---------------------------------------------------------------------------

#include <vcl.h>
#include <conio.h>
#include <stdio.h>
#include <vector>


#include <..\ADBCommLib\Winet.h>
#include <..\ADBCommLib\comm2.h>


#pragma hdrstop

//---------------------------------------------------------------------------

#define B_WHITE   BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY


#pragma argsused


int WaitingForList = 1;
int AtLeastOneConnected = 0;


class CPair{
  public:
  int id;
  AnsiString st;
  CPair(int _id,AnsiString _st){
    id = _id;
    st = _st;
  }
};
class CPairs{
  public:
  std::vector <CPair> Pairs;
  void Add(CPair p){
    Pairs.push_back(p);
  }
  void Clear(){
    Pairs.clear();
  }
  int Exists(int id){
    for(unsigned int a=0;a<Pairs.size();a++)
      if(Pairs[a].id==id)
        return 1;
    return 0;
  }
  AnsiString GetText(int id){
    for(unsigned int a=0;a<Pairs.size();a++)
      if(Pairs[a].id==id)
        return Pairs[a].st;
    return "";
  }
};

CPairs Errors,Cfps;
AnsiString DisplayText;
AnsiString Code,User,Pass;


class CHorse:public TThread
{
  private:
  AnsiString Proxy,ProxyUser,ProxyPass;
  AnsiString ConvertASPCodes(AnsiString st)
  {
    st = StrRep(st,"+","%2B");
    st = StrRep(st,"/","%2F");
    st = StrRep(st,"=","%3D");
    return st;
  }

  public:
  __fastcall CHorse(AnsiString P,AnsiString U,AnsiString S): TThread(0){
    FreeOnTerminate = true;
    Proxy = P;
    ProxyUser = U;
    ProxyPass = S;
  }
  void __fastcall Execute(){
    WInet * wi = NULL;
    try{
      wi = new WInet(Proxy);
      wi->ProxyUser = ProxyUser;
      wi->ProxyPass = ProxyPass;
      printf("Thread %i: Connecting...\r\n",int(this));
      AnsiString st = wi->Get("https://www.flightops.sita.aero");
      if(!st.Pos("Sita Flight Briefing Online")){
        if(!AtLeastOneConnected)
          printf("Thread %i: Cannot connect to FBOL: %s\r\n",int(this),wi->LastError.c_str());
        delete wi;
        return;
      }
      printf("Thread %i: Connected. Logging in...\r\n",int(this));
      AtLeastOneConnected = 1;
      AnsiString vs,ev;
      vs = ExtractString1(st,"__VIEWSTATE",">");
      vs = ConvertASPCodes(ExtractString1(vs,"value=\"","\""));
      ev = ExtractString1(st,"__EVENTVALIDATION",">");
      ev = ConvertASPCodes(ExtractString1(ev,"value=\"","\""));
      wi->Referer = "https://www.flightops.sita.aero/";
      st = wi->Post("https://www.flightops.sita.aero/Default.aspx",(AnsiString)
        "__VIEWSTATE="+vs+"&__EVENTVALIDATION="+ev+"&tbCompanyCode="+Code+"&tbUserName="+User+"&tbPassword="+Pass+"&btnLogin=Log+In");
      if(!st.Pos("Valerie Kulbaka")){
        printf("Thread %i: Cannot login to FBOL: %s\r\n",int(this),st.c_str());
      }
      printf("Thread %i: Logged in. Obtaining data...\r\n",int(this));

      TStringList * str = new TStringList();

      wi->Referer = "https://www.flightops.sita.aero/Default.aspx";
      st = wi->Get("https://www.flightops.sita.aero/FlightPlanning/Search.aspx");

      while(st.Pos("PlanDetail.aspx?PlanId="))
      {
        int id = ExtractString1(st,"PlanDetail.aspx?PlanId=","'").ToIntDef(0);
        AnsiString st1 = ExtractString1(st,(AnsiString)"PlanDetail.aspx?PlanId="+id,"</tr>");
        AnsiString flt = ExtractString1(Parm(st1,0,"><"),">","<");
        AnsiString dep = ExtractString1(Parm(st1,2,"><"),">","<");
        AnsiString arr = ExtractString1(Parm(st1,3,"><"),">","<");
        AnsiString t1 = ExtractString1(Parm(st1,8,"><"),">","<");
        AnsiString t2 = ExtractString1(Parm(st1,9,"><"),">","<");
        str->Add((AnsiString)"*"+id+" "+t1+" ("+flt+" "+dep+"-"+arr+") "+t2);
        st = StrRemoveFirst(st,(AnsiString)"PlanDetail.aspx?PlanId="+id);
        if(!Cfps.Exists(id)){
          AnsiString CfpText = wi->Get((AnsiString)"https://www.flightops.sita.aero/FlightPlanning/ExportPlans.aspx?planId="+id);
          printf("Thread %i: %s\r\n",int(this),AnsiString((AnsiString)"*"+id+" "+t1+" ("+flt+" "+dep+"-"+arr+") "+t2).c_str());
          CfpText = (AnsiString)CfpText+"\n\nEurocontrol CFMU check:\n"
            +ExtractString1(wi->Post("http://validation.eurofpl.eu","freeEntry="+ExtractString1(CfpText,"START OF ICAO FLIGHT PLAN","END OF ICAO FLIGHT PLAN").Trim()),"<span class=\"ifpuv_result\">","</span>").Trim();
          Cfps.Add(CPair(id,CfpText));
        }
      }
      DisplayText = str->Text;

      delete str;
      WaitingForList = 0;
    }
    catch(Exception &e){
      printf("Thread %i: %s\r\n",int(this),AnsiString("Exception "+e.ClassName()+" "+e.Message).c_str());

    }
    delete wi;
  }
};



int main(int argc, char* argv[])
{
  // + Read credentials
  AnsiString fname = ChangeFileExt(Application->ExeName,".pwd");
  if(FileExists(fname)){
    TStringList * str = new TStringList();
    str->LoadFromFile(fname);
    str->Text = DecodeBase64(str->Text);
    Code = str->Values["Code"];
    User = str->Values["User"];
    Pass = str->Values["Pass"];
    delete str;
  }
  else{
    char input[0xFF];
    printf("Airline code: ");
    scanf("%s", &input);
    Code = input;
    printf("Username: ");
    scanf("%s", &input);
    User = input;
    printf("Password: ");
    scanf("%s", &input);
    Pass = input;
    TStringList * str = new TStringList();
    str->Values["Code"] = Code;
    str->Values["User"] = User;
    str->Values["Pass"] = Pass;
    str->Text = EncodeBase64(str->Text);
    str->SaveToFile(fname);
    delete str;
  }
  // - Read credentials


  printf("Connecting to FBOL. Press ESC to abort.\r\n");
  //new CHorse("proxy:port","domain\login","password");

  new CHorse("","","");

  while(WaitingForList){
    if(kbhit()){
      char c = getch();
      if(c==27)return 1;
    }
  }

  system("cls");
  _setcursortype(_NOCURSOR);



  textcolor(B_WHITE);




  TStringList * str = new TStringList();
  str->Text = DisplayText;
  int line = 0;
  int run = 1;
  while(run){
    gotoxy(1,1);
    cprintf("       *** SITA FBOL *** by Pavel Krents (c) 2015 ***       \r\n");
    for(int a=0;a<str->Count;a++){
      if(a==line)
        cprintf("%s\r\n",str->Strings[a].c_str());
      else
        printf("%s\r\n",str->Strings[a].c_str());
    }
    cprintf(" %3i CFP(s) total. Press ESC to exit. Press Enter to export.",str->Count);



    while(!kbhit());
    if(kbhit()){
      char c = getch();
      if(c==27)run = 0;
      if((c==72)&&(line>0))line--;
      if((c==80)&&(line<str->Count-1))line++;
      if(c==13){
        if(str->Count>0){
          AnsiString st = str->Strings[line];
          int cid = ExtractString1(st,"*"," ").ToIntDef(0);
          if(cid>0){
            char temp[MAX_PATH];
            GetTempPath(MAX_PATH,temp);
            TStringList * str = new TStringList();
            str->Text = Cfps.GetText(cid);
            AnsiString fname = (AnsiString)temp+ExtractString1(st,"(",")")+".txt";
            str->SaveToFile(fname);
            delete str;
            ShellExecuteA(NULL,"open",fname.c_str(),NULL,NULL,SW_SHOW);
          }
        }
      }
      if((c==77)||(c==109)){
        if(str->Count>0){
          AnsiString st = str->Strings[line];
          int cid = ExtractString1(st,"*"," ").ToIntDef(0);
          if(cid>0){
            char temp[MAX_PATH];
            GetTempPath(MAX_PATH,temp);
            TStringList * str = new TStringList();
            AnsiString Icao = ExtractString1(Cfps.GetText(cid),"START OF ICAO FLIGHT PLAN","END OF ICAO FLIGHT PLAN").Trim();
            AnsiString Dep = Parm(Icao,5,"-").SubString(1,4);
            AnsiString Arr = Parm(Icao,7,"-").SubString(1,4);
            AnsiString TAlt = ExtractString1(Icao,"TALT/"," ");
            AnsiString DAlt = Parm(Icao,7,"-").SubString(9,100).Trim();
            str->Text = (AnsiString)Dep+" "+Arr+" "+TAlt+" "+DAlt;
            AnsiString fname = (AnsiString)temp+ExtractString1(st,"(",")")+"_ap.txt";
            str->SaveToFile(fname);
            delete str;
            ShellExecuteA(NULL,"open",fname.c_str(),NULL,NULL,SW_SHOW);
          }
        }
      }

    }
  }


  delete str;

  return 0;
}
//---------------------------------------------------------------------------

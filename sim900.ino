#include <SoftwareSerial.h>
#include <EEPROM.h>

#include <LiquidCrystal.h>
 
LiquidCrystal lcd(4, 5, 10, 11, 12, 13);


byte hasRTC=1;   // do we have RTC?
String std_del=" \r\n\t"; // std delimiters
String phoneCmd; // commandPhone


SoftwareSerial gprsSerial(7, 8);
int bOK, bERROR;
String OutBuf,OutFilter;
int modemRun(int msec); // returns >0 if OK detected, -1 on ERRORS

void out(String t) {
  Serial.println(t);
}

int ready=0;

int checkReady() {
   if (ready) return 1;
   char cO,cK;
            gprsSerial.print("AT\r");
            delay(10);
            cO = gprsSerial.read();
            cK = gprsSerial.read();
   ready =  (cO == 'O' || cK == 'K');
   if (ready) { // clear data
      while (gprsSerial.available()) {
         cK = gprsSerial.read();
         }
      }
   //Serial.println(c0);
   return ready;
}

void modemReset() {
pinMode(9,OUTPUT);  digitalWrite(9,HIGH);
delay(1500); digitalWrite(9,LOW); delay(500);
}

int modemAt(String cmd) {
gprsSerial.print("at"+cmd+"\r");
modemRun(5000);
return bOK>0;
}

int getInt(String &s) {
char arr[10]; 
get_word2(s," /\",:").toCharArray(arr,9); arr[9]=0;
return atoi((char*)arr);
}

String Int2(int D) { String r=String(D); if (r.length()==1) r="0"+r; return r;}

byte Day,Month,Year,Hour,Min,Sec;
  

int modemDate() { // ReadDateTime info
int ok;
OutBuf=""; OutFilter="+CCLK:";
ok = modemAt("+cclk?");
if (ok>0) {
     Year=getInt(OutBuf); Month=getInt(OutBuf); Day=getInt(OutBuf);
     Hour=getInt(OutBuf); Min=getInt(OutBuf);   Sec=getInt(OutBuf); 
       out(" Time: "+Int2(Hour)+":"+Int2(Min)+":"+Int2(Sec)+"  "+Int2(Day)+"."+Int2(Month)+".20"+Int2(Year));
     }
OutBuf=0; OutFilter="";
return ok;
}

int checkReady2() {
if (modemAt("")>0) ready=1;
return ready;
}

void strWrite(String str, int addr, int len) {
int i; 
for(i=0;i<str.length() && i<len; i++,addr++) {
  byte ch = str.charAt(i);
   EEPROM.write(addr,ch);
  }
 for(i=0;i<len;i++,addr++) EEPROM.write(addr,0);
}

String strRead(int addr,int len) {
int i; char ch; String res="";
  for(i=0;i<len;i++,addr++)  {
    ch=EEPROM.read(addr);
    if (ch==0) break;
    res+=ch;
  }
return res;
}

void setup()  {
     gprsSerial.begin(9600);
     Serial.begin(9600);
     
     lcd.begin(20, 4);
     
     checkReady2();
     if (!ready) modemReset();
     
     out("!waiting modem for ready");

      while (!ready) { 
         checkReady2();
         if (ready) break;
         Serial.print("*");
         delay(1000);
         }
       out("\n!done modem here");

        modemAt("+CMGF=1");
        modemAt("+IFC=1, 1");
        modemAt("+CNMI=1,2,2,1,0");
        
        phoneCmd=strRead(0,24); // command phone
        
       out("!init done, phoneCmd="+phoneCmd);
}

int sendSMS(String phone,String text) {
bOK=bERROR=0; OutBuf=""; // start run
gprsSerial.print("at+cmgs=\""+phone+"\"\r");
modemRun(1500);
gprsSerial.println(text);
modemRun(500);
gprsSerial.print((char)26); gprsSerial.print((char)13); // ctrl Z \r
bOK=bERROR=0;
modemRun(10000);
if (bOK>0) return 1;
return 0;
}



void ltrim2(String &buf,String del) {
while(buf.length()>0) { // ltrim
    char ch=buf.charAt(0);
    if (del.indexOf(ch)>=0) buf=buf.substring(1);
      else break;
    }  
}

void ltrim(String &buf) { return ltrim2(buf,std_del); }

int lcmp(String &buf,String name) {
 ltrim(buf);
if (buf.indexOf(name)==0) {
     buf=buf.substring(name.length());
     ltrim(buf);
     return 1;
     }
     
return 0;
} 


String get_word2(String &buf,String del) {
ltrim2(buf,del); int i;
for(i=0;i<buf.length();i++) {
   char ch=buf.charAt(i);
   if (del.indexOf(ch)>=0) break; // delimiter
   }
String res=buf.substring(0,i);
buf=buf.substring(i);
ltrim2(buf,del);
return res;   
}
String get_word(String &buf) { return get_word2(buf,std_del); }

    
void pushStr(String cmd) {
 //Serial.println("CMD=<"+cmd+">"); 
if (lcmp(cmd,"test")) {
  out(" test command here:"+cmd);
  if (lcmp(cmd,"sms")) {
     sendSMS(phoneCmd,"Just a test");
     }
  return;
  }
if (lcmp(cmd,"date")) {
  modemDate();
  return ;
  }
if (lcmp(cmd,"phoneCmd")) {
  phoneCmd=cmd;
  strWrite(phoneCmd,0,24);
  out("+new phoneCmd="+phoneCmd);
  return ;
  }
else if (lcmp(cmd,"sms")) {
  String phone =get_word(cmd); // need get first word
  int code = sendSMS(phone,cmd); // MUST RETURN +CMGS: on OK
  if (code>0) out("+ok_sms_send phone:"+phone+",text:"+cmd);
      else out("-error send "+code);
  return ;  
  }
else gprsSerial.println(cmd);
}
    
    
String line="";
void pushIn(char in) { // push input char for processing
int code = in;
//static String line;
if (code==10) return ; // ignore \n
if (code==13) { // flash collected line
   pushStr(line);
   line="";
   return;
   }
line=line+in;
}

int    fSMS=0;  // no SMS yet
String smsSender="";

void onNewSms(String phone,String Text) {
out("smsCommandHere:{"+Text+"} from {"+phone+"}"); // skip?
lcd.setCursor(0,1); lcd.print(Text+"  from:"+phone);
}

void pushOutStr(String str) {
String s=str;
if (str == "OK") bOK++;
else if (str == "ERROR") bERROR++;
 else if (lcmp(str,"+CMT:")) { // new incoming SMS
    // rest is "NUM",,"Date"
    lcmp(str,"\"");  int p=str.indexOf("\""); smsSender="";
    if (p>0) smsSender=str.substring(0,p);
   out("newSMS here, sender:"+smsSender+"now - wait for a text");
   fSMS=1; // here, and wait for a text
   } 
 else if (fSMS==1) {
   fSMS=0;
   onNewSms(smsSender,str);
   }
 else if (lcmp(s,OutFilter)) {
    if (OutBuf.length()>0) OutBuf=OutBuf+"\n";
    OutBuf+=s; // just collect in OutFIlter
    }
Serial.println("<"+str+">");
}

String outStr="";
void pushOut(char ch) { // push input char for processing
int code = ch;
//static String line;
if (code==13) return ; // ignore \n
if (code==10) { // flash collected line
   pushOutStr(outStr);
   outStr="";
   return;
   }
outStr=outStr+ch;
}

int modemRun(int msec) {
bOK=0; bERROR=0;
while(msec>=0) {
        if (gprsSerial.available()) {
            char currSymb = gprsSerial.read();
            pushOut(currSymb);
            Serial.print(currSymb);
            }
   delay(1); msec--;
   if (bOK) return 1;
   if (bERROR) return -1;
}
return 0;
}


void everySecond() {
modemDate();
 String t=Int2(Hour)+":"+Int2(Min)+":"+Int2(Sec)+"  "+Int2(Day)+"."+Int2(Month)+".20"+Int2(Year);
 lcd.setCursor(0,0); lcd.print(t);
}

int code=0;
void loop() {
 
        modemRun(0);
        if (Serial.available()) pushIn( Serial.read() );
        delay(10); code++;
        if (code>800) {
           code=0;
           everySecond();
           }
        
       
}


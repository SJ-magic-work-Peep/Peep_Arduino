/************************************************************
■description
	This is a sample code for testing : send & receive udp message : Arduino <-> oF
		oF	: key 0-9をpressすると、"Button|KeyId"をUDPで送ってくる。
		Ard	: UDPを受信し、"Button[p]KeyId[p]Nth_Message"の形に変形してoFに送り返す。

■参考
	■イーサーネットシールド２
		https://ht-deko.com/arduino/shield_ethernet2.html
		
	■Ethernet - Ethernet.begin()
		https://www.arduino.cc/reference/en/libraries/ethernet/ethernet.begin/
		
	■Ethernet.begin()
		http://www.musashinodenpa.com/arduino/ref/index.php?f=1&pos=1167
		
	■EthernetUDP
		https://garretlab.web.fc2.com/arduino_reference/libraries/standard_libraries/Ethernet/EthernetUDP/
		
		
■グローバル/ プライベート アドレス
	■【図解/初心者向け】IPアドレスの仕組みと種類 ~クラス, グローバルとプライベート, 例示用など特殊用途~
		https://milestone-of-se.nesuke.com/nw-basic/ip/ip-address/
		
	■IP アドレスのクラス、グローバル IP アドレス、プライベート IP アドレス
		https://www.keicode.com/netprimer/np13.php
		
	■プライベートIPアドレス
		https://atmarkit.itmedia.co.jp/aig/06network/privateip.html
	
	■レイヤ２（L2）スイッチとレイヤ３（L3）スイッチ、ルーターとの違いは？
		https://community.fs.com/jp/blog/layer-2-switch-vs-layer-3-switch-what-is-the-difference.html
		
	■サブネットマスクとは？
		https://www.cman.jp/network/term/subnet/
		
	■ブロードキャストアドレス 【broadcast address】
		https://e-words.jp/w/%E3%83%96%E3%83%AD%E3%83%BC%E3%83%89%E3%82%AD%E3%83%A3%E3%82%B9%E3%83%88%E3%82%A2%E3%83%89%E3%83%AC%E3%82%B9.html
		
	■ネットワークアドレスとブロードキャストアドレスって何だろう？（意味と使い方を解説）
		https://itmanabi.com/netword-broadcast-address/
		
	■「サブネットマスクが正しくないと通信できない」は本当か？
		https://ascii.jp/elem/000/000/562/562310/

■Arduino, mac, 共にclassAでIPをsetする際に、どうすれば通信できるか、調査/検討した。
	classA(10.0.0.0/8 = 10.0.0.0 - 10.255.255.255, subnet mask = 255.0.0.0)の場合、前8bitがグループアドレス、後 24bitがホストアドレス。
		■IPアドレスとサブネットマスクをまとめて表記する
			https://atmarkit.itmedia.co.jp/ait/articles/0701/06/news014.html
	
	classAで通信するには、Arduino側で、subnet mask = 255.0.0.0を明示的に指定する必要がある。
	ここで、ファイル/ スケッチ例/ Ethernet2/ AdvancedChatServerに
		Ethernet.begin(mac, ip, gateway, subnet);
	の例があったが、★★★ これは間違い!!!! ★★★
	この間違いに気付かず、
		mac(10.0.0.5) - Arduino(10.0.1.10)
	で通信できなくて、はまってしまった。
	
	■Ethernet.begin()
		http://www.musashinodenpa.com/arduino/ref/index.php?f=1&pos=1167
	にある通り、
		Ethernet.begin(mac); // DHCPで設定してbegin. 戻り値 == 0の時は、DHCP設定に失敗しているので、明示的に指定してbeginする必要あり。
		Ethernet.begin(mac, ip);
		Ethernet.begin(mac, ip, dns);
		Ethernet.begin(mac, ip, dns, gateway);
		Ethernet.begin(mac, ip, dns, gateway, subnet);
		
		gatewayのデフォルトはデバイスのIPアドレスの最終オクテットを1にしたもの、subnetのデフォルトは255.255.255.0です。
		
	と言う形で、subnet maskを指定する必要があり(sample codeは引数を間違っていた)、subnetのデフォルトは255.255.255.0 となっている。
	
	上記の通り設定すると、めでたく、
		mac(10.0.0.5) - Arduino(10.0.1.10)
	でも通信が通った。もちろん、他のHost AddressもOK.
	
	これで、好きなだけArdino deviceを接続できるので安心。
************************************************************/
#include <SPI.h>			// needed for Arduino versions later than 0018
#include <Ethernet2.h>		//
#include <EthernetUdp2.h> 	// UDP library from: bjoern@cs.stanford.edu 12/30/2008

#include <Servo.h>

/************************************************************
************************************************************/
/********************
Arduino
********************/
// byte MyMacAddress[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x27, 0x26 };	// RotStage
// byte MyMacAddress[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x75, 0xA9 };	// PrjShutter
byte MyMacAddress[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x88, 0x12 };	// Table

IPAddress MyIP(10, 0, 0, 7);
IPAddress dnsServer(10, 0, 0, 1);	// 1st of Host Address. : 実際には、ここに何もいないが : 名前解決を使わないので、temporaryで設定しておけばOK。
IPAddress gateway(10, 0, 0, 1);		// 1st of Host Address. : 実際には、ここに何もいないが : グループアドレスを出ないので、temporaryで設定しておけばOK。
IPAddress subnet(255, 0, 0, 0);
unsigned int ReceivePort = 12345;

/********************
mac
********************/
IPAddress IP_SendTo(10, 0, 0, 5);	// Arduino send udp message to this IP(that of mac).
unsigned int SendPort = 12346;		// Arduino send udp message to this Port(that of mac).

char Buf_for_Receive[UDP_TX_PACKET_MAX_SIZE/* 24 */];	// Buf for receiving message from mac.

EthernetUDP Udp; // An EthernetUDP instance to let us send and receive packets over UDP

enum{
	MAX_UDP_MSGS = 5, // separatorでsplitした結果の数
};
int c_ReceivedUdpMessages = 0;

/********************
********************/
String str_Command = "";
int udp_Param[3] = {0, 0, 0}; // 3 = MAX_UDP_MSGS - 2 (from==oF|command)
enum STATE{
	STATE__WAIT_COMMAND,
	STATE__PUSHING,
};
STATE State = STATE__WAIT_COMMAND;

/********************
Don´t use : DP : 4, 10, 11, 12, 13 : used by ethernet shield
********************/
int counter = 0;
int SERVO_SOCKET = 9;
Servo servo;

unsigned long t_From = 0;
int angle_Release = 90;
int PushTime = 500;

/************************************************************
************************************************************/
int SplitString(String src, char separater, String *dst, int MaxMessages);
void send_udp_Beat_to_oF();
void operation__SetAngle(int angle);
void operation__SetAngle_Release();


/************************************************************
************************************************************/
/******************************
******************************/
void setup() {
	/********************
	注意!!!
		ファイル/ スケッチ例/ Ethernet2/ AdvancedChatServerに
			Ethernet.begin(mac, ip, gateway, subnet);
		の例があったが、これは間違い!!!!
		
		■Ethernet.begin()
			http://www.musashinodenpa.com/arduino/ref/index.php?f=1&pos=1167
			https://www.arduino.cc/reference/en/libraries/ethernet/ethernet.begin/
		にある通り、
			Ethernet.begin(mac); // DHCPで設定してbegin. 戻り値 == 0の時は、DHCP設定に失敗しているので、明示的に指定してbeginする必要あり。
			Ethernet.begin(mac, ip);
			Ethernet.begin(mac, ip, dns);
			Ethernet.begin(mac, ip, dns, gateway);
			Ethernet.begin(mac, ip, dns, gateway, subnet);
		であり、subnetのデフォルトは255.255.255.0 となっている。
	********************/
	// Ethernet.begin(MyMacAddress, MyIP);
	Ethernet.begin(MyMacAddress, MyIP, dnsServer, gateway, subnet);
	
	/********************
	********************/
	Udp.begin(ReceivePort);
	
	/********************
	********************/
	servo.attach(SERVO_SOCKET);
	operation__SetAngle_Release();
	
	/********************
	********************/
	Serial.begin(9600);
	
	/********************
	********************/
	Serial.println("> Started");
	Serial.print("> UDP_TX_PACKET_MAX_SIZE = ");
	Serial.println(UDP_TX_PACKET_MAX_SIZE);	// Result = 24.
	
	Serial.print("my IP : ");
	Serial.println(Ethernet.localIP());
}

/******************************
description:
	全messageを処理する
******************************/
void loop() {
	/********************
	********************/
	str_Command = "None";
	
	unsigned long now = millis(); // 50 daysでoverflow.
	int packetSize = Udp.parsePacket();
	
	do{
		if (packetSize){
			Serial.println(">------------------------------");
			
			/********************
			■Arduino/IPAddress.h at master - esp8266 - GitHub
				https://github.com/esp8266/Arduino/blob/master/cores/esp8266/IPAddress.h
			********************/
			Serial.print("> Received packet of size ");
			Serial.println(packetSize);
			Serial.print("From ");
			IPAddress remote = Udp.remoteIP();
			for (int i = 0; i < 4; i++){
				Serial.print(remote[i], DEC); // Serial.print(remote[i], HEX);
				if(i < 3) Serial.print(".");
			}
			Serial.print(", port ");
			Serial.println(Udp.remotePort());
			
			/********************
			Udp.read 前に掃除しないと、前回のdataがゴミとして残っていた
			********************/
			memset(Buf_for_Receive, '\0', sizeof(Buf_for_Receive));
			/*
			for(int i = 0; i < UDP_TX_PACKET_MAX_SIZE; i++){
				Buf_for_Receive[i] = '\0';
			}
			*/
			
			Udp.read(Buf_for_Receive, UDP_TX_PACKET_MAX_SIZE);
			Serial.print("Contents = ");
			Serial.println(Buf_for_Receive);
			
			/********************
			********************/
			String str_Message = Buf_for_Receive;
			String UdpMsgs[MAX_UDP_MSGS];
			for(int i = 0; i < MAX_UDP_MSGS; i++){
				UdpMsgs[i] = ""; // 一応、ちゃんと初期化.
			}
			
			SplitString(str_Message, '|', UdpMsgs, MAX_UDP_MSGS);
			
			if(UdpMsgs[0] == "oF"){
				str_Command = UdpMsgs[1];
				udp_Param[0] = atoi(UdpMsgs[2].c_str());
				udp_Param[1] = atoi(UdpMsgs[3].c_str());
			}
			
			packetSize = Udp.parsePacket();
		}
		
		StateChart();
		
	}while(packetSize);
	
	/********************
	********************/
	send_udp_Beat_to_oF();
	
	/********************
	********************/
	delay(30);
}

/******************************
description
	簡単のため、separaterは、一文字のみの対応となります。
	ofSplitString()より不便ですが、上手く使いこなしてください。
	
return
	Num of splitted messages.
	
	実際は、"separatorの数 + 1". 
	separator間、或いは最後のseparator後にmessageが入ってないことも考えられるが、この時は、「空のmessageが入っていた」とcountすることとする。
******************************/
int SplitString(String src, char separater, String *dst, int MaxMessages){
	/********************
	********************/
	for(int i = 0; i < MaxMessages; i++){
		dst[i] = ""; // 初期化
	}
	
	/********************
	********************/
	int index = 0;
	int src_length = src.length();
	if(src_length <= 0) return 0;
	
	/********************
	********************/
	for (int i = 0; i < src_length; i++) {
		char tmp = src.charAt(i);
		if ( tmp == separater ) {
			// dst[index] += '\0'; // 気を効かしたつもりで、これをやるとNG. Stringはzero終端文字列ではなく、途中に'\0'を含むこともできる。
			
			index++;
			if(MaxMessages <= index) return MaxMessages; // range over. maxまで格納してreturn.
		}else{
			dst[index] += tmp;
		}
	}
	// dst[index] += '\0'; // 気を効かしたつもりで、これをやるとNG. Stringはzero終端文字列ではなく、途中に'\0'を含むこともできる。
	
	/********************
	********************/
	return index + 1;
}

/******************************
******************************/
void StateChart(){
	switch(State){
		case STATE__WAIT_COMMAND:
			if(str_Command == "set_dAngle"){
				operation__SetAngle(angle_Release + udp_Param[0]);
				PushTime = udp_Param[1];
				
				t_From = millis();
				State = STATE__PUSHING;
			}
			break;
			
		case STATE__PUSHING:
			if(PushTime < millis() - t_From){
				operation__SetAngle_Release();
				State = STATE__WAIT_COMMAND;
			}
			break;
	}
}

/******************************
******************************/
void send_udp_Beat_to_oF(){
	Udp.beginPacket(IP_SendTo, SendPort);
		String str_for_Send = "Table[p]Beat";
		Udp.write(str_for_Send.c_str());
	Udp.endPacket();
}

/******************************
******************************/
void operation__SetAngle_Release(){
	operation__SetAngle(angle_Release);
}

/******************************
******************************/
void operation__SetAngle(int angle){
	if( (angle < 0) || (180 < angle) )	return;
	
	servo.write(angle);	
}

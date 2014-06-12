#define LED 13

// 13pinのオンボードLEDを使う
long randNumber;
// long型で宣言

void setup() {
pinMode(LED, OUTPUT);
// LEDは出力で利用
}

void loop(){
randNumber = random(1000);
// 0~1000までの乱数を計算


 
digitalWrite(LED, HIGH);
delay(randNumber);
// LEDをrandNumberミリ秒間光らせます

digitalWrite(LED, LOW);
delay(1000);
// LEDを1000ミリ秒間消灯させます。
}

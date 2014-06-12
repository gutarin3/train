float x;
float y;
float z;

float a;
float aa;

float b;
float bb;

float c;
float cc;

float d;
float dd;

float e;
float ee;

//最初に1回だけ実行する
void setup() {
  size(300, 300);
  x = width/2;
  z = width/2;
  y = 0;
  a = random(width/2);
  aa = random(height/2);
  b = random(width/2);
  bb = random(height/2);
  c = random(width/2);
  cc = random(height/2);
  d = random(width/2);
  dd = random(height/2);
  e = random(width/2);
  ee = random(height/2);
}

//毎フレーム繰り返し実行する
void draw() {
  background(255);
  y += 1;
  //y += random(1.8);
  if (y > height) {
     y = random(-10);
     x = random(width - 10);
     z = random(width - 10);
     a = random(width - 10);
     aa = random(height -10);
     b = random(width - 10);
     bb = random(height -10);
     c = random(width - 10);
     cc = random(height -10);
     d = random(width - 10);
     dd = random(height -10);
     e = random(width - 10);
     ee = random(height -10);
  }
 colorMode(RGB,256);
 smooth();
 stroke(random(255),random(255),random(255));
  //for (int i=0; i < 255; i++){
    ellipse(x, y, 10, 10); //画面中央左に一辺が10pxの正方形を描く
//    ellipse(x-20, y-20, 10, 10); //画面中央左に一辺が10pxの正方形を描く
    //ellipse(z ,y - 20, 10, 10);
 stroke(random(255),random(255),random(255));
    ellipse(a ,y - aa, 10, 10);
 stroke(random(255),random(255),random(255));
    ellipse(b ,y - bb, 10, 10);
 stroke(random(255),random(255),random(255));
    ellipse(c ,y - cc, 10, 10);
 stroke(random(255),random(255),random(255));
    ellipse(d ,y - dd, 10, 10);
 stroke(random(255),random(255),random(255));
    ellipse(e ,y - ee, 10, 10);
    //ellipse(y, y-20, 10, 10); //画面中央左に一辺が10pxの正方形を描く  
  //}
}
